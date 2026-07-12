// Audio_Pipeline.h
#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include <Arduino.h>

void Init_Audio_System();
String Speech_To_Text();

// 纯粹的通用 LLM 接口
String Ask_LLM(String prompt); 

void Text_To_Speech_Play(String text);

#endif