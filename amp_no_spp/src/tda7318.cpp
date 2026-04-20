/**
 * @file tda7318.cpp
 * @brief Модуль для керування аудіопроцесором TDA7318 через I2C
 * На основі бібліотеки з проекту SoundCube/WifiRadio
 */
#include "tda7318.h"
#include "config.h"
#include <EEPROM.h>

// Глобальна змінна стану
static TDA7318_State tdaState = {
    .volume = VOLUME_DEFAULT,
    .bass = BASS_DEFAULT,
    .treble = TREBLE_DEFAULT,
    .balance = BALANCE_DEFAULT,
    .input = INPUT_BLUETOOTH,
    .gain = GAIN_DEFAULT,
    .muted = false
};

// Відкладене збереження в EEPROM (щоб не блокувати CPU при кожному тіку енкодера)
static bool     settingsDirty  = false;
static uint32_t lastChangeMs   = 0;
static const uint32_t SAVE_DELAY_MS = 3000;  // зберігати через 3 с після останньої зміни

static inline void markDirty() {
    settingsDirty = true;
    lastChangeMs  = millis();
}

// Дефолтний gain для кожного входу (0-3)
static const uint8_t DEFAULT_INPUT_GAIN[] = {
    0,  // INPUT_BLUETOOTH
    0,  // INPUT_COMPUTER
    2,  // INPUT_TV_BOX
    0   // INPUT_AUX
};

/**
 * @brief Конвертувати гучність 0-63 в байт для TDA7318
 */
static uint8_t tda7318VolumeToReg(uint8_t volume) {
    if (volume > 63) volume = 63;
    return 63 - volume;
}

/**
 * @brief Конвертувати бас/високі в байт для TDA7318
 */
static uint8_t tda7318ToneToReg(int8_t value, uint8_t baseReg) {
    uint8_t temp = baseReg;
    if (value > 0) {
        temp += 8;
        temp += (7 - value);
    } else {
        temp += (7 + value);
    }
    return temp;
}

/**
 * @brief Встановити аттенюатор динаміка
 */
static uint8_t tda7318SpeakerAtt(int speaker, int gain) {
    uint8_t temp = 0;
    if (speaker == 1) temp = 0b10000000;
    else if (speaker == 2) temp = 0b10100000;
    else if (speaker == 3) temp = 0b11000000;
    else if (speaker == 4) temp = 0b11100000;
    temp += gain;
    return temp;
}

/**
 * @brief Встановити вхід та gain
 */
static uint8_t tda7318AudioSwitch(int channel, int gain) {
    uint8_t temp = 0b01000000;
    temp += ((3 - gain) << 3);
    temp += channel;
    return temp;
}

/**
 * @brief Завантажити налаштування для поточного входу з EEPROM
 */
static void tda7318LoadSettingsForInput(AudioInput input) {
    EEPROM.begin(EEPROM_SIZE);

    if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
        DEBUG_PRINTLN("[TDA7318] EEPROM не ініціалізовано, використовуємо дефолтні значення");
        EEPROM.end();
        return;
    }

    int addr = EEPROM_MAGIC_ADDR + 2 + (input * sizeof(InputSettings));

    InputSettings settings;
    EEPROM.get(addr, settings);

    tdaState.volume = settings.volume;
    if (tdaState.volume > 63) tdaState.volume = VOLUME_DEFAULT;
    tdaState.bass = settings.bass;
    tdaState.treble = settings.treble;
    tdaState.balance = settings.balance;
    tdaState.gain = settings.gain;

    EEPROM.end();

    DEBUG_PRINTLN("[TDA7318] Завантажено налаштування для входу " + String(input) +
                  ": vol=" + String(tdaState.volume) +
                  ", bass=" + String(tdaState.bass) +
                  ", treble=" + String(tdaState.treble) +
                  ", bal=" + String(tdaState.balance) +
                  ", gain=" + String(tdaState.gain));
}

/**
 * @brief Зберегти налаштування для поточного входу в EEPROM
 */
static void tda7318SaveSettings() {
    EEPROM.begin(EEPROM_SIZE);

    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    EEPROM.write(EEPROM_MAGIC_ADDR + 1, tdaState.input);

    int addr = EEPROM_MAGIC_ADDR + 2 + (tdaState.input * sizeof(InputSettings));

    InputSettings settings;
    settings.volume = tdaState.volume;
    settings.bass = tdaState.bass;
    settings.treble = tdaState.treble;
    settings.balance = tdaState.balance;
    settings.gain = tdaState.gain;
    settings.reserved[0] = 0;
    settings.reserved[1] = 0;

    EEPROM.put(addr, settings);
    EEPROM.commit();
    EEPROM.end();

    DEBUG_PRINTLN("[TDA7318] Збережено налаштування для входу " + String(tdaState.input));
}

