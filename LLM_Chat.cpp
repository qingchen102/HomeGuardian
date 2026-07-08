#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "LLM_Chat.h"
#include "Config.h" // 引入全局配置，获取 SSID 和 API_KEY

// ==========================================
// 初始化 Wi-Fi 连接
// ==========================================
void Init_WiFi() {
    Serial.print("[Wi-Fi] 正在连接到: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println();
    Serial.println("[Wi-Fi] 📶 连接成功！");
    Serial.print("[Wi-Fi] IP地址: ");
    Serial.println(WiFi.localIP());
}

// ==========================================
// 向云端大模型提问并获取回复
// ==========================================
String Ask_LLM(String prompt) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[LLM] ❌ Wi-Fi断开，无法连接大模型！");
        return "网络错误，请检查连接。";
    }

    HTTPClient http;
    // 火山引擎 API 地址
    String api_url = "https://ark.cn-beijing.volces.com/api/v3/chat/completions"; 
    
    Serial.println("[LLM] 大脑正在思考中...");
    
    http.begin(api_url);
    http.addHeader("Content-Type", "application/json");
    
    // 拼接 API 密钥
    String auth_header = String("Bearer ") + LLM_API_KEY;
    http.addHeader("Authorization", auth_header);

    // 构造请求 JSON (ArduinoJson 7.x 语法)
    JsonDocument doc; 
    
    // 注意：这里需要替换为你自己在火山引擎创建的接入点模型 ID
    doc["model"] = "ep-xxxxxxxx-xxx"; 
    
    JsonArray messages = doc["messages"].to<JsonArray>();
    JsonObject msg1 = messages.add<JsonObject>();
    msg1["role"] = "user";
    msg1["content"] = prompt; 

    String requestBody;
    serializeJson(doc, requestBody); 

    // 发送 POST 请求
    int httpResponseCode = http.POST(requestBody);
    String responseString = "";

    if (httpResponseCode > 0) {
        String responseBody = http.getString();
        
        JsonDocument responseDoc;
        DeserializationError error = deserializeJson(responseDoc, responseBody);
        
        if (!error) {
            // 提取大模型回复的文本内容
            const char* answer = responseDoc["choices"][0]["message"]["content"];
            responseString = String(answer);
            Serial.println("[LLM] 收到回复: ");
            Serial.println(responseString);
        } else {
            Serial.println("[LLM] ❌ JSON 解析失败");
        }
    } else {
        Serial.print("[LLM] ❌ 请求失败，网络错误码: ");
        Serial.println(httpResponseCode);
    }
    
    http.end(); 
    return responseString;
}