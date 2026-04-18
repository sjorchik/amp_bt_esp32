/**
 * @file input.cpp
 * @brief Модуль ввідних пристроїв: енкодер, кнопки, IR RC-5
 */
#include "input.h"
#include "config.h"
#include "tda7318.h"
#include "display.h"
#include "bt_audio.h"

static void handleInputChange(AudioInput newInput) {
    AudioInput oldInput = tda7318GetInput();
    tda7318SetInput(newInput);
    
    if (newInput == INPUT_BLUETOOTH && oldInput != INPUT_BLUETOOTH) {
        btAudioStart();
    } else if (newInput != INPUT_BLUETOOTH && oldInput == INPUT_BLUETOOTH) {
        btAudioEnd();
        btAudioClearMetadata();
    }
    
    displayUpdateInput(newInput);
    displayUpdateValue(tda7318GetVolume(), 63);
}

// ============================================================================
// БІБЛІОТЕКИ
// ============================================================================

// IRremoteESP8266 для RC-5
#include <IRrecv.h>
#include <IRutils.h>

// ============================================================================
// СТАНИ ЕНКОДЕРА
// ============================================================================

static volatile int32_t encoderPosition = 0;
static int32_t lastEncoderPosition = 0;

// Піни енкодера
static const uint8_t PIN_ENC_CLK = ENCODER_CLK_PIN;
static const uint8_t PIN_ENC_DT = ENCODER_DT_PIN;
static const uint8_t PIN_ENC_SW = ENCODER_SW_PIN;

// Стани пінів
static bool encClkState = false;
static bool encDtState = false;
static bool encLastClkState = false;
static bool encSwState = false;
static bool encSwLastState = false;
static bool encSwPressed = false;

// Дебаунс для кнопки енкодера
static uint32_t encSwDebounceTime = 0;
static const uint32_t ENC_SW_DEBOUNCE_MS = 50;

// Поточний параметр енкодера (використовуємо Parameter з config.h)
static Parameter currentParam = PARAM_VOLUME;

// Назви параметрів
static const char* paramNames[] = {
    "Гучність",
    "НЧ",
    "ВЧ",
    "Баланс",
    "Підсилення"
};

// ============================================================================
// СТАНИ КНОПОК
// ============================================================================

// Структура кнопки
struct ButtonState {
    uint8_t pin;
    ButtonFunction code;
    bool lastState;
    bool currentState;
    bool pressed;
    bool longPressed;
    uint32_t pressTime;
    uint32_t lastEventTime;
};

// Піни кнопок
static const uint8_t BUTTON_PINS[] = {
    BTN_PWR_PIN,     // BTN_FUNC_PWR
    BTN_UP_PIN,      // BTN_FUNC_UP
    BTN_DOWN_PIN,    // BTN_FUNC_DOWN
    BTN_LEFT_PIN,    // BTN_FUNC_LEFT
    BTN_OK_PIN,      // BTN_FUNC_OK
    BTN_RIGHT_PIN    // BTN_FUNC_RIGHT
};

static const ButtonFunction BUTTON_CODES[] = {
    BTN_FUNC_PWR,
    BTN_FUNC_UP,
    BTN_FUNC_DOWN,
    BTN_FUNC_LEFT,
    BTN_FUNC_OK,
    BTN_FUNC_RIGHT
};

#define BUTTON_COUNT 6

static ButtonState buttons[BUTTON_COUNT];

static const uint32_t BTN_DEBOUNCE_MS = 50;
static const uint32_t BTN_LONG_PRESS_MS = 1000;

// ============================================================================
// IR RC-5
// ============================================================================

static IRrecv* irReceiver = nullptr;
static decode_results irResults;
static bool irReceiverReady = false;

static const uint8_t IR_PIN = IR_RECV_PIN;

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================

