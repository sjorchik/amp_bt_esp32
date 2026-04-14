#include "display.h"
#include <TFT_eSPI.h>
#include <SPIFFS.h>

static TFT_eSPI tft = TFT_eSPI();

static const char* inputNames[] = {"Bluetooth", "Computer", "TV Box", "AUX"};
static const char* paramNames[] = {"Гучність", "НЧ", "ВЧ", "Баланс", "Підсилення"};

static uint8_t lastVolume = 255;
static AudioInput lastInput = (AudioInput)255;
static Parameter lastParam = (Parameter)255;
static int16_t lastParamValue = 32767;
static bool lastStandby = false;
static bool lastBTConnected = false;

static bool fontsLoaded = false;

static void loadFonts() {
    if (fontsLoaded) return;
    
    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("[DISPLAY] SPIFFS помилка!");
        return;
    }
    
    if (SPIFFS.exists("/Arsenal-Bold15.vlw")) {
        DEBUG_PRINTLN("[DISPLAY] Шрифти Arsenal знайдено в SPIFFS");
    } else {
        DEBUG_PRINTLN("[DISPLAY] Шрифти не знайдено! Завантажте: platformio run --target uploadfs");
    }
    
    fontsLoaded = true;
}

static void drawVolumeBar(uint8_t volume) {
    int barX = 20;
    int barY = 75;
    int barW = 280;
    int barH = 35;
    
    tft.fillRect(barX, barY, barW, barH, 0x18E3);
    
    int fillW = (volume * barW) / 63;
    uint16_t color;
    if (volume < 30) color = 0x07E0;
    else if (volume < 50) color = 0xFFE0;
    else color = 0xF800;
    
    tft.fillRect(barX, barY, fillW, barH, color);
    tft.drawRect(barX, barY, barW, barH, 0xFFFF);
    
    tft.loadFont("Arsenal-Bold20");
    tft.setTextColor(0xFFFF);
    tft.setCursor(barX + 90, barY + 5);
    tft.print(volume);
    tft.print(" / 63");
    tft.unloadFont();
}

static void drawHeader() {
    tft.fillRect(0, 0, 320, 35, 0x001F);
    
    tft.loadFont("Arsenal-Bold20");
    tft.setTextColor(0xFFFF);
    tft.setCursor(10, 6);
    tft.print("ESP32 Audio");
    
    tft.unloadFont();
    tft.loadFont("Arsenal-Bold15");
    tft.setTextColor(0xFFFF);
    tft.setCursor(180, 10);
    const char* inp = (lastInput < INPUT_COUNT) ? inputNames[lastInput] : "Unknown";
    tft.print("In: ");
    tft.print(inp);
    tft.unloadFont();
    
    if (lastBTConnected) {
        tft.loadFont("Arsenal-Bold15");
        tft.setTextColor(0x07FF);
        tft.setCursor(280, 10);
        tft.print("BT");
        tft.unloadFont();
    }
}

static void drawFooter() {
    tft.fillRect(0, 135, 320, 35, 0x001F);
    
    tft.loadFont("Arsenal-Bold15");
    tft.setTextColor(0xFFFF);
    tft.setCursor(10, 145);
    
    if (lastStandby) {
        tft.setTextColor(0xF800);
        tft.print("STANDBY");
    } else {
        const char* pname = (lastParam < PARAM_COUNT) ? paramNames[lastParam] : "";
        tft.print("Param: ");
        tft.print(pname);
    }
    tft.unloadFont();
}

void displayInit() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(0x0000);
    
    loadFonts();
    
    tft.loadFont("Arsenal-Bold26");
    tft.setTextColor(0xFFFF);
    tft.setCursor(60, 50);
    tft.print("ESP32 Audio");
    tft.unloadFont();
    
    tft.loadFont("Arsenal-Bold20");
    tft.setCursor(80, 95);
    tft.print("Controller");
    tft.unloadFont();
    delay(1500);
    
    tft.fillScreen(0x0000);
    drawHeader();
    drawFooter();
    
    DEBUG_PRINTLN("[DISPLAY] Ініціалізовано ST7789 320x170");
}

void displayUpdateVolume(uint8_t volume) {
    if (volume == lastVolume) return;
    lastVolume = volume;
    
    drawVolumeBar(volume);
}

void displayUpdateInput(AudioInput input) {
    if (input == lastInput) return;
    lastInput = input;
    
    drawHeader();
}

void displayUpdateParameter(Parameter param, int16_t value) {
    if (param == lastParam && value == lastParamValue) return;
    lastParam = param;
    lastParamValue = value;
    
    tft.fillRect(0, 35, 320, 100, 0x0000);
    
    tft.loadFont("Arsenal-Bold20");
    tft.setTextColor(0xFFFF);
    tft.setCursor(10, 45);
    
    const char* pname = (param < PARAM_COUNT) ? paramNames[param] : "";
    tft.print(pname);
    tft.unloadFont();
    
    tft.loadFont("Arsenal-Bold26");
    tft.setTextColor(0x07FF);
    tft.setCursor(140, 50);
    tft.print(value);
    tft.unloadFont();
    
    drawFooter();
}

void displayShowMessage(const char* message) {
    tft.fillRect(0, 35, 320, 100, 0x0000);
    
    tft.loadFont("Arsenal-Bold20");
    tft.setTextColor(0x07FF);
    
    int len = strlen(message);
    int x = (320 - len * 14) / 2;
    tft.setCursor(max(10, x), 70);
    tft.print(message);
    tft.unloadFont();
}

void displaySetStandby(bool standby) {
    if (standby == lastStandby) return;
    lastStandby = standby;
    
    if (standby) {
        tft.fillScreen(0x0000);
        tft.loadFont("Arsenal-Bold26");
        tft.setTextColor(0xF800);
        tft.setCursor(70, 50);
        tft.print("STANDBY");
        tft.unloadFont();
    } else {
        tft.fillScreen(0x0000);
        drawHeader();
        drawFooter();
    }
}

void displaySetBTConnected(bool connected) {
    if (connected == lastBTConnected) return;
    lastBTConnected = connected;
    drawHeader();
}
