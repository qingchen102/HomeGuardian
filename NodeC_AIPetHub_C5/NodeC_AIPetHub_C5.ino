// NodeC_AIPetHub_C5.ino
#include <Arduino.h>
#include <WiFi.h>
#include "Audio_Config.h"
#include "Audio_Pipeline.h"
#include "Display_UI.h"

enum PetState {
    STATE_IDLE,       
    STATE_LISTENING,  
    STATE_THINKING,   
    STATE_SPEAKING    
};

PetState currentState = STATE_IDLE;
String recognizedText = ""; 
String llmResponse = "";    

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=========================================");
    Serial.println("Node C: AI 语音宠物 (全屏触控+上滑取消版)");
    Serial.println("=========================================");

    // 1. 黄金时序调整：上电瞬间，先亮屏，画上断网红叉，进入发呆表情
    Init_Display();
    Draw_WiFi_Icon(false);
    currentState = STATE_IDLE;
    Update_Display_State(currentState); 

    // 2. 然后再去连接 Wi-Fi，此时屏幕已经是活的了
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[Wi-Fi] 正在连接...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[Wi-Fi] 连接成功! IP: " + WiFi.localIP().toString());
    
    // 连上后，右上角红叉瞬间变绿格！
    Draw_WiFi_Icon(true);

    // 3. 最后初始化音频系统
    Init_Audio_System();
    
    Serial.println("\n[系统] 准备就绪！请【按住屏幕下半部】开始说话，【上滑】取消，【松开】发送！");
}

void loop() {
    switch (currentState) {
        case STATE_IDLE: {
            int tx = 0, ty = 0;
            // 🚨 核心修复：调用全新的 Get_Touch_State 函数，同时获取按压状态和坐标
            if (Get_Touch_State(tx, ty)) {
                delay(30); // 触控防抖
                if (Get_Touch_State(tx, ty)) {
                    Serial.println("\n[宠物] 屏幕已被按住！开始倾听...");
                    currentState = STATE_LISTENING;
                    Update_Display_State(currentState); 
                }
            }
            break;
        }

        case STATE_LISTENING:
            // 这里的录音函数已经具备了上滑检测和 300ms 防抖能力
            recognizedText = Speech_To_Text();
            
            if (recognizedText != "") {
                Serial.println("[听觉] 听到: " + recognizedText);
                currentState = STATE_THINKING;
                Update_Display_State(currentState);
            } else {
                Serial.println("[听觉] 已取消、录音太短或未识别，返回待机.");
                currentState = STATE_IDLE;
                Update_Display_State(currentState);
            }
            break;

        case STATE_THINKING:
            llmResponse = Ask_LLM(recognizedText); 
            
            if (llmResponse != "") {
                Serial.println("[大脑] LLM 回复: " + llmResponse);
                currentState = STATE_SPEAKING;
                Update_Display_State(currentState);
            } else {
                Serial.println("[大脑] 思考失败，网络可能波动了.");
                currentState = STATE_IDLE;
                Update_Display_State(currentState);
            }
            break;

        case STATE_SPEAKING:
            Text_To_Speech_Play(llmResponse);
            currentState = STATE_IDLE;
            Update_Display_State(currentState);
            Serial.println("\n[系统] 播报完毕！可以进行下一次触屏对讲。");
            break;
    }

    delay(10); 
}