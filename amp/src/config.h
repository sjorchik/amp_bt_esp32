#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// ВИЗНАЧЕННЯ ПІНІВ
// ============================================================================

// I2S - ЦАП UDA1334A
#ifndef I2S_BCK_PIN
#define I2S_BCK_PIN 26
#endif
#ifndef I2S_WS_PIN
#define I2S_WS_PIN 25
#endif
#ifndef I2S_DOUT_PIN
#define I2S_DOUT_PIN 27
#endif

// I2C - аудіопроцесор TDA7318
#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 21
#endif
#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 22
#endif
#ifndef TDA7318_ADDRESS
#define TDA7318_ADDRESS 0x44
#endif

// SPI - дисплей ST7789 (TFT_MISO не підключено)
#ifndef TFT_MOSI
#define TFT_MOSI 23
#endif
#ifndef TFT_SCLK
#define TFT_SCLK 18
#endif
#ifndef TFT_CS
#define TFT_CS 5
#endif
#ifndef TFT_DC
#define TFT_DC 16
#endif
#ifndef TFT_RST
#define TFT_RST 17
#endif
#ifndef TFT_BL
#define TFT_BL 4
#endif

// Енкодер (підтяжка 12кОм до +3.3В)
#ifndef ENCODER_CLK_PIN
#define ENCODER_CLK_PIN 13
#endif
#ifndef ENCODER_DT_PIN
#define ENCODER_DT_PIN 14
#endif
#ifndef ENCODER_SW_PIN
#define ENCODER_SW_PIN 19
#endif

// IR приймач (RC-5)
#ifndef IR_RECV_PIN
#define IR_RECV_PIN 15
#endif

// Кнопки (підтяжка 12кОм до +3.3В)
#ifndef BTN_PWR_PIN
#define BTN_PWR_PIN 33
#endif
#ifndef BTN_UP_PIN
#define BTN_UP_PIN 32
#endif
#ifndef BTN_DOWN_PIN
#define BTN_DOWN_PIN 36
#endif
#ifndef BTN_LEFT_PIN
#define BTN_LEFT_PIN 35
#endif
#ifndef BTN_OK_PIN
#define BTN_OK_PIN 34
#endif
#ifndef BTN_RIGHT_PIN
#define BTN_RIGHT_PIN 39
#endif

// ============================================================================
// КОНФІГУРАЦІЯ TDA7318
// ============================================================================

// Аудіовходи
enum AudioInput {
    INPUT_BLUETOOTH = 0,
    INPUT_COMPUTER = 1,
    INPUT_TV_BOX = 2,
    INPUT_AUX = 3,
    INPUT_COUNT = 4
};

// Адреси регістрів TDA7318
#define TDA7318_REG_VOLUME    0x00
#define TDA7318_REG_GAIN      0x01
#define TDA7318_REG_BASS      0x02
#define TDA7318_REG_MIDDLE    0x03
#define TDA7318_REG_TREBLE    0x04
#define TDA7318_REG_BALANCE   0x05
#define TDA7318_REG_ATTEN_L   0x06
#define TDA7318_REG_ATTEN_R   0x07
#define TDA7318_REG_INPUT     0x08
#define TDA7318_REG_MUTE      0x09

// Діапазони параметрів (згідно з даташитом TDA7318)
#define VOLUME_MIN     0    // -78.75 дБ (mute)
#define VOLUME_MAX     63   // 0 дБ (max, згідно з даташитом)
#define VOLUME_DEFAULT 40   // -28.75 дБ

#define BASS_MIN      -7    // -10.5 дБ
#define BASS_MAX       7    // +10.5 дБ
#define BASS_DEFAULT   0    // 0 дБ

#define TREBLE_MIN    -7    // -10.5 дБ
#define TREBLE_MAX     7    // +10.5 дБ
#define TREBLE_DEFAULT   0  // 0 дБ

#define BALANCE_MIN   -31   // Лівий канал (згідно з даташитом)
#define BALANCE_MAX    31   // Правий канал (згідно з даташитом)
#define BALANCE_DEFAULT 0   // Центр

#define GAIN_MIN       0    // 0 дБ
#define GAIN_MAX       3    // +13.5 дБ
#define GAIN_DEFAULT   0    // 0 дБ

