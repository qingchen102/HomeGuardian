/**
 * @file    tinyframe.c
 * @brief   TinyFrame 协议解析实现 (ESP-IDF 版)
 */

#include "tinyframe.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "tinyframe";

/* ================================================================
 * Checksum
 * ================================================================ */

uint8_t tf_calc_cksum(const uint8_t *data, uint8_t len)
{
    uint8_t ret = 0;
    for (uint8_t i = 0; i < len; i++) {
        ret ^= data[i];
    }
    return ~ret;
}

/* ================================================================
 * Data conversion (little-endian)
 * ================================================================ */

float tf_bytes_to_float(const uint8_t *bytes)
{
    uint32_t raw = (uint32_t)bytes[0]
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
 * Parser init
 * ================================================================ */

void tf_init(tf_parser_t *parser, tf_frame_callback_t cb, void *arg)
{
    if (!parser) return;
    memset(parser, 0, sizeof(tf_parser_t));
    parser->state = TF_STATE_WAIT_SOF;
    parser->on_frame = cb;
    parser->callback_arg = arg;
}

/* ================================================================
 * Feed a single byte into the parser state machine
 * ================================================================ */

bool tf_feed_byte(tf_parser_t *parser, uint8_t byte)
{
    if (!parser) return false;

    switch (parser->state) {

        case TF_STATE_WAIT_SOF:
            if (byte == TF_SOF) {
                memset(parser->head_buf, 0, TF_HEADER_LEN);
                parser->head_buf[0] = byte;
                parser->head_idx = 1;
                parser->state = TF_STATE_WAIT_ID_H;
            }
            return false;

        case TF_STATE_WAIT_ID_H:
            parser->head_buf[parser->head_idx++] = byte;
            parser->frame_id = (uint16_t)byte << 8;
            parser->state = TF_STATE_WAIT_ID_L;
            return false;

        case TF_STATE_WAIT_ID_L:
            parser->head_buf[parser->head_idx++] = byte;
            parser->frame_id |= byte;
            parser->state = TF_STATE_WAIT_LEN_H;
            return false;

        case TF_STATE_WAIT_LEN_H:
            parser->head_buf[parser->head_idx++] = byte;
            parser->data_len = (uint16_t)byte << 8;
            parser->state = TF_STATE_WAIT_LEN_L;
            return false;

        case TF_STATE_WAIT_LEN_L:
            parser->head_buf[parser->head_idx++] = byte;
            parser->data_len |= byte;
            if (parser->data_len > TF_MAX_DATA_LEN) {
                parser->error_count++;
                parser->state = TF_STATE_WAIT_SOF;
                return false;
            }
            parser->state = TF_STATE_WAIT_TYPE_H;
            return false;

        case TF_STATE_WAIT_TYPE_H:
            parser->head_buf[parser->head_idx++] = byte;
            parser->frame_type = (uint16_t)byte << 8;
            parser->state = TF_STATE_WAIT_TYPE_L;
            return false;

        case TF_STATE_WAIT_TYPE_L:
            parser->head_buf[parser->head_idx++] = byte;
            parser->frame_type |= byte;
            parser->state = TF_STATE_WAIT_HEAD_CKSUM;
            return false;

        case TF_STATE_WAIT_HEAD_CKSUM:
            parser->head_cksum = byte;
            parser->calc_head_cksum = tf_calc_cksum(parser->head_buf, 7);

            if (parser->calc_head_cksum != parser->head_cksum) {
                parser->error_count++;
                /* Try to resync if this byte is SOF */
                if (byte == TF_SOF) {
                    memset(parser->head_buf, 0, TF_HEADER_LEN);
                    parser->head_buf[0] = byte;
                    parser->head_idx = 1;
                    parser->state = TF_STATE_WAIT_ID_H;
                } else {
                    parser->state = TF_STATE_WAIT_SOF;
                }
                return false;
            }

            parser->data_idx = 0;
            parser->state = (parser->data_len > 0)
                ? TF_STATE_WAIT_DATA
                : TF_STATE_WAIT_DATA_CKSUM;
            return false;

        case TF_STATE_WAIT_DATA:
            parser->data_buf[parser->data_idx++] = byte;
            if (parser->data_idx >= parser->data_len) {
                parser->state = TF_STATE_WAIT_DATA_CKSUM;
            }
            return false;

        case TF_STATE_WAIT_DATA_CKSUM:
            parser->data_cksum = byte;
            parser->calc_data_cksum = tf_calc_cksum(parser->data_buf,
                                                      parser->data_len);

            {
                tf_frame_t frame;
                frame.frame_id   = parser->frame_id;
                frame.data_len   = parser->data_len;
                frame.frame_type = parser->frame_type;
                memcpy(frame.data, parser->data_buf, parser->data_len);
                frame.head_cksum_ok = true;
                frame.data_cksum_ok = (parser->calc_data_cksum == parser->data_cksum);

                if (frame.data_cksum_ok) {
                    parser->frame_rx_count++;
                } else {
                    parser->error_count++;
                }

                if (parser->on_frame) {
                    parser->on_frame(&frame, parser->callback_arg);
                }
            }

            parser->state = TF_STATE_WAIT_SOF;
            return true;

        default:
            parser->state = TF_STATE_WAIT_SOF;
            return false;
    }
}

/* ================================================================
 * Feed multiple bytes at once
 * ================================================================ */

void tf_feed_bytes(tf_parser_t *parser, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        tf_feed_byte(parser, data[i]);
    }
}
