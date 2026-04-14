/**
 * @file tda7318.h
 * @brief Модуль для керування аудіопроцесором TDA7318 через I2C
 * 
 * Формат I2C: [START] [0x88] [SUB-ADDRESS] [DATA] [STOP]
 */
#ifndef TDA7318_H
#define TDA7318_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// Стан аудіопроцесора
typedef struct {
    uint8_t volume;      // Гучність 0-63
    int8_t bass;         // Бас -7 до +7
    int8_t treble;       // Високі -7 до +7
    int8_t balance;      // Баланс L/R -31 до +31 (0 = центр)
    AudioInput input;    // Поточний вхід
    uint8_t gain;        // Підсилення входу 0-3
    bool muted;          // Стан Mute
} TDA7318_State;

/** @brief Ініціалізація TDA7318 */
bool tda7318Init();

/** @brief Встановити гучність (0-63) */
void tda7318SetVolume(uint8_t volume);

/** @brief Отримати поточну гучність */
uint8_t tda7318GetVolume();

/** @brief Встановити рівень басів (-7 до +7) */
void tda7318SetBass(int8_t value);

/** @brief Отримати поточний рівень басів */
int8_t tda7318GetBass();

/** @brief Встановити рівень високих частот (-7 до +7) */
void tda7318SetTreble(int8_t value);

/** @brief Отримати поточний рівень високих частот */
int8_t tda7318GetTreble();

/** @brief Встановити баланс (-31 до +31) */
void tda7318SetBalance(int8_t value);

/** @brief Отримати поточний баланс */
int8_t tda7318GetBalance();

/** @brief Встановити підсилення (0-3) */
void tda7318SetGain(uint8_t value);

/** @brief Отримати поточне підсилення */
uint8_t tda7318GetGain();

/** @brief Перемкнути вхід */
void tda7318SetInput(AudioInput input, int8_t gain = -1);

/** @brief Отримати поточний вхід */
AudioInput tda7318GetInput();

/** @brief Увімкнути Mute */
void tda7318Mute();

/** @brief Вимкнути Mute */
void tda7318UnMute();

/** @brief Перемкнути стан Mute */
void tda7318SetMute(bool muted);

/** @brief Отримати стан Mute */
bool tda7318IsMuted();

/** @brief Отримати поточний стан */
TDA7318_State tda7318GetState();

/** @brief Встановити стан */
void tda7318SetState(const TDA7318_State& state);

/** @brief Вивести поточний стан в Serial */
void tda7318PrintState();

/** @brief Завантажити останній активний вхід з EEPROM */
AudioInput tda7318LoadLastInput();

#endif // TDA7318_H
