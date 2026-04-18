/**
 * @file bt_spp.cpp
 * @brief Bluetooth SPP (Serial Port Profile) поверх існуючого Bluedroid від A2DP.
 *
 * ЧОМУ НЕ BluetoothSerial:
 *   BluetoothSerial викликає btStart() → esp_bluedroid_enable() повторно.
 *   Bluedroid вже запущений від BluetoothA2DPSink → panic / OOM crash.
 *
 * ПРАВИЛЬНИЙ ПІДХІД:
 *   esp_spp_api.h реєструє SPP поверх вже запущеного Bluedroid стека,
 *   не витрачаючи пам'ять на другий стек.
 */

#include "bt_spp.h"
#include "config.h"
#include "tda7318.h"
#include "input.h"
#include "display.h"

#include <esp_spp_api.h>
#include <esp_bt_main.h>
#include <esp_log.h>

// ============================================================================
// Внутрішній стан
// ============================================================================

static const char* TAG = "BT_SPP";
static uint32_t  spp_handle    = 0;     // handle активного з'єднання
static bool      spp_connected = false;

// Статичний буфер для вхідних даних (не heap!)
// JSON команда від Android не перевищить 64 байти
static char rx_buf[128];
static uint8_t rx_len = 0;

// ============================================================================
// Парсинг JSON команд (без ArduinoJson — економить ~8 KB heap)
// ============================================================================

/**
 * Мінімальний парсер: знаходить "key":"value" або "key":number.
 * Достатньо для нашого простого протоколу.
 */
static bool jsonGetStr(const char* json, const char* key, char* out, int out_size) {
    // шукаємо "key":"...
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char* p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    int i = 0;
    while (*p && *p != '"' && i < out_size - 1) out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

static bool jsonGetInt(const char* json, const char* key, int* out) {
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    // пропустити пробіли
    while (*p == ' ') p++;
    // перевірити наявність числа або true/false
    if (*p == 't') { *out = 1; return true; }  // true
    if (*p == 'f') { *out = 0; return true; }  // false
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        *out = (int)strtol(p, nullptr, 10);
        return true;
    }
    return false;
}

// ============================================================================
// Обробка команд
// ============================================================================

static void handleCommand(const char* json) {
    char cmd[32] = {0};
    int  value   = 0;

    if (!jsonGetStr(json, "cmd", cmd, sizeof(cmd))) {
        ESP_LOGW(TAG, "Немає поля 'cmd': %s", json);
        return;
    }

    // Ігноруємо команди в режимі очікування
    if (inputIsStandby()) {
        ESP_LOGW(TAG, "Standby mode - command ignored");
        return;
    }

    bool has_value = jsonGetInt(json, "value", &value);

    ESP_LOGI(TAG, "CMD: %s, val: %d", cmd, value);

    if (strcmp(cmd, "get_status") == 0) {
        btSPPSendFullStatus();
        return;
    }

    if (!has_value) return;

    if (strcmp(cmd, "set_volume") == 0) {
        int v = constrain(value, VOLUME_MIN, VOLUME_MAX);
        tda7318SetVolume(v);
        displayUpdateValue(v, 63);
        displayShowParam("Volume", v, 10000);
        btSPPSendUpdate("volume", v);
    }
    else if (strcmp(cmd, "set_bass") == 0) {
        int v = constrain(value, BASS_MIN, BASS_MAX);
        tda7318SetBass(v);
        displayUpdateValue(v - BASS_MIN, BASS_MAX - BASS_MIN);
        displayShowParam("Bass", v, 10000);
        btSPPSendUpdate("bass", v);
    }
    else if (strcmp(cmd, "set_treble") == 0) {
        int v = constrain(value, TREBLE_MIN, TREBLE_MAX);
        tda7318SetTreble(v);
        displayUpdateValue(v - TREBLE_MIN, TREBLE_MAX - TREBLE_MIN);
        displayShowParam("Treble", v, 10000);
        btSPPSendUpdate("treble", v);
    }
    else if (strcmp(cmd, "set_balance") == 0) {
        int v = constrain(value, BALANCE_MIN, BALANCE_MAX);
        tda7318SetBalance(v);
        displayUpdateValue(v - BALANCE_MIN, BALANCE_MAX - BALANCE_MIN);
        displayShowParam("Balance", v, 10000);
        btSPPSendUpdate("balance", v);
    }
    else if (strcmp(cmd, "set_gain") == 0) {
        int v = constrain(value, GAIN_MIN, GAIN_MAX);
        tda7318SetGain(v);
        displayUpdateValue(v, 3);
        displayShowParam("Gain", v, 10000);
        btSPPSendUpdate("gain", v);
    }
    else if (strcmp(cmd, "set_input") == 0) {
        if (value >= 0 && value < INPUT_COUNT) {
            AudioInput newInput = (AudioInput)value;
            tda7318SetInput(newInput);
            displayUpdateInput(newInput);
            displayUpdateValue(tda7318GetVolume(), 63);
            btSPPSendUpdate("input", value);
        }
    }
    else if (strcmp(cmd, "mute") == 0) {
        bool muted = (value != 0);
        tda7318SetMute(muted);
        displaySetMute(muted);
        btSPPSendUpdateBool("mute", muted);
    }
    else if (strcmp(cmd, "standby") == 0) {
        // TODO: викликати inputSetStandby(value != 0);
        btSPPSendUpdateBool("standby", value != 0);
    }
    else {
        ESP_LOGW(TAG, "Невідома команда: %s", cmd);
    }
}

