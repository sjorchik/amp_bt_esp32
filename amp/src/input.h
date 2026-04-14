/**
 * @file input.h
 * @brief Модуль ввідних пристроїв: енкодер, кнопки, IR RC-5
 */
#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>
#include "config.h"

// Використовуємо визначення з config.h:
// - Parameter (PARAM_VOLUME, PARAM_BASS, тощо)
// - InputEvent, EventType (EVENT_ENCODER_ROTATE, тощо)
// - ButtonFunction (BTN_FUNC_PWR, тощо)

// ============================================================================
// ФУНКЦІЇ ІНІЦІАЛІЗАЦІЇ
// ============================================================================

/** @brief Ініціалізація всіх ввідних пристроїв */
void inputInit();

/** @brief Встановити поточний параметр для енкодера */
void inputSetParameter(Parameter param);

/** @brief Отримати поточний параметр енкодера */
Parameter inputGetParameter();

/** @brief Отримати назву поточного параметру */
const char* inputGetParameterName();

// ============================================================================
// ФУНКЦІЇ ОБРОБКИ (викликати в loop)
// ============================================================================

/** @brief Обробка всіх ввідних пристроїв
 *  @return Подія вводу, або EVENT_NONE якщо нічого немає
 */
InputEvent inputPoll();

/** @brief Обробка команд IR RC-5 */
InputEvent inputPollIR();

/** @brief Обробка енкодера */
InputEvent inputPollEncoder();

/** @brief Обробка кнопок */
InputEvent inputPollButtons();

#endif // INPUT_H
