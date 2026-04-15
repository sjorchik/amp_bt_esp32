/**
 * @file input.cpp
 * @brief Модуль ввідних пристроїв: енкодер, кнопки, IR RC-5
 */
#include "input.h"
#include "config.h"

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
