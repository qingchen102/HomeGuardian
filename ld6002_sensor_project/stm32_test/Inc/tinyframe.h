/**
 * @file    tinyframe.h
 * @brief   TinyFrame 协议解析器 (LD6002 雷达模组通信协议 V1.2)
 * @author  Sensor Team
 * @date    2026-07-12
 *
 * TinyFrame 帧结构:
 *   SOF(1) | ID(2) | LEN(2) | TYPE(2) | HEAD_CKSUM(1) | DATA(LEN) | DATA_CKSUM(1)
 *
 * - SOF: 固定为 0x01
 * - ID/ LEN/ TYPE: 大端序 (Big-Endian) uint16
 * - DATA: 小端序 (Little-Endian) float / uint32
 * - HEAD_CKSUM: 从 SOF 字节到 HEAD_CKSUM 前一个字节的 XOR 取反
 * - DATA_CKSUM: 从 DATA 第一个字节到 DATA_CKSUM 前一个字节的 XOR 取反
 *
 * 校验算法:
 *   unsigned char getCksum(unsigned char *data, unsigned char len) {
 *       unsigned char ret = 0;
 *       for (int i = 0; i < len; i++) ret = ret ^ data[i];
 *       ret = ~ret;
 *       return ret;
 *   }
 */

#ifndef __TINYFRAME_H__
#define __TINYFRAME_H__

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 宏定义
 * ================================================================ */

/** TinyFrame 起始帧标识 */
#define TF_SOF                  0x01

/** 帧头固定长度: SOF(1) + ID(2) + LEN(2) + TYPE(2) + HEAD_CKSUM(1) = 8 */
#define TF_HEADER_LEN           8

/** 最大数据负载长度 */
#define TF_MAX_DATA_LEN         32

/** 最大帧总长度 */
#define TF_MAX_FRAME_LEN        (TF_HEADER_LEN + TF_MAX_DATA_LEN + 1)

/* ================================================================
 * 解析器状态机状态
 * ================================================================ */

typedef enum {
    TF_STATE_WAIT_SOF = 0,      /**< 等待起始帧 0x01 */
    TF_STATE_WAIT_ID_H,         /**< 等待帧ID高字节 */
    TF_STATE_WAIT_ID_L,         /**< 等待帧ID低字节 */
    TF_STATE_WAIT_LEN_H,        /**< 等待数据长度高字节 */
    TF_STATE_WAIT_LEN_L,        /**< 等待数据长度低字节 */
    TF_STATE_WAIT_TYPE_H,       /**< 等待帧类型高字节 */
    TF_STATE_WAIT_TYPE_L,       /**< 等待帧类型低字节 */
    TF_STATE_WAIT_HEAD_CKSUM,   /**< 等待头校验和 */
    TF_STATE_WAIT_DATA,         /**< 等待数据负载 */
    TF_STATE_WAIT_DATA_CKSUM,   /**< 等待数据校验和 */
} tf_state_t;

/* ================================================================
 * 数据结构
 * ================================================================ */

/**
 * @brief TinyFrame 解析后的帧结构
 */
typedef struct {
    uint16_t frame_id;          /**< 帧ID */
    uint16_t data_len;          /**< 数据长度 (字节) */
    uint16_t frame_type;        /**< 帧类型 */
    uint8_t  data[TF_MAX_DATA_LEN]; /**< 数据负载 */
    uint8_t  head_cksum_ok;    /**< 头校验和通过 */
    uint8_t  data_cksum_ok;    /**< 数据校验和通过 */
} tf_frame_t;

/**
 * @brief TinyFrame 解析器句柄
 */
typedef struct {
    tf_state_t state;           /**< 当前解析状态 */
    uint16_t   frame_id;       /**< 正在解析的帧ID */
    uint16_t   data_len;       /**< 正在解析的数据长度 */
    uint16_t   frame_type;     /**< 正在解析的帧类型 */
    uint8_t    head_buf[TF_HEADER_LEN]; /**< 帧头缓冲区 */
    uint8_t    head_idx;        /**< 帧头缓冲索引 */
    uint8_t    data_buf[TF_MAX_DATA_LEN]; /**< 数据缓冲区 */
    uint8_t    data_idx;        /**< 数据缓冲索引 */
    uint8_t    head_cksum;      /**< 接收到的头校验和 */
    uint8_t    calc_head_cksum; /**< 计算得到的头校验和 */
    uint8_t    data_cksum;      /**< 接收到的数据校验和 */
    uint8_t    calc_data_cksum; /**< 计算得到的数据校验和 */

    /* 统计 */
    uint32_t   frame_rx_count;  /**< 成功接收帧计数 */
    uint32_t   error_count;     /**< 错误帧计数 */

    /* 回调: 当解析出完整帧时调用 */
    void (*on_frame)(const tf_frame_t *frame);
} tf_parser_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */

/**
 * @brief 初始化 TinyFrame 解析器
 *
 * @param parser     解析器句柄指针
 * @param on_frame   帧接收回调函数 (可为 NULL)
 */
void tf_init(tf_parser_t *parser, void (*on_frame)(const tf_frame_t *frame));

/**
 * @brief 喂入一个字节到解析器
 *
 * 由 UART 接收中断或轮询调用。
 *
 * @param parser  解析器句柄指针
 * @param byte    接收到的字节
 * @retval 1      成功解析出一个完整帧
 * @retval 0      帧未完成或解析中
 */
uint8_t tf_feed_byte(tf_parser_t *parser, uint8_t byte);

/**
 * @brief 计算 TinyFrame 校验和
 *
 * 对 data[0..len-1] 逐字节 XOR，结果取反。
 *
 * @param data  数据指针
 * @param len   数据长度
 * @return      校验和
 */
uint8_t tf_calc_cksum(const uint8_t *data, uint8_t len);

/**
 * @brief 将4字节小端序数据转换为 float
 *
 * LD6002 的 DATA 段采用小端序 IEEE 754 格式。
 *
 * @param bytes  指向4字节数据的指针
 * @return       float 值
 */
float tf_bytes_to_float(const uint8_t *bytes);

/**
 * @brief 将4字节小端序数据转换为 uint32
 *
 * @param bytes  指向4字节数据的指针
 * @return       uint32 值
 */
uint32_t tf_bytes_to_uint32(const uint8_t *bytes);

/**
 * @brief 获取帧类型名称 (调试用)
 *
 * @param frame_type  帧类型
 * @return            类型名称字符串
 */
const char *tf_type_name(uint16_t frame_type);

#ifdef __cplusplus
}
#endif

#endif /* __TINYFRAME_H__ */
