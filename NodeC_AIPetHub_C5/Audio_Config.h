// Audio_Config.h
// 适配 ESP-SensairShuttle-MainBoard V1.0 (2025-12-16)
#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

// ==========================================
// 1. 网络与云端 API 密钥配置
// ==========================================
#define WIFI_SSID       "iQOONeo10"
#define WIFI_PASSWORD   "m20051020"

#define MQTT_BROKER     "your_mqtt_broker_address"
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "PetHub_C5"

// --- LLM API 配置 (通用大语言模型接口) ---
#define LLM_API_KEY     "sk-ws-H.EMPLELD.9T1n.MEUCIAFwtLwhYfAQ3SBLJdaiWGJcakMa6DBPsHiJupfX2rxXAiEA_9ZMXLP2wfpIMf3bVFbAcbAwDBcsbXXHRv3MQRNDhHo" 
#define LLM_API_URL     "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"
#define LLM_MODEL_ID    "qwen-math-turbo"

// ==========================================
// 2. 屏幕与触摸引脚配置 
// ==========================================
#define LCD_MOSI        23  
#define LCD_SCLK        24  
#define LCD_CS          25  
#define LCD_DC          26  
#define LCD_RST         -1  

#define TOUCH_SCL       3   
#define TOUCH_SDA       2   

// ==========================================
// 3. 氛围灯与外部接口
// ==========================================
#define PIN_RGB_LED     27  
#define PIN_EXT_IO1     4   
#define PIN_EXT_IO2     5   

// ==========================================
// 4. 音频系统通道
// ==========================================
#define PIN_MIC_ADC     6   // 真实的物理 ADC 麦克风引脚
#define PIN_PDM_TX      12  

// ==========================================
// 5. 语音合成 (TTS) 通用配置
// ==========================================
#define TTS_API_URL          "https://nls-gateway-cn-shanghai.aliyuncs.com/stream/v1/tts" 
#define TTS_AUTH_HEADER      ""
#define TTS_PAYLOAD_TEMPLATE "{\"appkey\":\"u2uE6AXcYd2NJaET\",\"token\":\"91db3e7422f64b27b7e9aefcfd2798b7\",\"text\":\"<TEXT>\",\"format\":\"pcm\",\"sample_rate\":16000,\"voice\":\"xiaoyun\"}"

// ==========================================
// 6. 语音识别 (STT) 通用配置 (新增)
// ==========================================
// 填入你选择的云端 STT (语音转文字) 服务的短连接 API 地址
#define STT_API_URL          "https://nls-gateway-cn-shanghai.aliyuncs.com/stream/v1/asr?appkey=u2uE6AXcYd2NJaET&format=pcm&sample_rate=16000&enable_punctuation_prediction=true" 

// ASR 的 Token (请求头中必须使用 X-NLS-Token)
#define STT_AUTH_HEADER      "91db3e7422f64b27b7e9aefcfd2798b7"

#endif