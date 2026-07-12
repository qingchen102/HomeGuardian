// Display_UI.cpp
#include "Display_UI.h"
#include "Audio_Config.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> 

#define ST77XX_LIGHTGREY 0xC618
#define PWR_CTRL 5 

Adafruit_ST7789 tft = Adafruit_ST7789(LCD_CS, LCD_DC, LCD_RST);

int last_state = -1; 
bool current_wifi_state = false; 

// 🚨 核心修复：将状态锁从 bool 改为 int，允许使用 -1 作为强制重置信号
int last_cancel_state = -1; 

// ==========================================
// 1. 初始化屏幕与触摸硬件
// ==========================================
void Init_Display() {
    Serial.println("[UI] 正在初始化 LCD 屏幕与触摸硬件...");
    
    pinMode(PWR_CTRL, OUTPUT);
    digitalWrite(PWR_CTRL, LOW); 
    delay(100); 
    
    SPI.begin(LCD_SCLK, -1, LCD_MOSI, -1);
    
    tft.init(240, 320); 
    tft.invertDisplay(true); 
    tft.setRotation(1);      
    tft.fillScreen(ST77XX_BLACK); 
    
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Serial.println("[UI] 🖥️ 屏幕与触摸系统秒速上线！");
}

// ==========================================
// 2. I2C 实时读取纯净触控坐标
// ==========================================
bool Get_Touch_State(int &x, int &y) {
    Wire.beginTransmission(0x15); 
    Wire.write(0x02); 
    if (Wire.endTransmission(false) == 0) {
        Wire.requestFrom(0x15, 5); 
        if (Wire.available() >= 5) {
            uint8_t fingers = Wire.read() & 0x0F;
            uint8_t x_h = Wire.read() & 0x0F;
            uint8_t x_l = Wire.read();
            uint8_t y_h = Wire.read() & 0x0F;
            uint8_t y_l = Wire.read();
            
            if (fingers > 0) {
                x = (x_h << 8) | x_l;
                y = (y_h << 8) | y_l;
                return true; 
            }
        }
    }
    return false;
}

// ==========================================
// 3. 绘制右上角 Wi-Fi 阶梯信号
// ==========================================
void Draw_WiFi_Icon(bool connected) {
    current_wifi_state = connected;
    int x = 280; 
    int y = 10;
    
    tft.fillRect(x, y, 24, 20, ST77XX_BLACK);
    
    if (connected) {
        tft.fillRect(x, y+12, 4, 4, ST77XX_GREEN);
        tft.fillRect(x+6, y+8, 4, 8, ST77XX_GREEN);
        tft.fillRect(x+12, y+4, 4, 12, ST77XX_GREEN);
        tft.fillRect(x+18, y, 4, 16, ST77XX_GREEN);
    } else {
        tft.fillRect(x, y+12, 4, 4, ST77XX_LIGHTGREY);
        tft.fillRect(x+6, y+8, 4, 8, ST77XX_LIGHTGREY);
        tft.fillRect(x+12, y+4, 4, 12, ST77XX_LIGHTGREY);
        tft.fillRect(x+18, y, 4, 16, ST77XX_LIGHTGREY);
        tft.drawLine(x, y, x+22, y+16, ST77XX_RED);
        tft.drawLine(x, y+16, x+22, y, ST77XX_RED);
    }
}

// ==========================================
// 4. 动态绘制“取消”提示区域 (完美居中排版)
// ==========================================
void Draw_Cancel_Warning(bool is_canceling) {
    int current_state = is_canceling ? 1 : 0;
    
    // 如果状态没有改变，且不是 -1 的强制重置信号，则跳过防止闪烁
    if (current_state == last_cancel_state) return; 
    last_cancel_state = current_state;
    
    // 覆盖宽度拉满到 320，确保旧文字被彻底擦除
    tft.fillRect(0, 180, 320, 60, ST77XX_BLACK); 
    tft.setTextSize(2);
    
    if (is_canceling) {
        tft.setTextColor(ST77XX_RED);
        tft.setCursor(45, 200); 
        tft.print("Release to CANCEL!");
    } else {
        tft.setTextColor(ST77XX_LIGHTGREY);
        tft.setCursor(55, 200); 
        tft.print("Slide Bottom-Left"); 
    }
}

// ==========================================
// 5. 根据系统状态机刷新表情
// ==========================================
void Update_Display_State(int state) {
    if (state == last_state) return; 
    last_state = state;
    
    tft.fillScreen(ST77XX_BLACK); 
    Draw_WiFi_Icon(current_wifi_state);
    
    switch(state) {
        case 0: // STATE_IDLE
            tft.setTextColor(ST77XX_WHITE);
            tft.setTextSize(4);
            tft.setCursor(40, 80); 
            tft.print("( - _ - ) zZ");
            tft.setTextSize(2);
            tft.setTextColor(ST77XX_LIGHTGREY);
            tft.setCursor(100, 150);
            tft.print("Sleeping...");
            break;
            
        case 1: // STATE_LISTENING
            tft.setTextColor(ST77XX_CYAN); 
            tft.setTextSize(4);
            tft.setCursor(60, 80);
            tft.print("( o _ o )");
            tft.setTextSize(2);
            tft.setCursor(100, 150);
            tft.print("Listening...");
            
            // 🚨 终极修复：强行重置状态锁为 -1，保证灰字提示绝对在第一时间浮现
            last_cancel_state = -1; 
            Draw_Cancel_Warning(false);
            break;
            
        case 2: // STATE_THINKING
            tft.setTextColor(ST77XX_MAGENTA); 
            tft.setTextSize(4);
            tft.setCursor(60, 80);
            tft.print("( > _ < )");
            tft.setTextSize(2);
            tft.setCursor(100, 150);
            tft.print("Thinking...");
            break;
            
        case 3: // STATE_SPEAKING
            tft.setTextColor(ST77XX_GREEN); 
            tft.setTextSize(4);
            tft.setCursor(60, 80);
            tft.print("( ^ O ^ )");
            tft.setTextSize(2);
            tft.setCursor(100, 150);
            tft.print("Speaking...");
            break;
    }
}