/**
 * @file    main.c
 * @brief   ESP32-C5 LD6002 传感器示例主程序
 *
 * 此文件是给"协作方-主控模块开发者"的参考示例。
 * 展示了如何集成 ld6002_sensor 组件。
 *
 * 协作方需要在 main.c 中:
 *   1. 初始化 WiFi/BLE/OLED
 *   2. 调用 ld6002_sensor_start() 并注册回调
 *   3. 在回调中将 JSON 数据转发到云端/OLED/小程序
 *   4. 在报警回调中处理报警 (蜂鸣器/推送通知/云端记录)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ld6002_sensor.h"

static const char *TAG = "main";

/* ================================================================
 * 示例: 主控模块的数据队列 (用于将传感器数据传递给其他任务)
 * ================================================================ */

/* 队列元素: JSON 字符串指针 */
static QueueHandle_t g_data_queue = NULL;

/* ================================================================
 * 传感器数据回调
 * ================================================================ */

static void on_sensor_data(const ld6002_data_t *data,
                            const char *json,
                            void *arg)
{
    /* ------------------------------------------------------------
     * 协作方在此处理传感器数据:
     *
     * 1. 发送 JSON 到主控任务队列
     * 2. 更新 OLED 显示 (心率、呼吸率)
     * 3. 通过 WiFi 发送到云端 (MQTT/HTTP)
     * ------------------------------------------------------------ */

    /* 示例: 将 JSON 放入队列, 由主控任务统一处理 */
    if (g_data_queue && json) {
        /* 复制 JSON 字符串 (队列传递指针不安全, 应复制) */
        size_t len = strlen(json) + 1;
        char *json_copy = malloc(len);
        if (json_copy) {
            memcpy(json_copy, json, len);
            if (xQueueSend(g_data_queue, &json_copy, 0) != pdTRUE) {
                free(json_copy); /* 队列满, 丢弃 */
            }
        }
    }

    /* 日志输出 (调试用, 生产环境可关闭) */
    ESP_LOGD(TAG, "Sensor: HR=%.1f, BR=%.1f, Range=%.1fcm, Presence=%u",
             data->heart_rate, data->breath_rate,
             data->range, data->presence);
}

/* ================================================================
 * 传感器报警回调
 * ================================================================ */

static void on_sensor_alert(uint8_t new_flags,
                             uint8_t old_flags,
                             void *arg)
{
    const char *msg = ld6002_alert_to_string(new_flags);

    ESP_LOGW(TAG, "ALERT CHANGE: 0x%02X -> 0x%02X (%s)",
             old_flags, new_flags, msg);

    /* ------------------------------------------------------------
     * 协作方在此处理报警:
     *
     * 1. 心率异常: 蜂鸣器告警 + 推送通知 + 云端记录
     * 2. 离床告警: 推送通知给家属/护工
     * 3. 传感器异常: 本地蜂鸣器提示检查设备
     * ------------------------------------------------------------ */

    /* 示例: 根据报警类型执行不同动作 */
    if (new_flags & LD6002_ALERT_HEART_LOW) {
        ESP_LOGE(TAG, "[ACTION] Buzzer + Push: Heart rate too LOW!");
        /* buzzer_on(); send_push_notification("心率过低"); */
    }
    if (new_flags & LD6002_ALERT_HEART_HIGH) {
        ESP_LOGE(TAG, "[ACTION] Buzzer + Push: Heart rate too HIGH!");
        /* buzzer_on(); send_push_notification("心率过高"); */
    }
    if (new_flags & LD6002_ALERT_BED_LEAVE) {
        ESP_LOGE(TAG, "[ACTION] Push: Bed left!");
        /* send_push_notification("老人离床"); */
    }
    if (new_flags & LD6002_ALERT_SENSOR_ERR) {
        ESP_LOGE(TAG, "[ACTION] Buzzer: Sensor timeout!");
        /* buzzer_on(); */
    }

    /* 报警恢复时关闭蜂鸣器 */
    if (new_flags == LD6002_ALERT_NONE && old_flags != LD6002_ALERT_NONE) {
        ESP_LOGI(TAG, "[ACTION] All alerts cleared — buzzer off");
        /* buzzer_off(); */
    }
}

/* ================================================================
 * 主控数据处理任务 (示例)
 * ================================================================ */

static void main_controller_task(void *arg)
{
    ESP_LOGI(TAG, "Main controller task started");

    /* 初始化 WiFi / MQTT / OLED 等 */
    /* wifi_init(); mqtt_connect(); oled_init(); */

    while (1) {
        char *json = NULL;
        if (xQueueReceive(g_data_queue, &json, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (json) {
                /* ------------------------------------------------------------
                 * 协作方在此将 JSON 数据分发到各个模块:
                 *
                 * 1. mqtt_publish("sensor/ld6002", json);
                 * 2. oled_update(json);
                 * 3. 通过 UART 转发给微信小程序模块
                 * ------------------------------------------------------------ */

                /* 示例: 通过日志输出 JSON */
                ESP_LOGI(TAG, "JSON: %s", json);

                free(json);
            }
        }
    }
}

/* ================================================================
 * 应用入口
 * ================================================================ */

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "LD6002 Sensor + ESP32-C5 Main Controller");
    ESP_LOGI(TAG, "========================================");

    /* 初始化 NVS (WiFi 配置等需要) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES
        || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* 创建数据队列 */
    g_data_queue = xQueueCreate(10, sizeof(char *));
    assert(g_data_queue);

    /* 创建主控任务 (协作方在此处理 WiFi/OLED/Cloud 等) */
    xTaskCreate(main_controller_task,
                "main_ctrl",
                8192,
                NULL,
                3,
                NULL);

    /* 启动传感器模块 (只需调用此函数!) */
    ret = ld6002_sensor_start(on_sensor_data, on_sensor_alert);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start sensor: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "System ready. Waiting for sensor data...");

    /* 传感器在独立任务中运行, 主循环可处理其他事务 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); /* 10s 心跳 */
        ESP_LOGI(TAG, "Heartbeat: stable=%d, presence=%d, alert=0x%02X",
                 ld6002_is_stable(),
                 ld6002_has_presence(),
                 ld6002_get_alert());
    }
}
