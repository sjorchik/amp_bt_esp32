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

// ============================================================================
// КОМАНДНИЙ ТЕРМІНАЛ
// ============================================================================

#define CMD_BUFFER_SIZE 64
static char cmdBuffer[CMD_BUFFER_SIZE];
static uint8_t cmdIndex = 0;

void printHelp() {
    Serial.println("");
    Serial.println("========================================");
    Serial.println("  ESP32 Audio Controller - Команди");
    Serial.println("========================================");
    Serial.println("  help              - Допомога");
    Serial.println("  status            - Стан TDA7318");
    Serial.println("  vol <0-63>        - Гучність");
    Serial.println("  vol+/vol-         - +/- гучність");
    Serial.println("  bass <val>        - НЧ (-7..+7)");
    Serial.println("  treble <val>      - ВЧ (-7..+7)");
    Serial.println("  balance <val>     - Баланс (-31..+31)");
    Serial.println("  gain <0-3>        - Підсилення");
    Serial.println("  input <0-3>       - Вхід (0=BT,1=PC,2=TV,3=AUX)");
    Serial.println("  mute/unmute       - Mute");
    Serial.println("  param <0-4>       - Параметр енкодера");
    Serial.println("                    0=Vol,1=Bass,2=Tr,3=Bal,4=Gain");
    Serial.println("========================================");
    Serial.println("");
}

void executeCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;
    
    int spaceIndex = cmd.indexOf(' ');
    String command = spaceIndex > 0 ? cmd.substring(0, spaceIndex) : cmd;
    String arg = spaceIndex > 0 ? cmd.substring(spaceIndex + 1) : "";
    
    command.toLowerCase();
    
    if (command == "help" || command == "h" || command == "?") {
        printHelp();
        return;
    }
    
    if (command == "status" || command == "st") {
        tda7318PrintState();
        return;
    }
    
    // Параметр енкодера
    if (command == "param" || command == "p") {
        if (arg.length() == 0) {
            Serial.println("Поточний параметр: " + String(inputGetParameter()) + " - " + inputGetParameterName());
            return;
        }
        int val = arg.toInt();
        if (val < 0 || val >= PARAM_COUNT) {
            Serial.println("Помилка: 0-" + String(PARAM_COUNT - 1));
            return;
        }
        inputSetParameter((Parameter)val);
        Serial.println("Параметр встановлено: " + String(val) + " - " + inputGetParameterName());
        return;
    }
    
    // Гучність
    if (command == "vol" || command == "volume") {
        if (arg.length() == 0) {
            Serial.println("Помилка: 0-63");
            return;
        }
        int val = arg.toInt();
        if (val < 0 || val > 63) { Serial.println("Помилка: 0-63"); return; }
        tda7318SetVolume(val);
        return;
    }
    if (command == "vol+" || command == "v+") {
        int step = arg.length() > 0 ? arg.toInt() : 3;
        if (step < 1 || step > 30) step = 3;
        tda7318SetVolume(min(tda7318GetVolume() + step, 63));
        return;
    }
    if (command == "vol-" || command == "v-") {
        int step = arg.length() > 0 ? arg.toInt() : 3;
        if (step < 1 || step > 30) step = 3;
        tda7318SetVolume(max(tda7318GetVolume() - step, 0));
        return;
    }
    
    // Бас
    if (command == "bass" || command == "b") {
        if (arg.length() == 0) { Serial.println("Помилка: -7..+7"); return; }
        int val = arg.toInt();
        if (val < -7 || val > 7) { Serial.println("Помилка: -7..+7"); return; }
        tda7318SetBass(val);
        return;
    }
    if (command == "bass+" || command == "b+") { tda7318SetBass(min(tda7318GetBass() + 1, 7)); return; }
    if (command == "bass-" || command == "b-") { tda7318SetBass(max(tda7318GetBass() - 1, -7)); return; }
    
    // ВЧ
    if (command == "treble" || command == "t") {
        if (arg.length() == 0) { Serial.println("Помилка: -7..+7"); return; }
        int val = arg.toInt();
        if (val < -7 || val > 7) { Serial.println("Помилка: -7..+7"); return; }
        tda7318SetTreble(val);
        return;
    }
    if (command == "treble+" || command == "t+") { tda7318SetTreble(min(tda7318GetTreble() + 1, 7)); return; }
    if (command == "treble-" || command == "t-") { tda7318SetTreble(max(tda7318GetTreble() - 1, -7)); return; }
    
    // Баланс
    if (command == "balance" || command == "bal") {
        if (arg.length() == 0) { Serial.println("Помилка: -31..+31"); return; }
        int val = arg.toInt();
        if (val < -31 || val > 31) { Serial.println("Помилка: -31..+31"); return; }
        tda7318SetBalance(val);
        return;
    }
    if (command == "bal+" || command == "bl+") { tda7318SetBalance(min(tda7318GetBalance() + 1, 31)); return; }
    if (command == "bal-" || command == "bl-") { tda7318SetBalance(max(tda7318GetBalance() - 1, -31)); return; }
    if (command == "bal-c" || command == "bl-c") { tda7318SetBalance(0); return; }
    
    // Підсилення
    if (command == "gain" || command == "g") {
        if (arg.length() == 0) { Serial.println("Помилка: 0-3"); return; }
        int val = arg.toInt();
        if (val < 0 || val > 3) { Serial.println("Помилка: 0-3"); return; }
        tda7318SetGain(val);
        return;
    }
    
    // Вхід
    if (command == "input" || command == "in") {
        if (arg.length() == 0) { Serial.println("Помилка: 0-3"); return; }
        int val = arg.toInt();
        if (val < 0 || val > 3) { Serial.println("Помилка: 0-3"); return; }
        tda7318SetInput((AudioInput)val);
        return;
    }
    
    // Mute
    if (command == "mute" || command == "m") { tda7318Mute(); return; }
    if (command == "unmute" || command == "um") { tda7318UnMute(); return; }
    if (command == "mute-toggle" || command == "mt") { tda7318SetMute(!tda7318IsMuted()); return; }
    
    Serial.println("Помилка: невідома команда. Введіть 'help'.");
}

