/**
 * @file    sensor_output.h
 * @brief   LD6002 传感器数据 JSON 格式化与输出模块 (STM32 测试版)
 * @author  Sensor Team
 * @date    2026-07-12
 *
 * 通过 USART2 (115200bps) 输出 JSON 格式的传感器数据。
 * 协作方的主控模块应从此 UART 接收 JSON 数据。
 *
 * JSON 输出格式:
 *   常规数据:  {"ts":1234,"br":18.5,"hr":72.3,"bp":1.23,"hp":5.67,"tp":3.45,"rng":85.0,"pres":1,"alert":0,"msg":""}
 *   报警数据:  {"ts":1234,"br":18.5,"hr":45.2,"bp":1.23,"hp":5.67,"tp":3.45,"rng":85.0,"pres":1,"alert":1,"msg":"heart_low"}
 *   离床告警:  {"ts":1234,"br":0.0,"hr":0.0,"bp":0.0,"hp":0.0,"tp":0.0,"rng":0.0,"pres":0,"alert":4,"msg":"bed_leave"}
 */

#ifndef __SENSOR_OUTPUT_H__
#define __SENSOR_OUTPUT_H__

#include <stdint.h>
#include "ld6002.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 宏定义
 * ================================================================ */

/** JSON 输出 UART 波特率 */
#define OUTPUT_BAUDRATE         115200

/** JSON 字符串最大长度 */
#define JSON_BUF_SIZE           256

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 JSON 输出模块
 *
 * 初始化 USART2 为 115200-8N1。
 * 输出引脚: PA2 (USART2 TX)
 */
void output_init(void);

/**
 * @brief 将传感器数据格式化为 JSON 并发送
 *
 * 根据 alert_flags 自动填充报警信息字段。
 * JSON 以换行符 '\n' 结尾，方便接收端按行解析。
 *
 * @param p_data  传感器数据指针
 */
void output_send_json(const ld6002_data_t *p_data);

/**
 * @brief 发送调试/状态日志 (非 JSON 格式，前缀 [LOG])
 *
 * @param fmt  格式化字符串
 * @param ...  可变参数
 */
void output_log(const char *fmt, ...);

/**
 * @brief 发送原始字符串 (USART2 底层发送)
 *
 * @param str   要发送的字符串
 * @param len   发送长度
 */
void output_send_raw(const char *str, uint16_t len);

/**
 * @brief 根据报警标志位获取描述字符串
 *
 * @param alert_flags  报警标志组合
 * @return             描述字符串 (如 "heart_low,bed_leave")
 */
const char *alert_to_string(uint8_t alert_flags);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_OUTPUT_H__ */
