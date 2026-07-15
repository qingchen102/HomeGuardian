/**
 * @file    tinyframe.c
 * @brief   TinyFrame 协议解析器实现
 * @note    参见 tinyframe.h 获取协议详情
 */

#include "tinyframe.h"
#include <stdio.h>

/* ================================================================
 * 校验和计算
 * ================================================================ */

uint8_t tf_calc_cksum(const uint8_t *data, uint8_t len)
{
    uint8_t ret = 0;
    for (uint8_t i = 0; i < len; i++) {
        ret ^= data[i];
    }
    ret = ~ret;
    return ret;
}

/* ================================================================
 * 数据转换 (小端序)
 * ================================================================ */

float tf_bytes_to_float(const uint8_t *bytes)
{
    uint32_t raw;
    /* LD6002 数据段为小端序: 最低字节在前 */
    raw = (uint32_t)bytes[0]
        | ((uint32_t)bytes[1] << 8)
        | ((uint32_t)bytes[2] << 16)
        | ((uint32_t)bytes[3] << 24);

    float result;
    memcpy(&result, &raw, sizeof(float));
    return result;
}

uint32_t tf_bytes_to_uint32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0]
         | ((uint32_t)bytes[1] << 8)
         | ((uint32_t)bytes[2] << 16)
         | ((uint32_t)bytes[3] << 24);
}

/* ================================================================
 * 帧类型名称 (调试用)
 * ================================================================ */

const char *tf_type_name(uint16_t frame_type)
{
    switch (frame_type) {
        case 0x0A13: return "PHASE";
        case 0x0A14: return "BREATH_RATE";
        case 0x0A15: return "HEART_RATE";
        case 0x0A16: return "DISTANCE";
        default:     return "UNKNOWN";
    }
}

/* ================================================================
 * 解析器初始化
 * ================================================================ */

void tf_init(tf_parser_t *parser, void (*on_frame)(const tf_frame_t *frame))
{
    if (!parser) return;

    memset(parser, 0, sizeof(tf_parser_t));
    parser->state = TF_STATE_WAIT_SOF;
    parser->on_frame = on_frame;
}

/* ================================================================
 * 逐字节喂入解析器 (状态机)
 *
 * 帧结构:
 *   [SOF:1] [ID:2] [LEN:2] [TYPE:2] [HCK:1] [DATA:LEN] [DCK:1]
 *
 * HEAD_CKSUM 覆盖: SOF + ID + LEN + TYPE = 7字节
 * ================================================================ */

