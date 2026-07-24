/*
 * ESP32-S3 N16R8 + HLK-LD2410S
 * 人体状态分析：无人 / 有人活动 / 有人静止
 *
 * 接线：
 * LD2410S 3V3 -> ESP32-S3 3V3
 * LD2410S GND -> ESP32-S3 GND
 * LD2410S OT1(TX) -> ESP32-S3 GPIO16(RX)
 * LD2410S RX      -> ESP32-S3 GPIO17(TX)
 * LD2410S OT2     -> 不接（可选的人体存在数字输出）
 *
 * Arduino IDE：
 * 开发板建议选择 ESP32S3 Dev Module
 * USB CDC On Boot: Enabled（若串口监视器无输出）
 * 串口监视器波特率：115200
 */

#include <Arduino.h>

// 接线
static constexpr int RADAR_RX_PIN = 16;  // ESP32 接收，引脚连接雷达 OT1/TX
static constexpr int RADAR_TX_PIN = 17;  // ESP32 发送，引脚连接雷达 RX
static constexpr uint32_t RADAR_BAUD = 115200;

// 雷达串口使用 UART1；USB/下载串口继续使用 Serial
HardwareSerial RadarSerial(1);

// 人体状态参数
static constexpr uint32_t DATA_TIMEOUT_MS = 2500;       // 超过该时间没收到数据，判定通信异常/无人
static constexpr uint32_t STILL_CONFIRM_MS = 1800;      // 持续多久无明显距离变化后判定静止
static constexpr uint32_t MOVING_HOLD_MS = 1000;        // 检测到运动后保持“活动”的时间
static constexpr uint16_t MOTION_DELTA_CM = 6;          // 距离变化超过该值，认为发生运动
static constexpr uint32_t PRINT_INTERVAL_MS = 250;      // 串口输出周期

enum class HumanState : uint8_t {
  NO_DATA,
  NOBODY,
  MOVING,
  STILL
};

struct RadarData {
  bool valid = false;
  bool present = false;
  uint8_t rawTargetState = 0;
  uint16_t distanceCm = 0;
  uint32_t lastFrameMs = 0;
};

RadarData radar;
HumanState humanState = HumanState::NO_DATA;

uint16_t filteredDistanceCm = 0;
uint16_t previousFilteredDistanceCm = 0;
uint32_t lastMotionMs = 0;
uint32_t presenceStartMs = 0;
uint32_t lastPrintMs = 0;
bool previousPresence = false;

// LD2410S 配置指令

static const uint8_t CMD_ENTER_CONFIG[] = {
  0xFD, 0xFC, 0xFB, 0xFA,
  0x04, 0x00,
  0xFF, 0x00, 0x01, 0x00,
  0x04, 0x03, 0x02, 0x01
};

static const uint8_t CMD_MINIMAL_MODE[] = {
  0xFD, 0xFC, 0xFB, 0xFA,
  0x08, 0x00,
  0x7A, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x04, 0x03, 0x02, 0x01
};

static const uint8_t CMD_EXIT_CONFIG[] = {
  0xFD, 0xFC, 0xFB, 0xFA,
  0x02, 0x00,
  0xFE, 0x00,
  0x04, 0x03, 0x02, 0x01
};

void clearRadarSerialBuffer() {
  while (RadarSerial.available() > 0) {
    RadarSerial.read();
  }
}

void sendRadarCommand(const uint8_t *command, size_t length) {
  RadarSerial.write(command, length);
  RadarSerial.flush();
}

// 强制切回默认的“极简数据模式”：6E 状态 距离低字节 距离高字节 62
void setRadarMinimalMode() {
  clearRadarSerialBuffer();

  sendRadarCommand(CMD_ENTER_CONFIG, sizeof(CMD_ENTER_CONFIG));
  delay(120);
  clearRadarSerialBuffer();

  sendRadarCommand(CMD_MINIMAL_MODE, sizeof(CMD_MINIMAL_MODE));
  delay(120);
  clearRadarSerialBuffer();

  sendRadarCommand(CMD_EXIT_CONFIG, sizeof(CMD_EXIT_CONFIG));
  delay(150);
  clearRadarSerialBuffer();
}

// 数据解析_极简数据帧固定为 5 字节：
// [0] 0x6E
// [1] 目标状态：0/1 无人，2/3 有人
// [2] 距离低字节
// [3] 距离高字节
// [4] 0x62

void processMinimalFrame(const uint8_t frame[5]) {
  if (frame[0] != 0x6E || frame[4] != 0x62) {
    return;
  }

  const uint8_t rawState = frame[1];
  if (rawState > 3) {
    return;
  }

  const uint16_t distance =
      static_cast<uint16_t>(frame[2]) |
      (static_cast<uint16_t>(frame[3]) << 8);

  radar.valid = true;
  radar.rawTargetState = rawState;
  radar.present = (rawState == 2 || rawState == 3);
  radar.distanceCm = distance;
  radar.lastFrameMs = millis();
}