bool tda7318Init() {
    DEBUG_PRINTLN("[TDA7318] Ініціалізація...");

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);  // Fast Mode — зменшує час блокування з ~100 до ~25 мкс
    delay(10);

    Wire.beginTransmission(TDA7318_ADDRESS);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
        DEBUG_PRINTLN("[TDA7318] Знайдено на I2C адресі 0x44");
    } else {
        DEBUG_PRINTLN("[TDA7318] ПОМИЛКА I2C! Код: " + String(error));
        DEBUG_PRINTLN("[TDA7318] Перевірте: SDA=GPIO" + String(I2C_SDA_PIN) + ", SCL=GPIO" + String(I2C_SCL_PIN));
    }

    // Завантажити останній активний вхід ПЕРЕД завантаженням налаштувань
    AudioInput lastInput = tda7318LoadLastInput();
    tdaState.input = lastInput;

    tda7318LoadSettingsForInput(tdaState.input);

    // Встановити початкові значення
    tda7318SetVolume(tdaState.volume);
    tda7318SetBass(tdaState.bass);
    tda7318SetTreble(tdaState.treble);
    tda7318SetInput(tdaState.input);

    // Встановити gain для всіх динаміків на максимум (0 = max gain)
    Wire.beginTransmission(TDA7318_ADDRESS);
    Wire.write(tda7318SpeakerAtt(1, 0));
    Wire.write(tda7318SpeakerAtt(2, 0));
    Wire.write(tda7318SpeakerAtt(3, 0));
    Wire.write(tda7318SpeakerAtt(4, 0));
    Wire.endTransmission();

    // Встановити баланс ПІСЛЯ налаштування speaker attenuation
    tda7318SetBalance(tdaState.balance);

    DEBUG_PRINTLN("[TDA7318] Ініціалізацію завершено");
    return true;
}

void tda7318SetVolume(uint8_t volume) {
    if (volume > 63) volume = 63;
    tdaState.volume = volume;

    Wire.beginTransmission(TDA7318_ADDRESS);
    Wire.write(tda7318VolumeToReg(volume));
    Wire.endTransmission();

    if (volume > 0 && tdaState.muted) {
        tda7318UnMute();
    }
    if (volume == 0 && !tdaState.muted) {
        tda7318Mute();
    }

    markDirty();
    DEBUG_PRINTLN("[TDA7318] Гучність: " + String(volume) + " / 63 (" + volume_to_db(volume) + " дБ)");
}

uint8_t tda7318GetVolume() {
    return tdaState.volume;
}

void tda7318SetBass(int8_t value) {
    if (value < BASS_MIN) value = BASS_MIN;
    if (value > BASS_MAX) value = BASS_MAX;
    tdaState.bass = value;

    Wire.beginTransmission(TDA7318_ADDRESS);
    Wire.write(tda7318ToneToReg(value, 96));  // 96 = 0b01100000
    Wire.endTransmission();

    markDirty();
    DEBUG_PRINTLN("[TDA7318] НЧ: " + String(value) + " (" + tone_to_db(value) + " дБ)");
}

int8_t tda7318GetBass() {
    return tdaState.bass;
}

void tda7318SetTreble(int8_t value) {
    if (value < TREBLE_MIN) value = TREBLE_MIN;
    if (value > TREBLE_MAX) value = TREBLE_MAX;
    tdaState.treble = value;

    Wire.beginTransmission(TDA7318_ADDRESS);
    Wire.write(tda7318ToneToReg(value, 112));  // 112 = 0b01110000
    Wire.endTransmission();

    markDirty();
    DEBUG_PRINTLN("[TDA7318] ВЧ: " + String(value) + " (" + tone_to_db(value) + " дБ)");
}

int8_t tda7318GetTreble() {
    return tdaState.treble;
}

void tda7318SetBalance(int8_t value) {
    if (value < BALANCE_MIN) value = BALANCE_MIN;
    if (value > BALANCE_MAX) value = BALANCE_MAX;
    tdaState.balance = value;

    uint8_t leftGain, rightGain;

    if (value < 0) {
        leftGain = 0;
        rightGain = abs(value);
    } else if (value > 0) {
        leftGain = value;
        rightGain = 0;
    } else {
        leftGain = 0;
        rightGain = 0;
    }

    Wire.beginTransmission(TDA7318_ADDRESS);
    Wire.write(0b10000000 | leftGain);
    Wire.write(0b10100000 | rightGain);
    Wire.endTransmission();

    markDirty();
    DEBUG_PRINTLN("[TDA7318] Баланс: " + String(value) + " (L: " + String(leftGain) + ", R: " + String(rightGain) + ")");
}

int8_t tda7318GetBalance() {
    return tdaState.balance;
}

void tda7318SetGain(uint8_t value) {
    if (value > 3) value = 3;
    tdaState.gain = value;
    markDirty();

    // Перевстановлюємо вхід з новим gain
    uint8_t regInput = tda7318AudioSwitch(tdaState.input, value);
    Wire.beginTransmission(TDA7318_ADDRESS);
    Wire.write(regInput);
    Wire.endTransmission();

    DEBUG_PRINTLN("[TDA7318] Підсилення: " + String(value));
}

uint8_t tda7318GetGain() {
    return tdaState.gain;
}

