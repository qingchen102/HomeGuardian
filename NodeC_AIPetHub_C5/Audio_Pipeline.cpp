// Audio_Pipeline.cpp
#include "Audio_Pipeline.h"
#include "Audio_Config.h"
#include "Display_UI.h" 
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "driver/i2s_pdm.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"

i2s_chan_handle_t tx_handle = NULL;

// ==========================================
// 1. 音频底层初始化 
// ==========================================
void Init_Audio_System() {
    Serial.println("[Audio] 准备初始化物理喇叭...");

    pinMode(1, OUTPUT);
    digitalWrite(1, LOW); 
    delay(50); 

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; 
    i2s_new_channel(&chan_cfg, &tx_handle, NULL);

    i2s_pdm_tx_config_t pdm_cfg = {
        .clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(16000), 
        .slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), 
        .gpio_cfg = {
            .clk = GPIO_NUM_NC,
            .dout = (gpio_num_t)7, 
            .invert_flags = { .clk_inv = false }
        }
    };
    
    pdm_cfg.clk_cfg.up_sample_fs = 480; 
    pdm_cfg.slot_cfg.sd_scale = I2S_PDM_SIG_SCALING_MUL_1;
    pdm_cfg.slot_cfg.hp_scale = I2S_PDM_SIG_SCALING_MUL_1;
    pdm_cfg.slot_cfg.lp_scale = I2S_PDM_SIG_SCALING_MUL_1;
    pdm_cfg.slot_cfg.sinc_scale = I2S_PDM_SIG_SCALING_MUL_1;

    i2s_channel_init_pdm_tx_mode(tx_handle, &pdm_cfg);
    
    gpio_set_direction(GPIO_NUM_8, GPIO_MODE_OUTPUT);
    esp_rom_gpio_connect_out_signal(GPIO_NUM_8, I2SO_SD_OUT_IDX, 1, 0); 
    
    Serial.println("[Audio] 🔊 物理扬声器驱动加载完毕！");
}

// ==========================================
// 2. 大语言模型对话
// ==========================================
String Ask_LLM(String prompt) {
    if (WiFi.status() != WL_CONNECTED) return "网络断开。";

    HTTPClient http;
    http.begin(LLM_API_URL); 
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", LLM_API_KEY);
    http.addHeader("Connection", "close"); 

    JsonDocument doc;
    doc["model"] = LLM_MODEL_ID; 
    
    JsonArray messages = doc["messages"].to<JsonArray>();
    
    JsonObject systemMsg = messages.add<JsonObject>();
    systemMsg["role"] = "system";
    systemMsg["content"] = "你是陪伴老人的机器宠物。回复用温柔可爱的语气。要求：绝对不能超过20个字！不要表情符号！";
    
    JsonObject userMsg = messages.add<JsonObject>();
    userMsg["role"] = "user";
    userMsg["content"] = prompt;

    String requestBody;
    serializeJson(doc, requestBody);
    
    Serial.println("[LLM] 正在发送请求至云端大模型...");
    int responseCode = http.POST(requestBody);
    String answer = "";
    
    if (responseCode == 200) {
        String responseBody = http.getString();
        JsonDocument resDoc;
        if (!deserializeJson(resDoc, responseBody)) {
            answer = resDoc["choices"][0]["message"]["content"].as<String>();
        }
    } else {
        Serial.println("[LLM] HTTP 请求失败");
    }
    
    http.end();
    return answer;
}

