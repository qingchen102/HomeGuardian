/**
 * @file    tinyframe.h
 * @brief   TinyFrame 协议解析器 (ESP32-C5 版)
 *
 * 与 STM32 版本协议逻辑完全一致，API 略有调整以适应 ESP-IDF。
 */

#ifndef __TINYFRAME_H__
#define __TINYFRAME_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TF_SOF                  0x01
#define TF_HEADER_LEN           8
#define TF_MAX_DATA_LEN         32

typedef enum {
    TF_STATE_WAIT_SOF = 0,
    TF_STATE_WAIT_ID_H,
    TF_STATE_WAIT_ID_L,
    TF_STATE_WAIT_LEN_H,
    TF_STATE_WAIT_LEN_L,
    TF_STATE_WAIT_TYPE_H,
    TF_STATE_WAIT_TYPE_L,
    TF_STATE_WAIT_HEAD_CKSUM,
    TF_STATE_WAIT_DATA,
    TF_STATE_WAIT_DATA_CKSUM,
} tf_state_t;

typedef struct {
    uint16_t frame_id;
    uint16_t data_len;
    uint16_t frame_type;
    uint8_t  data[TF_MAX_DATA_LEN];
    bool     head_cksum_ok;
    bool     data_cksum_ok;
} tf_frame_t;

typedef void (*tf_frame_callback_t)(const tf_frame_t *frame, void *arg);

typedef struct {
    tf_state_t state;
    uint16_t   frame_id;
    uint16_t   data_len;
    uint16_t   frame_type;
    uint8_t    head_buf[TF_HEADER_LEN];
    uint8_t    head_idx;
    uint8_t    data_buf[TF_MAX_DATA_LEN];
    uint8_t    data_idx;
    uint8_t    head_cksum;
    uint8_t    calc_head_cksum;
    uint8_t    data_cksum;
    uint8_t    calc_data_cksum;
    uint32_t   frame_rx_count;
    uint32_t   error_count;
    tf_frame_callback_t on_frame;
    void      *callback_arg;
} tf_parser_t;

/* API */
void tf_init(tf_parser_t *parser, tf_frame_callback_t cb, void *arg);
bool tf_feed_byte(tf_parser_t *parser, uint8_t byte);
void tf_feed_bytes(tf_parser_t *parser, const uint8_t *data, size_t len);
uint8_t tf_calc_cksum(const uint8_t *data, uint8_t len);
float tf_bytes_to_float(const uint8_t *bytes);
uint32_t tf_bytes_to_uint32(const uint8_t *bytes);
const char *tf_type_name(uint16_t frame_type);

#ifdef __cplusplus
}
#endif

#endif /* __TINYFRAME_H__ */