void inputInit() {
    // --- Енкодер ---
    pinMode(PIN_ENC_CLK, INPUT);
    pinMode(PIN_ENC_DT, INPUT);
    pinMode(PIN_ENC_SW, INPUT);
    
    encClkState = digitalRead(PIN_ENC_CLK);
    encDtState = digitalRead(PIN_ENC_DT);
    encLastClkState = encClkState;
    encSwState = digitalRead(PIN_ENC_SW);
    encSwLastState = encSwState;
    
    DEBUG_PRINTLN("[INPUT] Енкодер ініціалізовано (CLK=" + String(PIN_ENC_CLK) + 
                  ", DT=" + String(PIN_ENC_DT) + 
                  ", SW=" + String(PIN_ENC_SW) + ")");
    DEBUG_PRINTLN("[INPUT] Початкові стани: CLK=" + String(encClkState) + 
                  ", DT=" + String(encDtState) + 
                  ", SW=" + String(encSwState));

    // --- Кнопки ---
    for (int i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].pin = BUTTON_PINS[i];
        buttons[i].code = BUTTON_CODES[i];
        buttons[i].lastState = HIGH;  // Підтяжка до +3.3V
        buttons[i].currentState = HIGH;
        buttons[i].pressed = false;
        buttons[i].longPressed = false;
        buttons[i].pressTime = 0;
        buttons[i].lastEventTime = 0;
        
        pinMode(buttons[i].pin, INPUT);
    }
    
    DEBUG_PRINTLN("[INPUT] Кнопки ініціалізовано (6 шт)");

    // --- IR RC-5 ---
    irReceiver = new IRrecv(IR_PIN);
    irReceiver->enableIRIn();
    irReceiverReady = true;
    
    DEBUG_PRINTLN("[INPUT] IR приймач ініціалізовано (GPIO" + String(IR_PIN) + ")");
}

void inputSetParameter(Parameter param) {
    if (param < PARAM_COUNT) {
        currentParam = param;
    }
}

Parameter inputGetParameter() {
    return currentParam;
}

const char* inputGetParameterName() {
    if (currentParam < PARAM_COUNT) {
        return paramNames[currentParam];
    }
    return "Unknown";
}

// ============================================================================
// ОБРОБКА ЕНКОДЕРА
// ============================================================================

static void processEncoder() {
    bool clk = digitalRead(PIN_ENC_CLK);
    bool dt = digitalRead(PIN_ENC_DT);
    
    if (clk != encLastClkState) {
        if (clk == HIGH) {
            if (dt == LOW) {
                encoderPosition++;
            } else {
                encoderPosition--;
            }
        }
        encLastClkState = clk;
    }
    
    uint32_t now = millis();
    bool sw = digitalRead(PIN_ENC_SW);
    
    if (sw != encSwLastState) {
        if (now - encSwDebounceTime > ENC_SW_DEBOUNCE_MS) {
            encSwState = sw;
            encSwLastState = sw;
            encSwDebounceTime = now;
            
            // Виявлення натискання (HIGH -> LOW)
            if (sw == LOW && !encSwPressed) {
                encSwPressed = true;
            }
            // Виявлення відпускання (LOW -> HIGH)
            else if (sw == HIGH && encSwPressed) {
                encSwPressed = false;
            }
        }
    }
}

InputEvent inputPollEncoder() {
    InputEvent event;
    event.type = EVENT_NONE;
    event.value = 0;
    event.timestamp = millis();
    
    processEncoder();
    
    int32_t delta = encoderPosition - lastEncoderPosition;
    if (delta != 0) {
        event.type = EVENT_ENCODER_ROTATE;
        event.value = (delta > 0) ? 1 : -1;
        lastEncoderPosition = encoderPosition;
        DEBUG_PRINTLN("[ENC] Rotate: " + String(event.value));
        return event;
    }
    
    if (encSwPressed) {
        event.type = EVENT_ENCODER_CLICK;
        event.value = 1;
        encSwPressed = false;
        DEBUG_PRINTLN("[ENC] Click");
        return event;
    }
    
    return event;
}

// ============================================================================
// ОБРОБКА КНОПОК
// ============================================================================

