#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "config.h"

void displayInit();
void displayUpdateVolume(uint8_t volume);
void displayUpdateInput(AudioInput input);
void displayUpdateParameter(Parameter param, int16_t value);
void displayShowMessage(const char* message);
void displaySetStandby(bool standby);
void displaySetBTConnected(bool connected);

#endif
