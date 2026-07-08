// src/LLM_Chat.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "LLM_Chat.h"
#include "Config.h" // 获取 Wi-Fi 密码和 API_KEY

// ==========================================
// 函数：初始化 Wi-Fi 连接
// ==========================================
void Init_WiFi() {
    Serial.print("[Wi-Fi] 正在连接到: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // 等待连接成功
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
// 函数：向云端大模型 (如豆包) 提问并获取回复
// 参数：prompt (你想要对大模型说的话)
// 返回值：大模型生成的文本回复
// ==========================================
String Ask_LLM(String prompt) {
    // 检查网络是否正常
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[LLM] ❌ Wi-Fi断开，无法连接大模型！");
        return "网络错误";
    }

    HTTPClient http;
    
    // 这里以豆包 (Volcengine) 的 API 接口为例
    // 注意：这里的 URL 后期需要替换为你自己申请的模型接入点 URL
    String api_url = "https://ark.cn-beijing.volces.com/api/v3/chat/completions"; 
    
    Serial.println("[LLM] 正在思考中...");
    
    http.begin(api_url);
    
    // 配置 HTTP 请求头 (告诉服务器我们发的是 JSON，并带上钥匙)
    http.addHeader("Content-Type", "application/json");
    String auth_header = String("Bearer ") + LLM_API_KEY;
    http.addHeader("Authorization", auth_header);

    // 构造发给大模型的 JSON 数据包 (使用 ArduinoJson 库)
    // 官方规定格式：{"model": "模型ID", "messages": [{"role": "user", "content": "问题"}]}
    JsonDocument doc; 
    doc["model"] = "ep-xxxxxxxx-xxx"; // 这里的模型 ID 后期需要替换
    
    JsonArray messages = doc["messages"].to<JsonArray>();
    JsonObject msg1 = messages.add<JsonObject>();
    msg1["role"] = "user";
    msg1["content"] = prompt; // 将我们传进来的问题塞进去

    String requestBody;
    serializeJson(doc, requestBody); // 将 JSON 对象压缩成字符串

    // 发送 POST 请求
    int httpResponseCode = http.POST(requestBody);
    String responseString = "";

    // 检查服务器的响应状态
    if (httpResponseCode > 0) {
        String responseBody = http.getString();
        
        // 解析大模型返回的复杂 JSON 结果
        JsonDocument responseDoc;
        DeserializationError error = deserializeJson(responseDoc, responseBody);
        
        if (!error) {
            // 从多层嵌套的 JSON 中提取大模型回复的文字
            // 路径: choices -> [0] -> message -> content
            const char* answer = responseDoc["choices"][0]["message"]["content"];
            responseString = String(answer);
            Serial.println("[LLM] 收到回复: ");
            Serial.println(responseString);
        } else {
            Serial.println("[LLM] ❌ JSON 解析失败");
        }
    } else {
        Serial.print("[LLM] ❌ 请求失败，错误码: ");
        Serial.println(httpResponseCode);
    }
    
    http.end(); // 释放资源
    return responseString;
}