/**
 * @file    ld6002.c
 * @brief   LD6002 呼吸心率雷达模组驱动实现 (STM32 HAL)
 *
 * USART1: 连接 LD6002, 1382400 bps
 * 使用中断接收 + 环形缓冲区
 *
 * STM32F103C8T6 波特率计算 (USART1 on APB2 @72MHz):
 *   DIV = 72,000,000 / (16 × 1,382,400) = 3.2552
 *   Mantissa = 3, Fraction = 4 (0.25×16)
 *   实际波特率 = 72,000,000 / (52) = 1,384,615 bps
 *   误差 = 0.16% ✓ (< 2%, 可以稳定通信)
 */

#include "ld6002.h"
#include "tinyframe.h"

/* STM32 HAL 头文件 */
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_uart.h"

/* ================================================================
 * 全局变量
 * ================================================================ */

/** UART 句柄 (USART1 - 连接 LD6002) */
static UART_HandleTypeDef huart1_ld6002;

/** 接收环形缓冲区 */
static volatile uint8_t rx_ring_buf[LD6002_RX_BUF_SIZE];
static volatile uint16_t rx_ring_head = 0;   /**< 写入位置 (ISR) */
static volatile uint16_t rx_ring_tail = 0;   /**< 读取位置 (主循环) */

/** TinyFrame 解析器实例 */
static tf_parser_t tf_parser;

/** 当前传感器数据 (由主循环读取，ISR 不直接修改) */
static ld6002_data_t sensor_data;

/** 数据版本号 (递增，协作方可据此判断数据是否更新) */
static volatile uint32_t data_version = 0;

/** 系统启动时间戳 (ms) */
static volatile uint32_t sys_tick_ms = 0;

/** 最后一次收到有效帧的时间 */
static volatile uint32_t last_valid_frame_ms = 0;

/* ================================================================
 * 环形缓冲区操作
 * ================================================================ */

static inline uint8_t ring_is_empty(void)
{
    return (rx_ring_head == rx_ring_tail);
}

static inline uint8_t ring_is_full(void)
{
    return (((rx_ring_head + 1) % LD6002_RX_BUF_SIZE) == rx_ring_tail);
}

static inline void ring_put(uint8_t byte)
{
    if (!ring_is_full()) {
        rx_ring_buf[rx_ring_head] = byte;
        rx_ring_head = (rx_ring_head + 1) % LD6002_RX_BUF_SIZE;
    }
    /* 满时丢弃最旧字节: 缓冲区满说明系统处理能力不足 */
}

static inline uint8_t ring_get(void)
{
    uint8_t byte = 0;
    if (!ring_is_empty()) {
        byte = rx_ring_buf[rx_ring_tail];
        rx_ring_tail = (rx_ring_tail + 1) % LD6002_RX_BUF_SIZE;
    }
    return byte;
}

/* ================================================================
 * 数据处理回调 (TinyFrame 解析完成后调用)
 * ================================================================ */

static void on_tf_frame(const tf_frame_t *frame)
{
    if (!frame || !frame->data_cksum_ok) {
        sensor_data.error_count++;
        return;
    }

    uint32_t now = sys_tick_ms;

    switch (frame->frame_type) {

        case LD6002_TYPE_PHASE: /* 0x0A13: 总相位 + 呼吸相位 + 心跳相位 */
            if (frame->data_len >= 12) {
                sensor_data.total_phase  = tf_bytes_to_float(&frame->data[0]);
                sensor_data.breath_phase = tf_bytes_to_float(&frame->data[4]);
                sensor_data.heart_phase  = tf_bytes_to_float(&frame->data[8]);
                sensor_data.data_valid = 1;
            }
            break;

        case LD6002_TYPE_BREATH_RATE: /* 0x0A14: 呼吸速率 */
            if (frame->data_len >= 4) {
                sensor_data.breath_rate = tf_bytes_to_float(&frame->data[0]);
                sensor_data.data_valid = 1;
            }
            break;

        case LD6002_TYPE_HEART_RATE: /* 0x0A15: 心率 */
            if (frame->data_len >= 4) {
                sensor_data.heart_rate = tf_bytes_to_float(&frame->data[0]);
                sensor_data.data_valid = 1;
            }
            break;

        case LD6002_TYPE_DISTANCE: /* 0x0A16: 目标距离 */
            if (frame->data_len >= 8) {
                uint32_t flag = tf_bytes_to_uint32(&frame->data[0]);
                sensor_data.range    = tf_bytes_to_float(&frame->data[4]);
                sensor_data.presence = (flag == 1) ? 1 : 0;
                sensor_data.data_valid = 1;
            }
            break;

        default:
            /* 未知帧类型，忽略但记录 */
            break;
    }

    /* 更新统计 */
    sensor_data.frame_count++;
    sensor_data.timestamp_ms = now;
    last_valid_frame_ms = now;

    /* ------------------------------------------------------------
     * 报警判断逻辑
     * ------------------------------------------------------------ */

    uint8_t alerts = LD6002_ALERT_NONE;

    /* 1. 传感器建立中 (上电1分钟内) */
    if (now < LD6002_SETTLE_MS) {
        alerts |= LD6002_ALERT_SETTLING;
    }

    /* 2. 心率异常检测 (仅在建立完成后判断，且有有效数据) */
    if (now >= LD6002_SETTLE_MS && sensor_data.data_valid) {
        if (sensor_data.heart_rate > 0.1f && sensor_data.heart_rate < HR_MIN_NORMAL) {
            alerts |= LD6002_ALERT_HEART_LOW;
        }
        if (sensor_data.heart_rate > HR_MAX_NORMAL) {
            alerts |= LD6002_ALERT_HEART_HIGH;
        }
    }

    /* 3. 离床检测 (仅在建立完成后判断) */
    if (now >= LD6002_SETTLE_MS && sensor_data.presence == 0) {
        alerts |= LD6002_ALERT_BED_LEAVE;
    }

    /* 4. 传感器超时检测 (在主循环中处理) */

    sensor_data.alert_flags = alerts;

    /* 递增数据版本 */
    data_version++;
}