uint8_t tf_feed_byte(tf_parser_t *parser, uint8_t byte)
{
    if (!parser) return 0;

    switch (parser->state) {

        /* --------------------------------------------------------
         * 状态: 等待起始帧 0x01
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_SOF:
            if (byte == TF_SOF) {
                /* 开始新帧 */
                memset(parser->head_buf, 0, TF_HEADER_LEN);
                parser->head_buf[0] = byte;
                parser->head_idx = 1;
                parser->state = TF_STATE_WAIT_ID_H;
            }
            /* 非 SOF 字节直接丢弃 */
            return 0;

        /* --------------------------------------------------------
         * 状态: 接收帧ID高字节
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_ID_H:
            parser->head_buf[parser->head_idx++] = byte;
            parser->frame_id = (uint16_t)byte << 8;
            parser->state = TF_STATE_WAIT_ID_L;
            return 0;

        /* --------------------------------------------------------
         * 状态: 接收帧ID低字节
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_ID_L:
            parser->head_buf[parser->head_idx++] = byte;
            parser->frame_id |= byte;
            parser->state = TF_STATE_WAIT_LEN_H;
            return 0;

        /* --------------------------------------------------------
         * 状态: 接收数据长度高字节
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_LEN_H:
            parser->head_buf[parser->head_idx++] = byte;
            parser->data_len = (uint16_t)byte << 8;
            parser->state = TF_STATE_WAIT_LEN_L;
            return 0;

        /* --------------------------------------------------------
         * 状态: 接收数据长度低字节
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_LEN_L:
            parser->head_buf[parser->head_idx++] = byte;
            parser->data_len |= byte;

            /* 验证数据长度 */
            if (parser->data_len > TF_MAX_DATA_LEN) {
                /* 数据长度异常，复位 */
                parser->error_count++;
                parser->state = TF_STATE_WAIT_SOF;
                return 0;
            }
            parser->state = TF_STATE_WAIT_TYPE_H;
            return 0;

        /* --------------------------------------------------------
         * 状态: 接收帧类型高字节
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_TYPE_H:
            parser->head_buf[parser->head_idx++] = byte;
            parser->frame_type = (uint16_t)byte << 8;
            parser->state = TF_STATE_WAIT_TYPE_L;
            return 0;

        /* --------------------------------------------------------
         * 状态: 接收帧类型低字节
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_TYPE_L:
            parser->head_buf[parser->head_idx++] = byte;
            parser->frame_type |= byte;
            parser->state = TF_STATE_WAIT_HEAD_CKSUM;
            return 0;

        /* --------------------------------------------------------
         * 状态: 接收头校验和
         * HEAD_CKSUM 覆盖 head_buf[0..6] (SOF~TYPE共7字节)
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_HEAD_CKSUM:
            parser->head_cksum = byte;
            parser->calc_head_cksum = tf_calc_cksum(parser->head_buf, 7);

            if (parser->calc_head_cksum != parser->head_cksum) {
                /* 头校验失败 */
                parser->error_count++;

                /* 如果当前字节恰好是 0x01，可能是帧边界错位，尝试从此重新开始 */
                if (byte == TF_SOF) {
                    memset(parser->head_buf, 0, TF_HEADER_LEN);
                    parser->head_buf[0] = byte;
                    parser->head_idx = 1;
                    parser->state = TF_STATE_WAIT_ID_H;
                } else {
                    parser->state = TF_STATE_WAIT_SOF;
                }
                return 0;
            }

            /* 头校验通过，准备接收数据 */
            parser->data_idx = 0;
            if (parser->data_len > 0) {
                parser->state = TF_STATE_WAIT_DATA;
            } else {
                /* 无数据负载，直接跳到数据校验和 */
                parser->state = TF_STATE_WAIT_DATA_CKSUM;
                parser->calc_data_cksum = tf_calc_cksum(NULL, 0);
            }
            return 0;

        /* --------------------------------------------------------
         * 状态: 接收数据负载
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_DATA:
            parser->data_buf[parser->data_idx++] = byte;

            if (parser->data_idx >= parser->data_len) {
                parser->state = TF_STATE_WAIT_DATA_CKSUM;
            }
            return 0;

        /* --------------------------------------------------------
         * 状态: 接收数据校验和
         * -------------------------------------------------------- */
        case TF_STATE_WAIT_DATA_CKSUM:
            parser->data_cksum = byte;
            parser->calc_data_cksum = tf_calc_cksum(parser->data_buf,
                                                      parser->data_len);

            /* 准备输出帧 */
            {
                tf_frame_t frame;
                frame.frame_id   = parser->frame_id;
                frame.data_len   = parser->data_len;
                frame.frame_type = parser->frame_type;
                memcpy(frame.data, parser->data_buf, parser->data_len);
                frame.head_cksum_ok = 1;  /* 已验证通过 */
                frame.data_cksum_ok = (parser->calc_data_cksum == parser->data_cksum);

                if (frame.data_cksum_ok) {
                    parser->frame_rx_count++;
                } else {
                    parser->error_count++;
                }

                /* 调用回调 */
                if (parser->on_frame) {
                    parser->on_frame(&frame);
                }
            }

            /* 回退到等待下一帧 */
            parser->state = TF_STATE_WAIT_SOF;
            return 1;

        /* --------------------------------------------------------
         * 意外状态 (不应发生)
         * -------------------------------------------------------- */
        default:
            parser->state = TF_STATE_WAIT_SOF;
            return 0;
    }
}
