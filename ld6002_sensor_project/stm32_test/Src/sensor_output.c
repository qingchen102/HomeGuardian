/**
 * @file    sensor_output.c
 * @brief   JSON 格式化输出模块 (USART2, 115200 bps)
 *
 * 输出引脚: PA2 (USART2 TX)
 * 协作方主控模块连接: PA3 (USART2 RX) <-- 主控模块的 RX
 */

#include "sensor_output.h"
#include <stdio.h>
#include <stdarg.h>

/* STM32 HAL */
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_uart.h"

/* ================================================================
 * 全局变量
 * ================================================================ */

static UART_HandleTypeDef huart2_output;

/* ================================================================
 * 报警字符串
 * ================================================================ */

const char *alert_to_string(uint8_t alert_flags)
{
    static char buf[64];
    uint16_t pos = 0;
    buf[0] = '\0';

    if (alert_flags == LD6002_ALERT_NONE) {
        return "";
    }

    if (alert_flags & LD6002_ALERT_SETTLING) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%ssettling",
                        (pos > 0) ? "," : "");
    }
    if (alert_flags & LD6002_ALERT_HEART_LOW) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%sheart_low",
                        (pos > 0) ? "," : "");
    }
    if (alert_flags & LD6002_ALERT_HEART_HIGH) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%sheart_high",
                        (pos > 0) ? "," : "");
    }
    if (alert_flags & LD6002_ALERT_BED_LEAVE) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%sbed_leave",
                        (pos > 0) ? "," : "");
    }
    if (alert_flags & LD6002_ALERT_SENSOR_ERR) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%ssensor_err",
                        (pos > 0) ? "," : "");
    }
    if (alert_flags & LD6002_ALERT_MOTION) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%smotion",
                        (pos > 0) ? "," : "");
    }

    return buf;
}

/* ================================================================
 * JSON 格式化与发送
 * ================================================================ */

void output_send_json(const ld6002_data_t *p_data)
{
    if (!p_data) return;

    char json[JSON_BUF_SIZE];
    const char *alert_msg = alert_to_string(p_data->alert_flags);

    /* ------------------------------------------------------------
     * 构造紧凑 JSON (无多余空格，节省带宽)
     *
     * 字段说明:
     *   ts    - timestamp_ms, 毫秒时间戳
     *   br    - breath_rate, 呼吸率 (bpm)
     *   hr    - heart_rate, 心率 (bpm)
     *   bp    - breath_phase, 呼吸相位
     *   hp    - heart_phase, 心跳相位
     *   tp    - total_phase, 总相位
     *   rng   - range, 目标距离 (cm)
     *   pres  - presence, 1=有人 0=无人
     *   alert - alert_flags, 报警标志位
     *   msg   - alert message, 报警描述字符串
     * ------------------------------------------------------------ */
    int len = snprintf(json, sizeof(json),
        "{\"ts\":%lu,\"br\":%.1f,\"hr\":%.1f,\"bp\":%.2f,\"hp\":%.2f,"
        "\"tp\":%.2f,\"rng\":%.1f,\"pres\":%u,\"alert\":%u,\"msg\":\"%s\"}\n",
        (unsigned long)p_data->timestamp_ms,
        p_data->breath_rate,
        p_data->heart_rate,
        p_data->breath_phase,
        p_data->heart_phase,
        p_data->total_phase,
        p_data->range,
        p_data->presence,
        p_data->alert_flags,
        alert_msg);

    /* 防止 snprintf 截断 */
    if (len < 0) {
        len = 0;
    } else if (len >= JSON_BUF_SIZE) {
        len = JSON_BUF_SIZE - 1;
        json[JSON_BUF_SIZE - 2] = '\n';
        json[JSON_BUF_SIZE - 1] = '\0';
    }

    /* 通过 USART2 发送 */
    HAL_UART_Transmit(&huart2_output, (uint8_t *)json, (uint16_t)len, 100);
}

void output_send_raw(const char *str, uint16_t len)
{
    if (str && len > 0) {
        HAL_UART_Transmit(&huart2_output, (uint8_t *)str, len, 100);
    }
}

void output_log(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    if (len > 0) {
        /* 添加换行 */
        if ((uint16_t)len < sizeof(buf) - 1) {
            buf[len]     = '\n';
            buf[len + 1] = '\0';
            HAL_UART_Transmit(&huart2_output, (uint8_t *)buf, (uint16_t)(len + 1), 100);
        } else {
            HAL_UART_Transmit(&huart2_output, (uint8_t *)buf, (uint16_t)len, 100);
        }
    }
}

/* ================================================================
 * 输出模块初始化
 * ================================================================ */

void output_init(void)
{
    /* ------------------------------------------------------------
     * 配置 USART2: 115200-8N1
     * TX = PA2 (AF_PP), RX = PA3 (Input, 协作方连接用)
     *
     * STM32F103C8T6 USART2 on APB1 @36MHz:
     *   DIV = 36,000,000 / (16 × 115,200) = 19.531
     *   Mantissa = 19, Fraction = 8
     *   误差 = 0.16% ✓
     * ------------------------------------------------------------ */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_2;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin  = GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart2_output.Instance          = USART2;
    huart2_output.Init.BaudRate     = OUTPUT_BAUDRATE;
    huart2_output.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2_output.Init.StopBits     = UART_STOPBITS_1;
    huart2_output.Init.Parity       = UART_PARITY_NONE;
    huart2_output.Init.Mode         = UART_MODE_TX; /* 仅发送 */
    huart2_output.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2_output.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2_output);
}
