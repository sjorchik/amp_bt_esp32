#ifndef BT_SPP_H
#define BT_SPP_H

#include <Arduino.h>
#include <stdint.h>

/**
 * @brief Ініціалізація SPP поверх вже запущеного Bluedroid (A2DP).
 * 
 * ВАЖЛИВО: викликати ТІЛЬКИ після a2dp_sink.start()!
 * Не використовувати BluetoothSerial — вона конфліктує з A2DP.
 */
void btSPPInit();

/** Чи підключений Android клієнт по SPP */
bool btSPPIsConnected();

/**
 * @brief Відправити JSON-рядок клієнту.
 * @return true якщо відправлено, false якщо не підключено
 */
bool btSPPSend(const char* json);

/** Відправити повний статус при підключенні клієнта */
void btSPPSendFullStatus();

/** Відправити оновлення одного параметру */
void btSPPSendUpdate(const char* param, int value);
void btSPPSendUpdateBool(const char* param, bool value);

#endif // BT_SPP_H