// ==========================================
// 3. 真实物理录音并转文字 (无幽灵判定版)
// ==========================================
String Speech_To_Text() {
    Serial.println("[STT] 🎙️ 麦克风已开启，正在录音...");

    const int MAX_RECORD_BYTES = 16000 * 2 * 10;
    uint8_t* audio_buffer = (uint8_t*)heap_caps_malloc(MAX_RECORD_BYTES, MALLOC_CAP_SPIRAM);
    
    if (!audio_buffer) {
        Serial.println("[STT] 🚨 PSRAM 内存分配失败！");
        return "";
    }

    int recorded_bytes = 0;
    unsigned long next_sample_time = micros();
    
    unsigned long next_touch_time = millis(); 
    unsigned long last_touch_time = millis();
    bool is_canceling = false;
    
    int touch_x = 0, touch_y = 0;

    while (recorded_bytes < MAX_RECORD_BYTES) {
        unsigned long now_ms = millis();
        
        // 🚨 15 毫秒极速轮询屏幕坐标
        if (now_ms >= next_touch_time) {
            next_touch_time = now_ms + 15; 
            
            // 只有真正在屏幕上摸到了，才刷新坐标
            if (Get_Touch_State(touch_x, touch_y)) {
                last_touch_time = now_ms; 
                
                // 实时判断 Y 轴是否越过了 142 中线
                if (touch_y < 142) { 
                    is_canceling = true;
                    Draw_Cancel_Warning(true); 
                } else {
                    is_canceling = false;
                    Draw_Cancel_Warning(false); 
                }
            } else {
                // 彻底离开屏幕 300 毫秒后，跳出录音循环，锁定 is_canceling 的最后状态
                if (now_ms - last_touch_time > 300) {
                    Serial.println("\n[STT] 👆 手指已明确离开屏幕！");
                    break;
                }
            }
        }
        
        // 麦克风极速采样
        unsigned long now_us = micros();
        if (now_us >= next_sample_time) {
            next_sample_time = now_us + 62; 
            
            int raw_val = analogRead(PIN_MIC_ADC); 
            int32_t pcm_val = (raw_val - 2048) * 16;
            
            if (pcm_val > 32767) pcm_val = 32767;
            if (pcm_val < -32768) pcm_val = -32768;
            
            audio_buffer[recorded_bytes++] = pcm_val & 0xFF;        
            audio_buffer[recorded_bytes++] = (pcm_val >> 8) & 0xFF; 
        }
    }
    
    // 逻辑仲裁
    if (is_canceling) {
        Serial.println("[STT] 🛑 触发上滑取消！已销毁录音内存，静默退回待机。");
        free(audio_buffer);
        return ""; 
    }
    
    if (recorded_bytes < 16000 * 2 * 0.5) { 
        Serial.println("[STT] 🛑 录音时间太短，忽略！");
        free(audio_buffer);
        return "";
    }

    Serial.println("[STT] ✅ 录音成功，共截获 " + String(recorded_bytes) + " 字节声音数据。正在极速上传...");

    HTTPClient http;
    http.begin(STT_API_URL);
    http.addHeader("Content-Type", "application/octet-stream");
    http.addHeader("X-NLS-Token", STT_AUTH_HEADER);
    http.addHeader("Connection", "close");

    int httpCode = http.POST(audio_buffer, recorded_bytes);
    String resultText = "";
    
    if (httpCode == 200) {
        String response = http.getString();
        
        JsonDocument doc;
        if (!deserializeJson(doc, response)) {
            resultText = doc["result"].as<String>(); 
        }
    } else {
        Serial.print("[STT] 识别失败, 错误码: ");
        Serial.println(httpCode);
        Serial.println(http.getString());
    }

    http.end();
    free(audio_buffer); 
    
    return resultText;
}

// ==========================================
// 4. 文字转语音并播放 
// ==========================================
void Text_To_Speech_Play(String text) {
    if (WiFi.status() != WL_CONNECTED) return;
    
    Serial.println("[TTS] 请求云端合成语音...");
    HTTPClient http;
    http.begin(TTS_API_URL); 
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "close"); 
    
    String authHeader = String(TTS_AUTH_HEADER);
    if (authHeader.length() > 0) {
        http.addHeader("Authorization", authHeader);
    }

    String requestBody = String(TTS_PAYLOAD_TEMPLATE);
    requestBody.replace("<TEXT>", text); 

    int httpCode = http.POST(requestBody);

    if (httpCode == HTTP_CODE_OK) {
        Serial.println("[TTS] 开始极速接收音频流并直推...");
        
        i2s_channel_enable(tx_handle);
        
        const size_t silence_size = 1024;
        uint8_t silence[silence_size] = {0};
        size_t written = 0;
        i2s_channel_write(tx_handle, silence, silence_size, &written, portMAX_DELAY);
        
        delay(5);
        digitalWrite(1, HIGH);
        delay(5);
        
        WiFiClient *stream = http.getStreamPtr();
        
        uint8_t buff[1024]; 
        int buffer_idx = 0;
        int len = http.getSize();
        
        unsigned long last_data_time = millis(); 
        
        while (http.connected() && (len > 0 || len == -1)) {
            size_t size = stream->available();
            if (size > 0) {
                last_data_time = millis(); 
                
                int c = stream->read(&buff[buffer_idx], sizeof(buff) - buffer_idx);
                if (c > 0) {
                    buffer_idx += c;
                    int valid_bytes = buffer_idx - (buffer_idx % 2);
                    if (valid_bytes > 0) {
                        size_t bytes_written = 0;
                        i2s_channel_write(tx_handle, buff, valid_bytes, &bytes_written, portMAX_DELAY);
                        
                        if (buffer_idx > valid_bytes) {
                            buff[0] = buff[valid_bytes];
                            buffer_idx = 1;
                        } else {
                            buffer_idx = 0;
                        }
                    }
                    if (len > 0) len -= c;
                }
            } else {
                if (millis() - last_data_time > 1500) {
                    Serial.println("[TTS] 音频流接收空闲超时，主动切断连接！");
                    break; 
                }
                delay(1); 
            }
        }
        
        delay(30);
        i2s_channel_disable(tx_handle);
        digitalWrite(1, LOW);
        Serial.println("[TTS] 语音播报完毕！已瞬间切回待机！");
    } else {
        Serial.print("[TTS] HTTP 请求失败, 错误码: ");
        Serial.println(httpCode);
    }
    http.end();
}