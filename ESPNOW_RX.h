#ifndef ESPNOW_RX_H
#define ESPNOW_RX_H

void Init_ESPNOW();

// 全局标志位：用于通知主线程 Core 0 发生了跌倒报警
extern volatile bool flag_FallDetected; 

#endif