// Порядок перемикання параметрів енкодером
enum Parameter {
    PARAM_VOLUME = 0,    // Гучність
    PARAM_BASS = 1,      // НЧ
    PARAM_TREBLE = 2,    // ВЧ
    PARAM_BALANCE = 3,   // Баланс
    PARAM_GAIN = 4,      // Підсилення
    PARAM_COUNT = 5
};

// ============================================================================
// EEPROM - ЗБЕРІГАННЯ ДАНИХ
// ============================================================================

#define EEPROM_SIZE 512

// Адреси EEPROM
#define EEPROM_MAGIC_ADDR     0x0000
#define EEPROM_MAGIC_VALUE    0xA5

// Структура налаштувань для кожного входу
struct InputSettings {
    uint8_t volume;       // 0-63
    int8_t bass;          // -7 до +7
    int8_t treble;        // -7 до +7
    int8_t balance;       // -31 до +31
    uint8_t gain;         // 0-3
    uint8_t last_param;   // Останній активний параметр енкодера
    uint8_t reserved[2];  // Вирівнювання
};

// Глобальні налаштування
struct GlobalSettings {
    uint8_t magic;              // Магічний байт для перевірки валідності EEPROM
    uint8_t current_input;      // Поточний активний вхід
    uint8_t standby;            // Режим очікування (0=активний, 1=очікування)
    uint8_t reserved[1];
    InputSettings inputs[INPUT_COUNT];  // Налаштування для кожного входу
};

// ============================================================================
// ПРОТОКОЛ BLUETOOTH SPP
// ============================================================================

// JSON протокол для обміну даними з Android додатком
// ESP32 -> Android (при підключенні):
// {
//   "type": "status",
//   "input": 0,
//   "volume": 40,
//   "bass": 0,
//   "treble": 0,
//   "balance": 0,
//   "gain": 0,
//   "mute": false,
//   "standby": false
// }

// Android -> ESP32 (команди):
// {"cmd": "set_volume", "value": 40}
// {"cmd": "set_bass", "value": 3}
// {"cmd": "set_treble", "value": -2}
// {"cmd": "set_balance", "value": 10}
// {"cmd": "set_gain", "value": 2}
// {"cmd": "set_input", "value": 1}
// {"cmd": "mute", "value": true}
// {"cmd": "standby", "value": false}
// {"cmd": "get_status"}

// ESP32 -> Android (оновлення статусу, тільки при зміні):
// {
//   "type": "update",
//   "param": "volume",
//   "value": 40
// }

// ============================================================================
// ДЖЕРЕЛА ВХОДУ (КНОПКИ ТА IR)
// ============================================================================

// Функції кнопок
enum ButtonFunction {
    BTN_FUNC_PWR,
    BTN_FUNC_UP,
    BTN_FUNC_DOWN,
    BTN_FUNC_LEFT,
    BTN_FUNC_OK,
    BTN_FUNC_RIGHT,
    BTN_FUNC_ENCODER_SW,
    BTN_FUNC_COUNT
};

// Команди IR RC-5
#define IR_RC5_ADDR 0x00
#define IR_RC5_PWR    0x0C
#define IR_RC5_UP     0x10
#define IR_RC5_DOWN   0x11
#define IR_RC5_LEFT   0x12
#define IR_RC5_OK     0x13
#define IR_RC5_RIGHT  0x14
#define IR_RC5_VOL_UP   0x15
#define IR_RC5_VOL_DOWN 0x16
#define IR_RC5_MUTE     0x0D

// ============================================================================
// КОНФІГУРАЦІЯ ЗАВДАНЬ FREERTOS
// ============================================================================

#ifndef TASK_PRIORITY_BT_AUDIO
#define TASK_PRIORITY_BT_AUDIO 24
#endif
#ifndef TASK_PRIORITY_BT_CONTROL
#define TASK_PRIORITY_BT_CONTROL 20
#endif
#ifndef TASK_PRIORITY_TDA7318
#define TASK_PRIORITY_TDA7318 16
#endif
#ifndef TASK_PRIORITY_DISPLAY
#define TASK_PRIORITY_DISPLAY 12
#endif
#ifndef TASK_PRIORITY_INPUT
#define TASK_PRIORITY_INPUT 8
#endif
#ifndef TASK_PRIORITY_SYSTEM
#define TASK_PRIORITY_SYSTEM 4
#endif