/* ================================================================
 * 公开 API 实现
 * ================================================================ */

void ld6002_init(void)
{
    /* 清零传感器数据结构 */
    memset(&sensor_data, 0, sizeof(sensor_data));

    /* ------------------------------------------------------------
     * 配置 USART1: 1382400-8N1, 无流控
     * ------------------------------------------------------------ */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9 = TX, PA10 = RX */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin       = GPIO_PIN_10;
    gpio.Mode      = GPIO_MODE_INPUT;
    gpio.Pull      = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart1_ld6002.Instance          = USART1;
    huart1_ld6002.Init.BaudRate     = LD6002_BAUDRATE;
    huart1_ld6002.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1_ld6002.Init.StopBits     = UART_STOPBITS_1;
    huart1_ld6002.Init.Parity       = UART_PARITY_NONE;
    huart1_ld6002.Init.Mode         = UART_MODE_TX_RX;
    huart1_ld6002.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1_ld6002.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1_ld6002);

    /* 使能 UART 接收中断 (单字节中断) */
    __HAL_UART_ENABLE_IT(&huart1_ld6002, UART_IT_RXNE);

    /* 配置 NVIC (USART1 中断优先级) */
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    /* ------------------------------------------------------------
     * 初始化 TinyFrame 解析器
     * ------------------------------------------------------------ */
    tf_init(&tf_parser, on_tf_frame);

    /* ------------------------------------------------------------
     * 记录系统时间
     * ------------------------------------------------------------ */
    sys_tick_ms = HAL_GetTick();
    last_valid_frame_ms = sys_tick_ms;
}

void ld6002_process(void)
{
    /* 更新系统时间戳 */
    sys_tick_ms = HAL_GetTick();

    /* ------------------------------------------------------------
     * 处理环形缓冲区中的所有字节
     * ------------------------------------------------------------ */
    while (!ring_is_empty()) {
        uint8_t byte = ring_get();
        tf_feed_byte(&tf_parser, byte);
    }

    /* ------------------------------------------------------------
     * 传感器超时检测
     * ------------------------------------------------------------ */
    if (sensor_data.data_valid) {
        uint32_t elapsed = sys_tick_ms - last_valid_frame_ms;
        if (elapsed > LD6002_TIMEOUT_MS) {
            sensor_data.alert_flags |= LD6002_ALERT_SENSOR_ERR;
        }
    }
}

uint8_t ld6002_get_data(ld6002_data_t *p_data)
{
    if (!p_data) return 0;

    /* 禁用中断以确保原子读取 (STM32F1 无 LDREX/STREX) */
    __disable_irq();
    memcpy(p_data, (void *)&sensor_data, sizeof(ld6002_data_t));
    __enable_irq();

    return p_data->data_valid;
}

uint8_t ld6002_is_stable(void)
{
    return (sys_tick_ms >= LD6002_SETTLE_MS) ? 1 : 0;
}

uint8_t ld6002_has_presence(void)
{
    return sensor_data.presence;
}

uint8_t ld6002_get_alert(void)
{
    return sensor_data.alert_flags;
}

/* ================================================================
 * UART 中断回调 (在 stm32f1xx_it.c 的 USART1_IRQHandler 中调用)
 * ================================================================ */

void ld6002_uart_rx_callback(uint8_t byte)
{
    ring_put(byte);
}

/* ================================================================
 * UART 中断处理函数 (平台相关，调用 ld6002_uart_rx_callback)
 * ================================================================ */

void USART1_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1_ld6002, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart1_ld6002.Instance->DR & 0xFF);
        ld6002_uart_rx_callback(byte);
        __HAL_UART_CLEAR_FLAG(&huart1_ld6002, UART_FLAG_RXNE);
    }

    /* 其他 UART 错误处理 (溢出、帧错误等) */
    if (__HAL_UART_GET_FLAG(&huart1_ld6002, UART_FLAG_ORE)) {
        /* 溢出: 读取 DR 清除 */
        (void)(huart1_ld6002.Instance->DR & 0xFF);
        __HAL_UART_CLEAR_FLAG(&huart1_ld6002, UART_FLAG_ORE);
    }

    if (__HAL_UART_GET_FLAG(&huart1_ld6002, UART_FLAG_FE)) {
        __HAL_UART_CLEAR_FLAG(&huart1_ld6002, UART_FLAG_FE);
    }

    if (__HAL_UART_GET_FLAG(&huart1_ld6002, UART_FLAG_NE)) {
        __HAL_UART_CLEAR_FLAG(&huart1_ld6002, UART_FLAG_NE);
    }
}

/**
 * @brief HAL UART 初始化回调 (HAL 库自动调用)
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    /* 时钟和 GPIO 已在 ld6002_init() 中配置 */
    /* 此处为空，避免与 ld6002_init() 重复 */
}
