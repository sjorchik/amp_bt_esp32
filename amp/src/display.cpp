#include "display.h"
#include <TFT_eSPI.h>
#include <SPIFFS.h>
#include <FS.h>
#include "fonts/Arsenal_Bold40.h"
#include "fonts/Arsenal_Bold25.h"

static TFT_eSPI tft = TFT_eSPI();

static const char* inputLabels[] = {"BT", "PC", "TV", "AUX"};
static const char* inputNames[] = {"Bluetooth", "Computer", "TV Box", "AUX"};
static const char* inputImages[] = {"/bt_.bmp", "/pc_.bmp", "/tv_.bmp", "/aux_.bmp"};

static AudioInput lastInput = (AudioInput)255;
static uint8_t lastVolume = 255;

static void drawVolumeBar(uint8_t volume) {
    int barX = 290;
    int barY = 20;
    int barW = 20;
    int barH = 140;
    
    tft.fillRect(barX, barY, barW, barH, 0x18E3);
    
    int fillH = (volume * barH) / 63;
    int fillY = barY + barH - fillH;
    tft.fillRect(barX, fillY, barW, fillH, 0x001F);
    tft.drawRect(barX, barY, barW, barH, 0xFFFF);
}

static void drawInputImage() {
    if (lastInput >= INPUT_COUNT) return;
    
    tft.fillRect(60, 55, 230, 115, 0x0000);
    
    const char* imgPath = inputImages[lastInput];
    fs::File bmpFS = SPIFFS.open(imgPath, "r");
    if (!bmpFS) return;
    
    if (bmpFS.read() != 'B' || bmpFS.read() != 'M') {
        bmpFS.close();
        return;
    }
    
    bmpFS.seek(0);
    uint8_t header[54];
    bmpFS.read(header, 54);
    
    int32_t bmpWidth = *(int32_t*)&header[18];
    int32_t bmpHeight = *(int32_t*)&header[22];
    uint16_t bitsPerPixel = *(uint16_t*)&header[28];
    int32_t dataOffset = *(int32_t*)&header[10];
    
    int16_t drawX = 60 + (230 - bmpWidth) / 2;
    int16_t drawY = 55 + (115 - bmpHeight) / 2;
    
    if (bitsPerPixel == 24) {
        int32_t rowSize = (bmpWidth * 3 + 3) & ~3;
        uint8_t *allData = new uint8_t[rowSize * bmpHeight];
        bmpFS.seek(dataOffset);
        bmpFS.read(allData, rowSize * bmpHeight);
        
        uint16_t *imgBuf = new uint16_t[bmpWidth * bmpHeight];
        
        for (int32_t row = 0; row < bmpHeight; row++) {
            int32_t srcRow = bmpHeight - 1 - row;
            uint8_t *rowPtr = allData + srcRow * rowSize;
            
            for (int32_t col = 0; col < bmpWidth; col++) {
                uint8_t *pixelPtr = rowPtr + col * 3;
                uint8_t b = pixelPtr[0];
                uint8_t g = pixelPtr[1];
                uint8_t r = pixelPtr[2];
                imgBuf[row * bmpWidth + col] = (r & 0xF8) << 8 | (g & 0xFC) << 3 | (b >> 3);
            }
        }
        
        tft.setSwapBytes(true);
        tft.pushImage(drawX, drawY, bmpWidth, bmpHeight, imgBuf);
        tft.setSwapBytes(false);
        
        delete[] imgBuf;
        delete[] allData;
    }
    
    bmpFS.close();
}

static void drawInputName() {
    if (lastInput >= INPUT_COUNT) return;
    
    int nameX = 60;
    int nameY = 10;
    int nameW = 230;
    
    tft.fillRect(nameX, nameY, nameW, 45, 0x0000);
    
    tft.loadFont(Arsenal_Bold40);
    tft.setTextColor(0xFFFF);
    
    const char* name = inputNames[lastInput];
    int len = strlen(name);
    int textX = nameX + (nameW - len * 22) / 2;
    tft.setCursor(max(nameX, textX), nameY);
    tft.print(name);
    tft.unloadFont();
}

static void drawInputButtons() {
    int btnW = 50;
    int btnH = 32;
    int btnX = 5;
    int startY = 15;
    int gap = 6;
    
    tft.loadFont(Arsenal_Bold25);
    
    for (int i = 0; i < 4; i++) {
        int btnY = startY + i * (btnH + gap);
        int nameLen = strlen(inputLabels[i]);
        int textX = btnX + (btnW - nameLen * 14) / 2;
        int textY = btnY + 6;
        
        if (i == lastInput) {
            tft.fillRect(btnX, btnY, btnW, btnH, 0x001F);
            tft.drawRect(btnX, btnY, btnW, btnH, 0x07FF);
            tft.setTextColor(0x07FF);
        } else {
            tft.fillRect(btnX, btnY, btnW, btnH, 0x18E3);
            tft.drawRect(btnX, btnY, btnW, btnH, 0x39E7);
            tft.setTextColor(0xAD55);
        }
        
        tft.setCursor(textX, textY);
        tft.print(inputLabels[i]);
    }
    
    tft.unloadFont();
}

void displayInit() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(0x0000);
    
    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("[DISPLAY] SPIFFS помилка!");
    }
    
    drawInputButtons();
    drawInputName();
    drawInputImage();
    drawVolumeBar(0);
    
    DEBUG_PRINTLN("[DISPLAY] Ініціалізовано ST7789 320x170");
}

void displayUpdateInput(AudioInput input) {
    if (input == lastInput) return;
    lastInput = input;
    drawInputButtons();
    drawInputName();
    drawInputImage();
}

void displayUpdateVolume(uint8_t volume) {
    if (volume == lastVolume) return;
    lastVolume = volume;
    drawVolumeBar(volume);
}

void displayUpdateParameter(Parameter param, int16_t value) {}
void displayShowMessage(const char* message) {}
void displaySetStandby(bool standby) {}
void displaySetBTConnected(bool connected) {}
