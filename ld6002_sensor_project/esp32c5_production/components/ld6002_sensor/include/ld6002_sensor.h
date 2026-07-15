/**
 * @file    ld6002_sensor.h
 * @brief   顶层 API 头文件 - 协作方集成入口
 *
 * 使用方法:
 *   1. 在项目的 CMakeLists.txt 中添加:
 *      set(EXTRA_COMPONENT_DIRS path/to/components)
 *      或直接将 ld6002_sensor 目录放入 components/
 *
 *   2. 在代码中:
 *      #include "ld6002_sensor.h"
 *      ld6002_sensor_start(my_data_handler, my_alert_handler);
 *
 * 协作方只需 include 此文件即可使用全部功能。
 */

#ifndef __LD6002_SENSOR_H__
#define __LD6002_SENSOR_H__

#include "ld6002.h"
#include "tinyframe.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动传感器模块 (使用默认配置)
 *
 * 这是协作方主控模块唯一需要调用的初始化函数。
 * 传感器数据通过回调函数异步推送。
 *
 * @param on_data   数据回调 (每次有新的有效数据时调用)
 *                  参数: data - 结构化数据, json - JSON字符串, arg - 用户参数
 * @param on_alert  报警回调 (报警状态变化时调用)
 *                  参数: new_flags, old_flags, arg
 * @param arg       回调用户参数 (可为 NULL)
 * @retval ESP_OK   启动成功
 *
 * 示例:
 *   void on_sensor_data(const ld6002_data_t *d, const char *json, void *arg) {
 *       // 将 json 发送到主控任务队列
 *       xQueueSend(main_queue, &json, 0);
 *   }
 *   void on_alert(uint8_t new, uint8_t old, void *arg) {
 *       ESP_LOGE("ALERT", "Alert: %s", ld6002_alert_to_string(new));
 *   }
 *   ld6002_sensor_start(on_sensor_data, on_alert, NULL);
 */
static inline esp_err_t ld6002_sensor_start(
    ld6002_data_callback_t on_data,
    ld6002_alert_callback_t on_alert)
{
    return ld6002_init(on_data, on_alert);
}

/**
 * @brief 启动传感器模块 (使用自定义配置)
 */
static inline esp_err_t ld6002_sensor_start_ex(
    const ld6002_config_t *config)
{
    return ld6002_init_ex(config);
}

/**
 * @brief 停止传感器模块
 */
static inline esp_err_t ld6002_sensor_stop(void)
{
    return ld6002_deinit();
}

/* 以下是对 ld6002.h 中其他 API 的透传，协作方也可直接 include "ld6002.h" */

#ifdef __cplusplus
}
#endif

#endif /* __LD6002_SENSOR_H__ */