InputEvent inputPollButtons() {
    InputEvent event;
    event.type = EVENT_NONE;  // EVENT_NONE
    event.value = 0;
    event.timestamp = millis();
    
    uint32_t now = millis();
    
    for (int i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].currentState = digitalRead(buttons[i].pin);
        
        // HIGH = не натиснута, LOW = натиснута (підтяжка до +3.3В)
        bool pressed = (buttons[i].currentState == LOW);
        bool wasPressed = (buttons[i].lastState == LOW);
        
        // Виявлення натискання
        if (pressed && !wasPressed) {
            buttons[i].pressTime = now;
            buttons[i].pressed = true;
            buttons[i].lastState = LOW;
            buttons[i].lastEventTime = now;
        }
        // Виявлення відпускання
        else if (!pressed && wasPressed) {
            if (buttons[i].pressed && !buttons[i].longPressed) {
                // Подія буде відправлена
            }
            buttons[i].pressed = false;
            buttons[i].longPressed = false;
            buttons[i].lastState = HIGH;
        }
        
        // Виявлення довгого натискання
        if (pressed && !buttons[i].longPressed && (now - buttons[i].pressTime > BTN_LONG_PRESS_MS)) {
            buttons[i].longPressed = true;
            event.type = EVENT_BUTTON_PRESS;  // Використовуємо EVENT_BUTTON_PRESS для довгого теж
            event.value = buttons[i].code;
            event.timestamp = now;
            return event;
        }
        
        // Виявлення короткого натискання (при відпусканні)
        if (!pressed && wasPressed && !buttons[i].longPressed) {
            event.type = EVENT_BUTTON_PRESS;
            event.value = buttons[i].code;
            event.timestamp = now;
            return event;
        }
        
        buttons[i].lastState = buttons[i].currentState;
    }
    
    return event;
}

// ============================================================================
// ОБРОБКА IR
// ============================================================================

InputEvent inputPollIR() {
    InputEvent event;
    event.type = EVENT_NONE;  // EVENT_NONE
    event.value = 0;
    event.timestamp = millis();
    
    if (!irReceiverReady) return event;
    
    if (irReceiver->decode(&irResults)) {
        uint32_t irCode = irResults.value;
        
        event.type = EVENT_IR_COMMAND;
        
        if (irCode == 0x2F || irCode == 0x82F) {
            event.value = 121; // TRE-
        } else if (irCode == 0x102F || irCode == 0x182F) {
            event.value = 131; // BAL-
        } else if (irCode == 0x10F2 || irCode == 0x18F2) {
            event.value = 111; // BASS-
        } else if (irCode == 0x10F3 || irCode == 0x18F3) {
            event.value = 141; // GAIN-
        } else {
            event.value = (int16_t)(irCode & 0xFFFF);
        }
        
        DEBUG_PRINTLN("[IR] Повний код: 0x" + String(irCode, HEX) + " | value=" + String(event.value));
        
        event.timestamp = millis();
        irReceiver->resume();
    }
    
    return event;
}

// ============================================================================
// ЗАГАЛЬНА ОБРОБКА
// ============================================================================

InputEvent inputPoll() {
    // Пріоритет: IR > Buttons > Encoder
    InputEvent event;
    
    event = inputPollIR();
    if (event.type != EVENT_NONE) return event;
    
    event = inputPollButtons();
    if (event.type != EVENT_NONE) return event;
    
    event = inputPollEncoder();
    if (event.type != EVENT_NONE) return event;
    
    return event;
}

// ============================================================================
// ОБРОБКА ПОДІЙ
// ============================================================================

static bool isStandby = false;

static void toggleStandby() {
    isStandby = !isStandby;
    
    if (isStandby) {
        Serial.println("[SYSTEM] Режим очікування увімкнено");
        tda7318SetMute(true);
        displaySetStandby(true);
        btAudioEnd();
    } else {
        Serial.println("[SYSTEM] Режим очікування вимкнено");
        tda7318SetMute(false);
        displaySetStandby(false);
        displayUpdateInput(tda7318GetInput());
        displayUpdateValue(tda7318GetVolume(), 63);
        
        if (tda7318GetInput() == INPUT_BLUETOOTH) {
            btAudioStart();
        }
    }
}