void readRadarFrames() {
  static uint8_t frame[5];
  static uint8_t position = 0;

  while (RadarSerial.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(RadarSerial.read());

    // 等待帧头
    if (position == 0) {
      if (value == 0x6E) {
        frame[position++] = value;
      }
      continue;
    }

    frame[position++] = value;

    if (position == sizeof(frame)) {
      if (frame[4] == 0x62) {
        processMinimalFrame(frame);
        position = 0;
      } else {
        // 帧尾错误，尝试把当前字节作为下一帧帧头
        position = (value == 0x6E) ? 1 : 0;
        if (position == 1) {
          frame[0] = 0x6E;
        }
      }
    }
  }
}

// 人体状态判断
// 用LD2410S给出的“是否有人”和距离，判断连续距离变化，把“有人”进一步分为活动/静止。

uint16_t absoluteDifference(uint16_t a, uint16_t b) {
  return (a >= b) ? (a - b) : (b - a);
}

void updateHumanState() {
  const uint32_t now = millis();

  // 长时间没收到合法数据
  if (!radar.valid || (now - radar.lastFrameMs > DATA_TIMEOUT_MS)) {
    humanState = HumanState::NO_DATA;
    previousPresence = false;
    return;
  }

  if (!radar.present) {
    humanState = HumanState::NOBODY;
    previousPresence = false;
    filteredDistanceCm = radar.distanceCm;
    previousFilteredDistanceCm = radar.distanceCm;
    return;
  }

  // 从无人刚变成有人，先判定为活动
  if (!previousPresence) {
    previousPresence = true;
    presenceStartMs = now;
    lastMotionMs = now;
    filteredDistanceCm = radar.distanceCm;
    previousFilteredDistanceCm = radar.distanceCm;
    humanState = HumanState::MOVING;
    return;
  }

  // 一阶低通滤波，减少测距轻微跳动带来的误判
  filteredDistanceCm = static_cast<uint16_t>(
      (static_cast<uint32_t>(filteredDistanceCm) * 3U + radar.distanceCm) / 4U);

  const uint16_t distanceChange =
      absoluteDifference(filteredDistanceCm, previousFilteredDistanceCm);

  if (distanceChange >= MOTION_DELTA_CM) {
    lastMotionMs = now;
    previousFilteredDistanceCm = filteredDistanceCm;
  }

  // 刚检测到人体，或最近检测到明显距离变化：活动
  if ((now - presenceStartMs < STILL_CONFIRM_MS) ||
      (now - lastMotionMs < MOVING_HOLD_MS)) {
    humanState = HumanState::MOVING;
  } else {
    humanState = HumanState::STILL;
  }
}

const char *stateToEnglish(HumanState state) {
  switch (state) {
    case HumanState::NO_DATA: return "NO_DATA";
    case HumanState::NOBODY:  return "NOBODY";
    case HumanState::MOVING:  return "MOVING";
    case HumanState::STILL:   return "STILL";
    default:                  return "UNKNOWN";
  }
}

const char *stateToChinese(HumanState state) {
  switch (state) {
    case HumanState::NO_DATA: return "无雷达数据";
    case HumanState::NOBODY:  return "无人";
    case HumanState::MOVING:  return "有人活动";
    case HumanState::STILL:   return "有人静止";
    default:                  return "未知";
  }
}

void printResult() {
  const uint32_t now = millis();
  if (now - lastPrintMs < PRINT_INTERVAL_MS) {
    return;
  }
  lastPrintMs = now;

  const uint32_t frameAge = radar.valid ? (now - radar.lastFrameMs) : 0;

  // 串口监视器查看的人类可读格式
  Serial.printf(
      "状态：%-10s | 距离：%4u cm | 原始状态：%u | 数据延迟：%lu ms\n",
      stateToChinese(humanState),
      radar.distanceCm,
      radar.rawTargetState,
      static_cast<unsigned long>(frameAge));

  // 输出一行 JSON（后续直接给小程序、网页或上位机解析）
  Serial.printf(
      "{\"state\":\"%s\",\"presence\":%d,\"distance_cm\":%u,"
      "\"raw_state\":%u,\"frame_age_ms\":%lu}\n",
      stateToEnglish(humanState),
      radar.present ? 1 : 0,
      radar.distanceCm,
      radar.rawTargetState,
      static_cast<unsigned long>(frameAge));
}

void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("========================================");
  Serial.println("ESP32-S3 + HLK-LD2410S 人体状态检测");
  Serial.println("状态：无人 / 有人活动 / 有人静止");
  Serial.println("========================================");

  RadarSerial.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  delay(300);

  setRadarMinimalMode();
  Serial.println("雷达已切换为极简数据模式，开始接收数据……");
}

void loop() {
  readRadarFrames();
  updateHumanState();
  printResult();
  delay(2);
}
