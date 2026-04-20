#ifndef BT_AUDIO_H
#define BT_AUDIO_H

#include <Arduino.h>

void btAudioInit();
void btAudioStart();
void btAudioEnd();
void btAudioLoop();
bool btAudioIsConnected();
const char* btAudioGetDeviceName();
void btAudioPlayPause();
void btAudioNext();
void btAudioPrev();
void btAudioStop();
void btAudioClearMetadata();

#endif