#ifndef TASK_STACK_BT_AUDIO
#define TASK_STACK_BT_AUDIO 4096
#endif
#ifndef TASK_STACK_BT_CONTROL
#define TASK_STACK_BT_CONTROL 3072
#endif
#ifndef TASK_STACK_TDA7318
#define TASK_STACK_TDA7318 2048
#endif
#ifndef TASK_STACK_DISPLAY
#define TASK_STACK_DISPLAY 4096
#endif
#ifndef TASK_STACK_INPUT
#define TASK_STACK_INPUT 2048
#endif
#ifndef TASK_STACK_SYSTEM
#define TASK_STACK_SYSTEM 2048
#endif

// ============================================================================
// КОНФІГУРАЦІЯ ЧЕРГ
// ============================================================================

#ifndef QUEUE_SIZE_INPUT
#define QUEUE_SIZE_INPUT 20
#endif
#ifndef QUEUE_SIZE_DISPLAY
#define QUEUE_SIZE_DISPLAY 10
#endif
#ifndef QUEUE_SIZE_TDA7318
#define QUEUE_SIZE_TDA7318 10
#endif
#ifndef QUEUE_SIZE_BT_CONTROL
#define QUEUE_SIZE_BT_CONTROL 10
#endif

// Типи подій для черги вводу
enum EventType {
    EVENT_NONE,           // Немає події
    EVENT_ENCODER_ROTATE, // Обертання енкодера
    EVENT_ENCODER_CLICK,  // Натискання енкодера
    EVENT_BUTTON_PRESS,   // Натискання кнопки
    EVENT_IR_COMMAND,     // Команда IR RC-5
    EVENT_COUNT
};

struct InputEvent {
    EventType type;
    int16_t value;      // Дельта енкодера, номер кнопки, код IR
    uint32_t timestamp;
};

// Типи подій для черги дисплея
enum DisplayEvent {
    DISP_UPDATE_SCREEN,   // Оновлення екрану
    DISP_SHOW_MESSAGE,    // Показати повідомлення
    DISP_SHOW_MENU,       // Показати меню
    DISP_UPDATE_PARAM     // Оновити параметр
};

struct DisplayEventStruct {
    DisplayEvent type;
    int16_t param1;
    int16_t param2;
    char message[64];
};

// ============================================================================
// КОНФІГУРАЦІЯ ДИСПЛЕЯ
// ============================================================================

#define DISPLAY_WIDTH 170
#define DISPLAY_HEIGHT 320

// Кольори UI (RGB565)
#define COLOR_BG        0x0000    // Чорний
#define COLOR_TEXT      0xFFFF    // Білий
#define COLOR_ACCENT    0x07FF    // Бірюзовий
#define COLOR_HIGHLIGHT 0xF800    // Червоний
#define COLOR_DIM       0x7BEF    // Сірий

// Елементи UI
#define UI_MARGIN       10
#define UI_HEADER_HEIGHT 40
#define UI_FOOTER_HEIGHT 30
#define UI_PARAM_HEIGHT   50

// ============================================================================
// КОНФІГУРАЦІЯ СИСТЕМИ
// ============================================================================

// Керування живленням
#define STANDBY_TIMEOUT_MS 300000  // 5 хвилин
#define DEBOUNCE_MS 50

// Debug
#ifdef DEBUG_SERIAL
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(x, y) Serial.printf(x, y)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(x, y)
#endif

// ============================================================================
// ДОПОМІЖНІ ФУНКЦІЇ (inline)
// ============================================================================

// Конвертувати регістр гучності TDA7318 у дБ
// 0 = -78.75дБ (mute), 63 = 0дБ (max)
inline float volume_to_db(uint8_t vol) {
    if (vol > 63) vol = 63;
    return -78.75f + (vol * 1.25f);
}

// Конвертувати регістр TDA7318 в гучність (зворотна інверсія)
// Регістр: 0 = max гучність, 63 = тиша
// Гучність: 0 = тиша, 63 = max гучність
inline uint8_t reg_to_volume(uint8_t reg) {
    if (reg > 63) reg = 63;
    return 63 - reg;
}

// Конвертувати НЧ/ВЧ у дБ
inline int8_t tone_to_db(int8_t val) {
    return val * 1.5f;  // 1.5 дБ на крок
}

// Конвертувати баланс у дБ
inline float balance_to_db(int8_t val) {
    return val * 1.0f;  // 1.0 дБ на крок
}

#endif // CONFIG_H
