/**
 * @file    ld6002.h
 * @brief   LD6002 呼吸心率雷达模组驱动 (STM32 测试版)
 * @author  Sensor Team
 * @date    2026-07-12
 *
 * 硬件连接 (STM32F103C8T6 Blue Pill):
 *   LD6002 3V3  → 3.3V (AMS1117, ≥1A, 纹波 ≤50mV)
 *   LD6002 GND  → GND
 *   LD6002 TX0  → PA10 (USART1 RX)
 *   LD6002 RX0  → PA9  (USART1 TX)
 *   LD6002 P19  → GND  (启动模式: Flash启动)
 *
 * 波特率: 1382400 bps (非标准高速波特率)
 * 通信协议: TinyFrame (参见 tinyframe.h)
 */

#ifndef __LD6002_H__
#define __LD6002_H__

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 宏定义
 * ================================================================ */

/** LD6002 UART 波特率 */
#define LD6002_BAUDRATE         1382400

/** 默认数据刷新间隔 (ms) */
#define LD6002_REFRESH_MS       50

/** 传感器建立稳定探测所需时间 (ms)，约1分钟 */
#define LD6002_SETTLE_MS        60000

/** 数据超时时间 (ms)，超过此时间无有效帧则判定传感器异常 */
#define LD6002_TIMEOUT_MS       3000

/** 接收环形缓冲区大小 */
#define LD6002_RX_BUF_SIZE      512

/** 最大单帧数据长度 (字节) */
#define LD6002_MAX_FRAME_SIZE   64

/* ================================================================
 * 传感器帧类型 (TinyFrame TYPE 字段)
 * ================================================================ */

/** 0x0A13: 总相位 + 呼吸相位 + 心跳相位 (3个float, 12字节) */
#define LD6002_TYPE_PHASE       0x0A13

/** 0x0A14: 呼吸速率 (1个float, 4字节) */
#define LD6002_TYPE_BREATH_RATE 0x0A14

/** 0x0A15: 心跳速率 (1个float, 4字节) */
#define LD6002_TYPE_HEART_RATE  0x0A15

/** 0x0A16: 目标距离 (flag uint32 + range float, 8字节) */
#define LD6002_TYPE_DISTANCE    0x0A16

/* ================================================================
 * 报警标志位 (可组合)
 * ================================================================ */

/** 无报警 */
#define LD6002_ALERT_NONE       0x00
/** 心率过低 (<50 bpm) */
#define LD6002_ALERT_HEART_LOW  0x01
/** 心率过高 (>120 bpm) */
#define LD6002_ALERT_HEART_HIGH 0x02
/** 离床/无人检测 */
#define LD6002_ALERT_BED_LEAVE  0x04
/** 传感器超时/异常 */
#define LD6002_ALERT_SENSOR_ERR 0x08
/** 传感器建立中 (上电1分钟内) */
#define LD6002_ALERT_SETTLING   0x10
/** 检测到大幅身体运动 (心跳数据不可靠) */
#define LD6002_ALERT_MOTION     0x20

/* ================================================================
 * 心率报警阈值
 * ================================================================ */

#define HR_MIN_NORMAL           50.0f   /**< 正常心率下限 */
#define HR_MAX_NORMAL           120.0f  /**< 正常心率上限 */

/* ================================================================
 * 数据结构
 * ================================================================ */

/**
 * @brief LD6002 传感器完整数据结构体
 */
typedef struct {
    /* 呼吸数据 */
    float breath_rate;          /**< 呼吸速率 (bpm)，范围 9-48 */
    float breath_phase;         /**< 呼吸相位 */

    /* 心率数据 */
    float heart_rate;           /**< 心率 (bpm)，范围 60-150 */
    float heart_phase;          /**< 心跳相位 */

    /* 通用数据 */
    float total_phase;          /**< 总相位 */

    /* 距离/存在检测 */
    float range;                /**< 目标距离 (cm) */
    uint8_t presence;           /**< 1=检测到人体, 0=无人 */

    /* 时间戳 */
    uint32_t timestamp_ms;      /**< 数据时间戳 (ms) */

    /* 状态 */
    uint8_t alert_flags;        /**< 报警标志 (LD6002_ALERT_xxx 组合) */
    uint8_t data_valid;         /**< 1=至少收到过一个有效帧 */

    /* 帧接收计数 (调试用) */
    uint32_t frame_count;       /**< 累计有效帧数 */
    uint32_t error_count;       /**< 累计错误帧数 */
} ld6002_data_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 LD6002 传感器驱动
 *
 * 初始化 USART1 为 1382400-8N1，使能接收中断，配置环形缓冲区。
 * 调用此函数后传感器即开始接收数据。
 *
 * @note 必须在 SystemClock_Config() 之后调用
 */
void ld6002_init(void);

/**
 * @brief 获取最新传感器数据
 *
 * @param[out] p_data  指向数据结构的指针，用于接收数据副本
 * @retval 1  数据有效
 * @retval 0  尚无有效数据
 */
uint8_t ld6002_get_data(ld6002_data_t *p_data);

/**
 * @brief 检查传感器是否处于稳定探测状态
 *
 * 上电后需要约1分钟建立时间。在此期间数据可能不准确。
 *
 * @retval 1  稳定状态
 * @retval 0  建立中
 */
uint8_t ld6002_is_stable(void);

/**
 * @brief 检查是否有人体在检测范围内
 *
 * @retval 1  有人
 * @retval 0  无人
 */
uint8_t ld6002_has_presence(void);

/**
 * @brief 获取当前报警状态
 *
 * @return 报警标志位组合 (LD6002_ALERT_xxx)
 */
uint8_t ld6002_get_alert(void);

/**
 * @brief LD6002 数据处理任务 (需在主循环中周期性调用)
 *
 * 处理接收到的字节流，解析 TinyFrame，更新传感器数据结构。
 * 建议调用间隔 ≤ 50ms。
 */
void ld6002_process(void);

/**
 * @brief UART 接收中断回调 (在 stm32f1xx_it.c 的 USART1_IRQHandler 中调用)
 *
 * @param byte  接收到的字节
 */
void ld6002_uart_rx_callback(uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* __LD6002_H__ */