void handleSerialInput() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (cmdIndex > 0) {
                cmdBuffer[cmdIndex] = '\0';
                String cmd = String(cmdBuffer);
                cmdIndex = 0;
                Serial.print("> ");
                Serial.println(cmd);
                executeCommand(cmd);
                Serial.print("cmd> ");
            }
        } else if (c == '\b' || c == 127) {
            if (cmdIndex > 0) {
                cmdIndex--;
                Serial.print("\b \b");
            }
        } else if (c >= 32 && c < 127 && cmdIndex < CMD_BUFFER_SIZE - 1) {
            cmdBuffer[cmdIndex++] = c;
            Serial.print(c);
        }
    }
}

// ============================================================================
// ОБРОБКА ВВОДУ
// ============================================================================

void handleInputEvent(InputEvent event) {
    switch (event.type) {
        case EVENT_ENCODER_ROTATE: {
            // Обертання енкодера - регулюємо поточний параметр
            int16_t delta = event.value;
            Parameter param = inputGetParameter();
            
            switch (param) {
                case PARAM_VOLUME: {
                    uint8_t vol = tda7318GetVolume();
                    int newVol = (int)vol + delta;
                    if (newVol < 0) newVol = 0;
                    if (newVol > 63) newVol = 63;
                    tda7318SetVolume(newVol);
                    Serial.println("[Encoder] Гучність: " + String(newVol));
                    displayUpdateVolume(newVol);
                    break;
                }
                case PARAM_BASS: {
                    int8_t bass = tda7318GetBass();
                    int newBass = (int)bass + delta;
                    if (newBass < BASS_MIN) newBass = BASS_MIN;
                    if (newBass > BASS_MAX) newBass = BASS_MAX;
                    tda7318SetBass(newBass);
                    Serial.println("[Encoder] НЧ: " + String(newBass));
                    displayUpdateParameter(param, newBass);
                    break;
                }
                case PARAM_TREBLE: {
                    int8_t treble = tda7318GetTreble();
                    int newTreble = (int)treble + delta;
                    if (newTreble < TREBLE_MIN) newTreble = TREBLE_MIN;
                    if (newTreble > TREBLE_MAX) newTreble = TREBLE_MAX;
                    tda7318SetTreble(newTreble);
                    Serial.println("[Encoder] ВЧ: " + String(newTreble));
                    displayUpdateParameter(param, newTreble);
                    break;
                }
                case PARAM_BALANCE: {
                    int8_t balance = tda7318GetBalance();
                    int newBalance = (int)balance + delta;
                    if (newBalance < BALANCE_MIN) newBalance = BALANCE_MIN;
                    if (newBalance > BALANCE_MAX) newBalance = BALANCE_MAX;
                    tda7318SetBalance(newBalance);
                    Serial.println("[Encoder] Баланс: " + String(newBalance));
                    displayUpdateParameter(param, newBalance);
                    break;
                }
                case PARAM_GAIN: {
                    uint8_t gain = tda7318GetGain();
                    int newGain = (int)gain + delta;
                    if (newGain < 0) newGain = 0;
                    if (newGain > 3) newGain = 3;
                    tda7318SetGain(newGain);
                    Serial.println("[Encoder] Підсилення: " + String(newGain));
                    displayUpdateParameter(param, newGain);
                    break;
                }
            }
            break;
        }
        
        case EVENT_ENCODER_CLICK: {
            Parameter current = inputGetParameter();
            Parameter next = (Parameter)((current + 1) % PARAM_COUNT);
            inputSetParameter(next);
            Serial.println("[Encoder] Параметр: " + String(next) + " - " + inputGetParameterName());
            displayUpdateParameter(next, 0);
            break;
        }
        
        case EVENT_BUTTON_PRESS: {
            ButtonFunction btn = (ButtonFunction)event.value;
            Serial.print("[Button] Натискання: ");
            
            switch (btn) {
                case BTN_FUNC_PWR:
                    Serial.println("PWR");
                    break;
                case BTN_FUNC_UP:
                    Serial.println("UP");
                    {
                        AudioInput current = tda7318GetInput();
                        AudioInput prev = (AudioInput)((current == 0) ? (INPUT_COUNT - 1) : (current - 1));
                        tda7318SetInput(prev);
                        Serial.println("[Input] Вхід: " + String(prev));
                        displayUpdateInput(prev);
                        displayUpdateVolume(tda7318GetVolume());
                    }
                    break;
                case BTN_FUNC_DOWN:
                    Serial.println("DOWN");
                    {
                        AudioInput current = tda7318GetInput();
                        AudioInput next = (AudioInput)((current + 1) % INPUT_COUNT);
                        tda7318SetInput(next);
                        Serial.println("[Input] Вхід: " + String(next));
                        displayUpdateInput(next);
                        displayUpdateVolume(tda7318GetVolume());
                    }
                    break;
                case BTN_FUNC_LEFT:
                    Serial.println("LEFT (prev track)");
                    break;
                case BTN_FUNC_OK:
                    Serial.println("OK (play/pause)");
                    break;
                case BTN_FUNC_RIGHT:
                    Serial.println("RIGHT (next track)");
                    break;
                default:
                    Serial.println("Unknown (" + String(event.value) + ")");
                    break;
            }
            break;
        }
        
        case EVENT_IR_COMMAND: {
            Serial.print("[IR] Команда: ");
            
            // Спеціальні коди
            if (event.value == 100) {
                // VOL+
                uint8_t vol = tda7318GetVolume();
                if (vol < 63) tda7318SetVolume(vol + 1);
                Serial.println("VOL+");
            } else if (event.value == 101) {
                // VOL-
                uint8_t vol = tda7318GetVolume();
                if (vol > 0) tda7318SetVolume(vol - 1);
                Serial.println("VOL-");
            } else if (event.value == 102) {
                // MUTE
                tda7318SetMute(!tda7318IsMuted());
                Serial.println("MUTE");
            } else {
                // Звичайні кнопки
                ButtonFunction btn = (ButtonFunction)event.value;
                switch (btn) {
                    case BTN_FUNC_PWR:       Serial.println("PWR"); break;
                    case BTN_FUNC_UP:        Serial.println("UP"); break;
                    case BTN_FUNC_DOWN:      Serial.println("DOWN"); break;
                    case BTN_FUNC_LEFT:      Serial.println("LEFT"); break;
                    case BTN_FUNC_OK:        Serial.println("OK"); break;
                    case BTN_FUNC_RIGHT:     Serial.println("RIGHT"); break;
                    default:                 Serial.println("Unknown (" + String(event.value) + ")"); break;
                }
            }
            break;
        }
        
        default:
            break;
    }
}

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
    
    // Показати початковий стан на дисплеї
    displayUpdateVolume(tda7318GetVolume());
    displayUpdateInput(tda7318GetInput());
    displayUpdateParameter(inputGetParameter(), 0);
    
    lastEncoderActivity = millis();
    
    Serial.println("");
    Serial.println("[SYSTEM] Готово! Введіть 'help' для списку команд");
    Serial.println("");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // 1. Обробка Serial терміналу
    handleSerialInput();
    
    // 2. Обробка ввідних пристроїв (енкодер, кнопки, IR)
    InputEvent event = inputPoll();
    if (event.type != EVENT_NONE) {
        handleInputEvent(event);
        
        if (event.type == EVENT_ENCODER_ROTATE || event.type == EVENT_ENCODER_CLICK) {
            lastEncoderActivity = millis();
        }
    }
    
    // 3. Скидання параметра на гучність після 10с неактивності
    if (inputGetParameter() != PARAM_VOLUME) {
        if (millis() - lastEncoderActivity > ENCODER_TIMEOUT_MS) {
            inputSetParameter(PARAM_VOLUME);
            displayUpdateParameter(PARAM_VOLUME, 0);
            Serial.println("[INPUT] Таймаут - скинуто на гучність");
        }
    }
    
    delay(1);
}
