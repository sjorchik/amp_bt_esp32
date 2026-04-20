#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "config.h"

void displayInit();
void displayUpdateInput(AudioInput input);
void displayUpdateValue(int16_t value, int16_t maxVal);
void displayShowParam(const char* label, int16_t value, uint32_t timeoutMs);
void displaySetStandby(bool standby);
void displaySetMute(bool muted);
void displaySetBTConnected(bool connected);
void displayUpdateBTTrackInfo(const char* artist, const char* title, const char* album);
void displaySetBTPlaying(bool playing);
void displayLoop();

#endif
