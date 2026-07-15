/**
 * @file    main.c
 * @brief   LD6002 传感器测试程序 (STM32F103C8T6 Blue Pill)
 *
 * 功能:
 *   1. 初始化系统时钟 (72MHz HSE/PLL)
 *   2. 初始化 LD6002 传感器驱动 (USART1, 1382400 bps)
 *   3. 初始化 JSON 输出模块 (USART2, 115200 bps)
 *   4. 主循环: 处理传感器数据 → 格式化 JSON → 通过 USART2 输出
 *
 * 硬件连接:
 *   LD6002 3V3   → STM32 3.3V (AMS1117, ≥1A)
 *   LD6002 GND   → STM32 GND
 *   LD6002 P19   → STM32 GND (Boot1=0, Flash启动)
 *   LD6002 TX0   → STM32 PA10 (USART1 RX)
 *   LD6002 RX0   → STM32 PA9  (USART1 TX)
 *
 * JSON 输出 (连接 USB-TTL 模块到 PC):
 *   STM32 PA2 (USART2 TX) → USB-TTL RX
 *   STM32 GND             → USB-TTL GND
 *
 * 测试步骤:
 *   1. 编译烧录固件到 STM32F103C8T6
 *   2. 按硬件连接表连接 LD6002 和 USB-TTL 模块
 *   3. 上电，打开串口助手 (115200-8N1) 连接 USB-TTL
 *   4. 约1分钟后传感器建立稳定探测，开始输出 JSON 数据
 *   5. 观察 JSON 输出中的呼吸率、心率、距离、报警等信息
 */

#include "stm32f1xx_hal.h"
#include "ld6002.h"
#include "sensor_output.h"
#include <stdio.h>

/* ================================================================
 * 系统时钟配置 (72MHz, HSE 8MHz × 9 PLL)
 * ================================================================ */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 使能 HSE (8MHz 晶振) */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;  /* 8MHz × 9 = 72MHz */
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* AHB=72MHz, APB1=36MHz, APB2=72MHz */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1
                                     | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK = 72MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;     /* APB1 = 36MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2 = 72MHz */
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

/* ================================================================
 * SysTick 定时器 (提供 HAL_GetTick() 毫秒时间基准)
 * ================================================================ */

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ================================================================
 * HardFault 处理 (调试用)
 * ================================================================ */

void HardFault_Handler(void)
{
    output_log("[ERROR] HardFault! System halted.");
    while (1) {}
}

/* ================================================================
 * 主函数
 * ================================================================ */

int main(void)
{
    /* HAL 库初始化 */
    HAL_Init();

    /* 配置系统时钟 72MHz */
    SystemClock_Config();

    /* 初始化 JSON 输出 (优先初始化，方便调试日志) */
    output_init();

    output_log("[INIT] LD6002 Sensor Test Firmware");
    output_log("[INIT] STM32F103C8T6 @ 72MHz");
    output_log("[INIT] System boot OK");

    /* 初始化 LD6002 传感器驱动 */
    ld6002_init();
    output_log("[INIT] LD6002 driver initialized (USART1, 1382400 bps)");
    output_log("[INIT] Sensor settling... (~60s)");

    /* 上一次 JSON 发送时间 */
    uint32_t last_output_ms = 0;
    const uint32_t output_interval_ms = 200; /* JSON 发送间隔 200ms (5Hz) */

    /* 上一次稳定状态 (用于检测状态变化) */
    uint8_t was_stable = 0;

    /* ================================================================
     * 主循环
     * ================================================================ */
    while (1) {
        /* --- 处理传感器数据 (解析接收到的 UART 字节) --- */
        ld6002_process();

        /* --- 检测稳定状态变化 --- */
        uint8_t is_stable = ld6002_is_stable();
        if (is_stable && !was_stable) {
            output_log("[STATUS] Sensor stable! Detection ready.");
        }
        was_stable = is_stable;

        /* --- 定时输出 JSON --- */
        uint32_t now = HAL_GetTick();
        if (now - last_output_ms >= output_interval_ms) {
            last_output_ms = now;

            /* 获取最新传感器数据 */
            ld6002_data_t data;
            if (ld6002_get_data(&data)) {
                /* 发送 JSON 数据 */
                output_send_json(&data);

                /* 报警时额外发送日志 */
                if (data.alert_flags != LD6002_ALERT_NONE
                    && data.alert_flags != LD6002_ALERT_SETTLING) {
                    output_log("[ALERT] flags=0x%02X, hr=%.1f, br=%.1f, msg=%s",
                               data.alert_flags,
                               data.heart_rate,
                               data.breath_rate,
                               alert_to_string(data.alert_flags));
                }
            }
        }
    }
}