// ============================================================================
// ESP-IDF SPP callback
// ============================================================================

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
    switch (event) {

        case ESP_SPP_INIT_EVT:
            // Bluedroid ініціалізував SPP — запускаємо сервер
            ESP_LOGI(TAG, "SPP ініціалізовано, запуск сервера...");
            esp_spp_start_srv(ESP_SPP_SEC_NONE,
                              ESP_SPP_ROLE_SLAVE,
                              0,             // scn = авто
                              "AmpControl"); // назва сервісу в SDP
            break;

        case ESP_SPP_SRV_OPEN_EVT:
            spp_handle    = param->srv_open.handle;
            spp_connected = true;
            ESP_LOGI(TAG, "SPP клієнт підключився, handle=%lu", spp_handle);
            // Відправити повний статус при підключенні
            btSPPSendFullStatus();
            break;

        case ESP_SPP_CLOSE_EVT:
            spp_handle    = 0;
            spp_connected = false;
            rx_len        = 0;
            ESP_LOGI(TAG, "SPP клієнт відключився");
            break;

        case ESP_SPP_DATA_IND_EVT: {
            // Акумулюємо дані до символу '\n' або заповнення буфера
            const uint8_t* data = param->data_ind.data;
            uint16_t       len  = param->data_ind.len;

            for (uint16_t i = 0; i < len; i++) {
                char c = (char)data[i];
                if (c == '\n' || c == '\r') {
                    if (rx_len > 0) {
                        rx_buf[rx_len] = '\0';
                        handleCommand(rx_buf);
                        rx_len = 0;
                    }
                } else if (rx_len < sizeof(rx_buf) - 1) {
                    rx_buf[rx_len++] = c;
                }
            }
            break;
        }

        case ESP_SPP_WRITE_EVT:
            // Дані відправлено, нічого не робимо
            break;

        case ESP_SPP_UNINIT_EVT:
            ESP_LOGW(TAG, "SPP деініціалізовано");
            break;

        default:
            break;
    }
}

// ============================================================================
// Публічний API
// ============================================================================

void btSPPInit() {
    // A2DP вже запустив Bluedroid → просто реєструємо SPP поверх нього
    esp_err_t ret = esp_spp_register_callback(spp_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_spp_register_callback failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_spp_init(ESP_SPP_MODE_CB);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_spp_init failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SPP зареєстровано поверх A2DP Bluedroid");
}

bool btSPPIsConnected() {
    return spp_connected;
}

bool btSPPSend(const char* json) {
    if (!spp_connected || spp_handle == 0) return false;

    uint16_t len = (uint16_t)strlen(json);
    // Додаємо '\n' як роздільник пакетів
    char buf[192];
    snprintf(buf, sizeof(buf), "%s\n", json);
    esp_err_t ret = esp_spp_write(spp_handle, strlen(buf), (uint8_t*)buf);
    return (ret == ESP_OK);
}

void btSPPSendFullStatus() {
    // Формуємо JSON статично, без ArduinoJson — економимо ~8 KB heap
    char json[192];
    snprintf(json, sizeof(json),
        "{\"type\":\"status\","
        "\"input\":%d,"
        "\"volume\":%d,"
        "\"bass\":%d,"
        "\"treble\":%d,"
        "\"balance\":%d,"
        "\"gain\":%d,"
        "\"mute\":%s,"
        "\"standby\":%s}",
        (int)tda7318GetInput(),
        tda7318GetVolume(),
        tda7318GetBass(),
        tda7318GetTreble(),
        tda7318GetBalance(),
        tda7318GetGain(),
        tda7318IsMuted()      ? "true" : "false",
        inputIsStandby()      ? "true" : "false"
    );
    btSPPSend(json);
    ESP_LOGI(TAG, "Відправлено статус: %s", json);
}

void btSPPSendUpdate(const char* param, int value) {
    if (!spp_connected) return;
    char json[80];
    snprintf(json, sizeof(json),
        "{\"type\":\"update\",\"param\":\"%s\",\"value\":%d}",
        param, value);
    btSPPSend(json);
}

void btSPPSendUpdateBool(const char* param, bool value) {
    if (!spp_connected) return;
    char json[80];
    snprintf(json, sizeof(json),
        "{\"type\":\"update\",\"param\":\"%s\",\"value\":%s}",
        param, value ? "true" : "false");
    btSPPSend(json);
}
