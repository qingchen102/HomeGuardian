// Display_UI.h
#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>

void Init_Display();
void Update_Display_State(int state);
void Draw_WiFi_Icon(bool connected);

// 回归纯净的坐标读取接口
bool Get_Touch_State(int &x, int &y);

void Draw_Cancel_Warning(bool is_canceling);

#endif