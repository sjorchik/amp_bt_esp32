/**
 * @file main.cpp
 * @brief ESP32 Audio Controller - TDA7318 + UDA1334A + ST7789 + Bluetooth
 * 
 * Головна програма з підтримкою:
 * - TDA7318 (аудіопроцесор)
 * - Енкодер, кнопки, IR RC-5
 * - Serial термінал
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "tda7318.h"
#include "input.h"
#include "display.h"
#include "terminal.h"
#include "bt_audio.h"

// ============================================================================
// SETUP
// ============================================================================

uint32_t lastEncoderActivity = 0;
static const uint32_t ENCODER_TIMEOUT_MS = 10000;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Увімкнути підсилювач (GPIO4 = STBY)
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
    delay(100);
    
    Serial.println("");
    Serial.println("+--------------------------------------+");
    Serial.println("|   ESP32 Audio Controller v0.2          |");
    Serial.println("|   TDA7318 + Inputs (Encoder/Buttons/IR)|");
    Serial.println("+--------------------------------------+");
    Serial.println("");
    Serial.println("[SYSTEM] Запуск...");
    Serial.println("[SYSTEM] Підсилювач увімкнено (GPIO4 HIGH)");
    Serial.printf("[SYSTEM] Free heap: %d байт\n", ESP.getFreeHeap());
    
    // Ініціалізація TDA7318
    Serial.println("[TDA7318] Ініціалізація...");
    if (tda7318Init()) {
        Serial.println("[TDA7318] Успішно!");
        tda7318PrintState();
    } else {
        Serial.println("[TDA7318] ПОМИЛКА ініціалізації!");
    }
    
    // Ініціалізація вводу
    Serial.println("[INPUT] Ініціалізація...");
    inputInit();
    Serial.println("[INPUT] Параметр за замовчуванням: " + String(inputGetParameter()) + " - " + inputGetParameterName());
    
    // Ініціалізація дисплея
    Serial.println("[DISPLAY] Ініціалізація...");
    displayInit();
    
    // Запуск в режимі очікування
    Serial.println("[SYSTEM] Запуск в режимі очікування...");
    tda7318SetMute(true);
    displaySetStandby(true);
    
    // Ініціалізація терміналу
    terminalInit();
    
    // Ініціалізація Bluetooth Audio
    Serial.println("[BT_AUDIO] Ініціалізація...");
    btAudioInit();
    
    // Показати початковий стан на дисплеї
    displayUpdateInput(tda7318GetInput());
    displayUpdateValue(tda7318GetVolume(), 63);
    
    lastEncoderActivity = millis();
    
    Serial.println("");
    Serial.println("[SYSTEM] Готово! Введіть 'help' для списку команд");
    Serial.println("");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    btAudioLoop();
    terminalHandleInput();
    
    InputEvent event = inputPoll();
    if (event.type != EVENT_NONE) {
        static uint32_t lastPwrTime = 0;
        uint32_t now = millis();
        
        bool isPwrBtn = (event.type == EVENT_BUTTON_PRESS && event.value == BTN_FUNC_PWR);
        bool isIrPwr = (event.type == EVENT_IR_COMMAND && (event.value == 103 || event.value == 0x0C || event.value == 0x80C || event.value == 0x100C));
        
        if (isPwrBtn || isIrPwr) {
            if (now - lastPwrTime > 500) {
                lastPwrTime = now;
                handleInputEvent(event);
            }
        } else if (!inputIsStandby()) {
            handleInputEvent(event);
            
            if (event.type == EVENT_ENCODER_ROTATE || event.type == EVENT_ENCODER_CLICK) {
                lastEncoderActivity = millis();
            }
        }
    }
    
    displayLoop();
    
    if (!inputIsStandby() && inputGetParameter() != PARAM_VOLUME) {
        if (millis() - lastEncoderActivity > ENCODER_TIMEOUT_MS) {
            inputSetParameter(PARAM_VOLUME);
            displayUpdateValue(tda7318GetVolume(), 63);
            Serial.println("[INPUT] Таймаут - скинуто на гучність");
        }
    }
    
    delay(1);
}
