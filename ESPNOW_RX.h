#ifndef ESPNOW_RX_H
#define ESPNOW_RX_H

void Init_ESPNOW();
// 此全局标志位用于通知主线程：沙发节点发来了跌倒报警！
extern volatile bool flag_FallDetected; 

#endif