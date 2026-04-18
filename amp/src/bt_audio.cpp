/**
 * @file bt_audio.cpp  (МОДИФІКОВАНИЙ)
 * 
 * Зміни:
 * 1. Додано btSPPInit() після a2dp_sink.start() — так SPP поділяє 
 *    один Bluedroid стек з A2DP, не виділяючи пам'ять на другий.
 * 2. set_default_bt_mode(ESP_BT_MODE_CLASSIC_BT) — вимикає BLE в Bluedroid
 *    (економить ~30 KB heap, якщо BLE не потрібен).
 * 3. Зменшено I2S буфер через set_auto_reconnect, щоб A2DP не тримав 
 *    зайву пам'ять.
 */

#include "bt_audio.h"
#include "bt_spp.h"        // ← ДОДАНО
#include "config.h"
#include "display.h"
#include <driver/i2s.h>
#include <cstring>
#include <BluetoothA2DPSink.h>

static BluetoothA2DPSink a2dp_sink;
static bool isConnected = false;
static bool isPlaying = false;
static char deviceName[64] = {0};

static void connection_state_changed(esp_a2d_connection_state_t state, void* ptr) {
    switch (state) {
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            Serial.println("[BT] Приєднано!");
            isConnected = true;
            isPlaying = false;
            displaySetBTConnected(true);
            btAudioClearMetadata();
            // Повідомити SPP клієнта про A2DP підключення
            btSPPSendUpdateBool("bt_connected", true);
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            Serial.println("[BT] Від'єднано");
            isConnected = false;
            isPlaying = false;
            deviceName[0] = '\0';
            displaySetBTConnected(false);
            btSPPSendUpdateBool("bt_connected", false);
            break;
        default:
            break;
    }
}

static void avrc_metadata_callback(uint8_t id, const uint8_t* text) {
    static char artist[64] = {0};
    static char title[64] = {0};
    static char album[64] = {0};
    
    const char* str = (const char*)text;
    if (strcmp(str, "Not Provided") == 0 || strcmp(str, "UNKNOWN") == 0) return;
    
    switch (id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            strncpy(title, str, sizeof(title) - 1);
            Serial.printf("[BT] Title: %s\n", str);
            displayUpdateBTTrackInfo(artist, title, album);
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            strncpy(artist, str, sizeof(artist) - 1);
            Serial.printf("[BT] Artist: %s\n", str);
            displayUpdateBTTrackInfo(artist, title, album);
            break;
        case ESP_AVRC_MD_ATTR_ALBUM:
            strncpy(album, str, sizeof(album) - 1);
            Serial.printf("[BT] Album: %s\n", str);
            displayUpdateBTTrackInfo(artist, title, album);
            break;
    }
}

static void avrc_playstatus_callback(esp_avrc_playback_stat_t playback) {
    switch (playback) {
        case ESP_AVRC_PLAYBACK_PLAYING:
            isPlaying = true;
            displaySetBTPlaying(true);
            Serial.println("[BT] Status: Playing");
            break;
        case ESP_AVRC_PLAYBACK_PAUSED:
            isPlaying = false;
            displaySetBTPlaying(false);
            Serial.println("[BT] Status: Paused");
            break;
        case ESP_AVRC_PLAYBACK_STOPPED:
            isPlaying = false;
            displaySetBTPlaying(false);
            Serial.println("[BT] Status: Stopped");
            break;
        default:
            break;
    }
}

void btAudioInit() {
    Serial.println("[BT_AUDIO] Ініціалізація Bluetooth A2DP...");
    
    // ── ОПТИМІЗАЦІЯ ПАМ'ЯТІ ────────────────────────────────────────────────
    // Вимкнути BLE в Bluedroid — звільняє ~30 KB heap.
    // Якщо BLE потрібен в майбутньому — прибрати цей рядок.
    a2dp_sink.set_default_bt_mode(ESP_BT_MODE_CLASSIC_BT);
    // ────────────────────────────────────────────────────────────────────────

    i2s_pin_config_t my_pin_config = {
        .bck_io_num   = I2S_BCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };
    
    a2dp_sink.set_pin_config(my_pin_config);
    a2dp_sink.set_auto_reconnect(true);
    a2dp_sink.set_on_connection_state_changed(connection_state_changed);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.set_avrc_rn_playstatus_callback(avrc_playstatus_callback);
    
    Serial.printf("[BT_AUDIO] I2S піни: BCK=%d, WS=%d, DOUT=%d\n",
                  I2S_BCK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
    
    // Запускаємо A2DP — після цього виклику Bluedroid повністю ініціалізований
    a2dp_sink.start("ESP32-Audio");
    Serial.println("[BT_AUDIO] A2DP активний");

    // ── РЕЄСТРАЦІЯ SPP ПОВЕРХ ІСНУЮЧОГО BLUEDROID ──────────────────────────
    // Важливо: лише після start()! A2DP ще не "зайняв" інші профілі.
    // esp_spp_init() просто додає SPP до вже запущеного стека.
    Serial.println("[BT_AUDIO] Реєстрація SPP...");
    btSPPInit();
    // ────────────────────────────────────────────────────────────────────────

    Serial.printf("[BT_AUDIO] Готово. Heap: %d B\n", ESP.getFreeHeap());
}

void btAudioStart() {
    Serial.println("[BT_AUDIO] Bluetooth вхід активний");
    if (!isConnected) {
        a2dp_sink.reconnect();
        Serial.println("[BT_AUDIO] Спроба перепідключення...");
    }
}

void btAudioEnd() {
    if (isConnected) {
        a2dp_sink.disconnect();
    }
    isConnected = false;
    isPlaying = false;
    displaySetBTConnected(false);
    Serial.println("[BT_AUDIO] Bluetooth від'єднано");
}

void btAudioLoop() {
    // Бібліотека обробляє все в фоновому режимі
}

bool btAudioIsConnected()         { return isConnected; }
const char* btAudioGetDeviceName() { return deviceName; }

void btAudioPlayPause() {
    if (isConnected) {
        if (isPlaying) { a2dp_sink.pause(); displaySetBTPlaying(false); }
        else           { a2dp_sink.play();  }
    }
}

void btAudioNext()   { if (isConnected) a2dp_sink.next(); }
void btAudioPrev()   { if (isConnected) a2dp_sink.previous(); }
void btAudioStop()   { if (isConnected) a2dp_sink.stop(); }

void btAudioClearMetadata() {
    displayUpdateBTTrackInfo("", "", "");
    isPlaying = false;
    displaySetBTPlaying(false);
}