void tda7318SetInput(AudioInput input, int8_t gain) {
    if (input > INPUT_AUX) input = INPUT_BLUETOOTH;

    if (gain < 0) {
        gain = DEFAULT_INPUT_GAIN[input];
    }
    if (gain > 3) gain = 3;
    if (gain < 0) gain = 0;

    // Зберігаємо налаштування для попереднього входу
    tda7318SaveSettings();

    tdaState.input = input;

    // Завантажуємо налаштування для нового входу
    tda7318LoadSettingsForInput(input);

    // Встановлюємо вхід
    uint8_t regInput = tda7318AudioSwitch(input, gain);
    tdaState.gain = gain;

    Wire.beginTransmission(TDA7318_ADDRESS);
    Wire.write(regInput);
    Wire.endTransmission();

    // Встановлюємо збережені налаштування для нового входу
    tda7318SetVolume(tdaState.volume);
    tda7318SetBass(tdaState.bass);
    tda7318SetTreble(tdaState.treble);
    tda7318SetBalance(tdaState.balance);

    DEBUG_PRINTLN("[TDA7318] Вхід: " + String(input) + ", підсилення: " + String(gain));
}

AudioInput tda7318GetInput() {
    return tdaState.input;
}

void tda7318Mute() {
    if (tdaState.muted) return;

    tdaState.muted = true;

    Wire.beginTransmission(TDA7318_ADDRESS);
    Wire.write(0x3F);  // Max attenuation (mute)
    Wire.endTransmission();

    DEBUG_PRINTLN("[TDA7318] Mute увімкнено");
}

void tda7318UnMute() {
    if (!tdaState.muted) return;

    tdaState.muted = false;

    uint8_t regVolume = tda7318VolumeToReg(tdaState.volume);

    Wire.beginTransmission(TDA7318_ADDRESS);
    Wire.write(regVolume);
    Wire.endTransmission();

    DEBUG_PRINTLN("[TDA7318] Mute вимкнено, гучність: " + String(tdaState.volume));
}

void tda7318SetMute(bool muted) {
    if (muted) {
        tda7318Mute();
    } else {
        tda7318UnMute();
    }
}

bool tda7318IsMuted() {
    return tdaState.muted;
}

TDA7318_State tda7318GetState() {
    return tdaState;
}

void tda7318SetState(const TDA7318_State& state) {
    tda7318SetInput(state.input, state.gain);
    tda7318SetVolume(state.volume);
    tda7318SetBass(state.bass);
    tda7318SetTreble(state.treble);
    tda7318SetBalance(state.balance);
    tda7318SetMute(state.muted);
}

void tda7318PrintState() {
    Serial.println("========================================");
    Serial.println("  TDA7318 - Поточний стан");
    Serial.println("========================================");
    Serial.print("  Вхід:          ");
    switch (tdaState.input) {
        case INPUT_BLUETOOTH: Serial.println("Bluetooth"); break;
        case INPUT_COMPUTER:  Serial.println("Computer"); break;
        case INPUT_TV_BOX:    Serial.println("TV Box"); break;
        case INPUT_AUX:       Serial.println("AUX"); break;
        default:              Serial.println("Unknown"); break;
    }
    Serial.print("  Гучність:      ");
    Serial.print(tdaState.volume);
    Serial.print(" / 63 (");
    Serial.print(volume_to_db(tdaState.volume));
    Serial.println(" дБ)");

    Serial.print("  НЧ (Bass):     ");
    Serial.print(tdaState.bass);
    Serial.print(" (");
    Serial.print(tone_to_db(tdaState.bass));
    Serial.println(" дБ)");

    Serial.print("  ВЧ (Treble):   ");
    Serial.print(tdaState.treble);
    Serial.print(" (");
    Serial.print(tone_to_db(tdaState.treble));
    Serial.println(" дБ)");

    Serial.print("  Баланс:        ");
    Serial.println(tdaState.balance);

    Serial.print("  Підсилення:    ");
    Serial.print(tdaState.gain);
    Serial.println(" / 3");

    Serial.print("  Mute:          ");
    Serial.println(tdaState.muted ? "Так" : "Ні");

    Serial.println("========================================");
}

AudioInput tda7318LoadLastInput() {
    EEPROM.begin(EEPROM_SIZE);
    
    if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
        EEPROM.end();
        return INPUT_BLUETOOTH;
    }
    
    uint8_t lastInput = EEPROM.read(EEPROM_MAGIC_ADDR + 1);
    EEPROM.end();
    
    if (lastInput >= INPUT_COUNT) lastInput = INPUT_BLUETOOTH;
    
    DEBUG_PRINTLN("[TDA7318] Останній вхід з EEPROM: " + String(lastInput));
    return (AudioInput)lastInput;
}

// ============================================================================
// Відкладене збереження — викликати з loop()
// ============================================================================

void tda7318Loop() {
    if (!settingsDirty) return;
    if (millis() - lastChangeMs < SAVE_DELAY_MS) return;

    // 3 секунди без змін — зберігаємо
    tda7318SaveSettings();
    settingsDirty = false;
    DEBUG_PRINTLN("[TDA7318] Налаштування збережено (відкладено)");
}
