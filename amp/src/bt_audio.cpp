#include "bt_audio.h"
#include "config.h"
#include "display.h"
#include <driver/i2s.h>
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
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            Serial.println("[BT] Від'єднано");
            isConnected = false;
            isPlaying = false;
            deviceName[0] = '\0';
            displaySetBTConnected(false);
            break;
        default:
            break;
    }
}

static void avrc_metadata_callback(uint8_t id, const uint8_t* text) {
    switch (id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            Serial.printf("[BT] Title: %s\n", text);
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            Serial.printf("[BT] Artist: %s\n", text);
            break;
        case ESP_AVRC_MD_ATTR_ALBUM:
            Serial.printf("[BT] Album: %s\n", text);
            break;
    }
}

void btAudioInit() {
    Serial.println("[BT_AUDIO] Ініціалізація Bluetooth A2DP...");
    
    i2s_pin_config_t my_pin_config = {
        .bck_io_num = I2S_BCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    a2dp_sink.set_pin_config(my_pin_config);
    a2dp_sink.set_auto_reconnect(true);
    a2dp_sink.set_on_connection_state_changed(connection_state_changed);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    
    Serial.printf("[BT_AUDIO] I2S піни: BCK=%d, WS=%d, DOUT=%d\n", I2S_BCK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
    
    a2dp_sink.start("ESP32-Audio");
    Serial.println("[BT_AUDIO] Bluetooth активний, очікування підключення...");
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

bool btAudioIsConnected() {
    return isConnected;
}

const char* btAudioGetDeviceName() {
    return deviceName;
}

void btAudioPlayPause() {
    if (isConnected) {
        if (isPlaying) {
            a2dp_sink.pause();
            isPlaying = false;
            Serial.println("[BT] Pause");
        } else {
            a2dp_sink.play();
            isPlaying = true;
            Serial.println("[BT] Play");
        }
    }
}

void btAudioNext() {
    if (isConnected) {
        a2dp_sink.next();
        Serial.println("[BT] Next");
    }
}

void btAudioPrev() {
    if (isConnected) {
        a2dp_sink.previous();
        Serial.println("[BT] Previous");
    }
}

void btAudioStop() {
    if (isConnected) {
        a2dp_sink.stop();
        isPlaying = false;
        Serial.println("[BT] Stop");
    }
}