bool inputIsStandby() {
    return isStandby;
}

void handleInputEvent(InputEvent event) {
    switch (event.type) {
        case EVENT_ENCODER_ROTATE: {
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
                    displayUpdateValue(newVol, 63);
                    displayShowParam("Volume", newVol, 10000);
                    break;
                }
                case PARAM_BASS: {
                    int8_t bass = tda7318GetBass();
                    int newBass = (int)bass + delta;
                    if (newBass < BASS_MIN) newBass = BASS_MIN;
                    if (newBass > BASS_MAX) newBass = BASS_MAX;
                    tda7318SetBass(newBass);
                    Serial.println("[Encoder] НЧ: " + String(newBass));
                    displayUpdateValue(newBass - BASS_MIN, BASS_MAX - BASS_MIN);
                    displayShowParam("Bass", newBass, 10000);
                    break;
                }
                case PARAM_TREBLE: {
                    int8_t treble = tda7318GetTreble();
                    int newTreble = (int)treble + delta;
                    if (newTreble < TREBLE_MIN) newTreble = TREBLE_MIN;
                    if (newTreble > TREBLE_MAX) newTreble = TREBLE_MAX;
                    tda7318SetTreble(newTreble);
                    Serial.println("[Encoder] ВЧ: " + String(newTreble));
                    displayUpdateValue(newTreble - TREBLE_MIN, TREBLE_MAX - TREBLE_MIN);
                    displayShowParam("Treble", newTreble, 10000);
                    break;
                }
                case PARAM_BALANCE: {
                    int8_t balance = tda7318GetBalance();
                    int newBalance = (int)balance + delta;
                    if (newBalance < BALANCE_MIN) newBalance = BALANCE_MIN;
                    if (newBalance > BALANCE_MAX) newBalance = BALANCE_MAX;
                    tda7318SetBalance(newBalance);
                    Serial.println("[Encoder] Баланс: " + String(newBalance));
                    displayUpdateValue(newBalance - BALANCE_MIN, BALANCE_MAX - BALANCE_MIN);
                    displayShowParam("Balance", newBalance, 10000);
                    break;
                }
                case PARAM_GAIN: {
                    uint8_t gain = tda7318GetGain();
                    int newGain = (int)gain + delta;
                    if (newGain < 0) newGain = 0;
                    if (newGain > 3) newGain = 3;
                    tda7318SetGain(newGain);
                    Serial.println("[Encoder] Підсилення: " + String(newGain));
                    displayUpdateValue(newGain, 3);
                    displayShowParam("Gain", newGain, 10000);
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
            
            const char* paramLabels[] = {"Volume", "Bass", "Treble", "Balance", "Gain"};
            int16_t displayVal = 0;
            int16_t barVal = 0;
            int16_t maxVal = 63;
            switch (next) {
                case PARAM_VOLUME: displayVal = tda7318GetVolume(); barVal = displayVal; maxVal = 63; break;
                case PARAM_BASS: displayVal = tda7318GetBass(); barVal = displayVal - BASS_MIN; maxVal = BASS_MAX - BASS_MIN; break;
                case PARAM_TREBLE: displayVal = tda7318GetTreble(); barVal = displayVal - TREBLE_MIN; maxVal = TREBLE_MAX - TREBLE_MIN; break;
                case PARAM_BALANCE: displayVal = tda7318GetBalance(); barVal = displayVal - BALANCE_MIN; maxVal = BALANCE_MAX - BALANCE_MIN; break;
                case PARAM_GAIN: displayVal = tda7318GetGain(); barVal = displayVal; maxVal = 3; break;
            }
            displayUpdateValue(barVal, maxVal);
            displayShowParam(paramLabels[next], displayVal, 10000);
            break;
        }
        
        case EVENT_BUTTON_PRESS: {
            ButtonFunction btn = (ButtonFunction)event.value;
            Serial.print("[Button] Натискання: ");
            
            switch (btn) {
                case BTN_FUNC_PWR:
                    Serial.println("PWR");
                    toggleStandby();
                    break;
                case BTN_FUNC_UP:
                    Serial.println("UP");
                    {
                        AudioInput current = tda7318GetInput();
                        AudioInput prev = (AudioInput)((current == 0) ? (INPUT_COUNT - 1) : (current - 1));
                        handleInputChange(prev);
                        Serial.println("[Input] Вхід: " + String(prev));
                    }
                    break;
                case BTN_FUNC_DOWN:
                    Serial.println("DOWN");
                    {
                        AudioInput current = tda7318GetInput();
                        AudioInput next = (AudioInput)((current + 1) % INPUT_COUNT);
                        handleInputChange(next);
                        Serial.println("[Input] Вхід: " + String(next));
                    }
                    break;
                case BTN_FUNC_LEFT:
                    Serial.println("LEFT (prev track)");
                    btAudioPrev();
                    break;
                case BTN_FUNC_OK:
                    Serial.println("OK (play/pause)");
                    btAudioPlayPause();
                    break;
                case BTN_FUNC_RIGHT:
                    Serial.println("RIGHT (next track)");
                    btAudioNext();
                    break;
                default:
                    Serial.println("Unknown (" + String(event.value) + ")");
                    break;
            }
            break;
        }
        
        case EVENT_IR_COMMAND: {
            static uint32_t lastIrCmdTime = 0;
            uint32_t now = millis();
            
            if (now - lastIrCmdTime < 300) break;
            lastIrCmdTime = now;
            
            int16_t cmd = event.value;
            
            if (cmd == 100 || cmd == 0x10 || cmd == 0x810) {
                uint8_t vol = tda7318GetVolume();
                if (vol < 63) tda7318SetVolume(vol + 1);
                vol = tda7318GetVolume();
                Serial.println("[IR] VOL+");
                displayUpdateValue(vol, 63);
                displayShowParam("Volume", vol, 10000);
            } else if (cmd == 101 || cmd == 0x11 || cmd == 0x811) {
                uint8_t vol = tda7318GetVolume();
                if (vol > 0) tda7318SetVolume(vol - 1);
                vol = tda7318GetVolume();
                Serial.println("[IR] VOL-");
                displayUpdateValue(vol, 63);
                displayShowParam("Volume", vol, 10000);
            } else if (cmd == 102 || cmd == 0x0D || cmd == 0x80D || cmd == 0x100D) {
                bool muted = !tda7318IsMuted();
                tda7318SetMute(muted);
                Serial.println("[IR] MUTE");
                displaySetMute(muted);
            } else if (cmd == 0x1010 || cmd == 0x1810) {
                btAudioPlayPause();
            } else if (cmd == 0x1011 || cmd == 0x1811) {
                btAudioStop();
            } else if (cmd == 0x1015 || cmd == 0x1815) {
                btAudioPrev();
            } else if (cmd == 0x1016 || cmd == 0x1816) {
                btAudioNext();
            } else if (cmd == 111 || cmd == 0x32 || cmd == 0x832 || cmd == 0x1032 || cmd == 0x10F2 || cmd == 0x18F2) {
                int8_t bass = tda7318GetBass();
                if (bass > BASS_MIN) tda7318SetBass(bass - 1);
                bass = tda7318GetBass();
                Serial.println("[IR] BASS-");
                displayUpdateValue(bass - BASS_MIN, BASS_MAX - BASS_MIN);
                displayShowParam("Bass", bass, 10000);
            } else if (cmd == 121 || cmd == 0x2F || cmd == 0x82F) {
                int8_t treble = tda7318GetTreble();
                if (treble > TREBLE_MIN) tda7318SetTreble(treble - 1);
                treble = tda7318GetTreble();
                Serial.println("[IR] TRE-");
                displayUpdateValue(treble - TREBLE_MIN, TREBLE_MAX - TREBLE_MIN);
                displayShowParam("Treble", treble, 10000);
            } else if (cmd == 131) {
                int8_t balance = tda7318GetBalance();
                if (balance > BALANCE_MIN) tda7318SetBalance(balance - 1);
                balance = tda7318GetBalance();
                Serial.println("[IR] BAL-");
                displayUpdateValue(balance - BALANCE_MIN, BALANCE_MAX - BALANCE_MIN);
                displayShowParam("Balance", balance, 10000);
            } else if (cmd == 141 || cmd == 0x33 || cmd == 0x833 || cmd == 0x1033 || cmd == 0x10F3 || cmd == 0x18F3) {
                uint8_t gain = tda7318GetGain();
                if (gain > 0) tda7318SetGain(gain - 1);
                gain = tda7318GetGain();
                Serial.println("[IR] GAIN-");
                displayUpdateValue(gain, 3);
                displayShowParam("Gain", gain, 10000);
            } else if (cmd == 0x2D || cmd == 0x82D) {
                int8_t bass = tda7318GetBass();
                if (bass < BASS_MAX) tda7318SetBass(bass + 1);
                bass = tda7318GetBass();
                Serial.println("[IR] BASS+");
                displayUpdateValue(bass - BASS_MIN, BASS_MAX - BASS_MIN);
                displayShowParam("Bass", bass, 10000);
            } else if (cmd == 0x2B || cmd == 0x82B) {
                int8_t treble = tda7318GetTreble();
                if (treble < TREBLE_MAX) tda7318SetTreble(treble + 1);
                treble = tda7318GetTreble();
                Serial.println("[IR] TRE+");
                displayUpdateValue(treble - TREBLE_MIN, TREBLE_MAX - TREBLE_MIN);
                displayShowParam("Treble", treble, 10000);
            } else if (cmd == 0x2C || cmd == 0x82C) {
                uint8_t gain = tda7318GetGain();
                if (gain < 3) tda7318SetGain(gain + 1);
                gain = tda7318GetGain();
                Serial.println("[IR] GAIN+");
                displayUpdateValue(gain, 3);
                displayShowParam("Gain", gain, 10000);
            } else if (cmd == 0x29 || cmd == 0x829) {
                int8_t balance = tda7318GetBalance();
                if (balance < BALANCE_MAX) tda7318SetBalance(balance + 1);
                balance = tda7318GetBalance();
                Serial.println("[IR] BAL+");
                displayUpdateValue(balance - BALANCE_MIN, BALANCE_MAX - BALANCE_MIN);
                displayShowParam("Balance", balance, 10000);
            } else if (cmd == 0x102B || cmd == 0x182B) {
                handleInputChange(INPUT_BLUETOOTH);
                Serial.println("[IR] INPUT BLUETOOTH");
            } else if (cmd == 0x102C || cmd == 0x182C) {
                handleInputChange(INPUT_COMPUTER);
                Serial.println("[IR] INPUT COMPUTER");
            } else if (cmd == 0x102D || cmd == 0x182D) {
                handleInputChange(INPUT_TV_BOX);
                Serial.println("[IR] INPUT TV BOX");
            } else if (cmd == 0x102E || cmd == 0x182E) {
                handleInputChange(INPUT_AUX);
                Serial.println("[IR] INPUT AUX");
            } else if (cmd == 0x20 || cmd == 0x820) {
                AudioInput current = tda7318GetInput();
                AudioInput prev = (AudioInput)((current == 0) ? (INPUT_COUNT - 1) : (current - 1));
                handleInputChange(prev);
                Serial.println("[IR] PREV INPUT");
            } else if (cmd == 0x21 || cmd == 0x821) {
                AudioInput current = tda7318GetInput();
                AudioInput next = (AudioInput)((current + 1) % INPUT_COUNT);
                handleInputChange(next);
                Serial.println("[IR] NEXT INPUT");
            } else if (cmd == 103 || cmd == 0x0C || cmd == 0x80C || cmd == 0x100C) {
                toggleStandby();
            }
            break;
        }
        
        default:
            break;
    }
}
