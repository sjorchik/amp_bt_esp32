#include "display.h"
#include <TFT_eSPI.h>
#include <SPIFFS.h>
#include <FS.h>
#include "fonts/Arsenal_Bold40.h"
#include "fonts/Arsenal_Bold25.h"
#include "tda7318.h"

static TFT_eSPI tft = TFT_eSPI();

static const char* inputLabels[] = {"BT", "PC", "TV", "AUX"};
static const char* inputNames[] = {"Bluetooth", "Computer", "TV Box", "AUX"};
static const char* inputImages[] = {"/bt_.bmp", "/pc_.bmp", "/tv_.bmp", "/aux_.bmp"};

static AudioInput lastInput = (AudioInput)255;
static bool isMuted = false;
static bool btConnected = false;
static bool btIsPlaying = false;
static char btArtist[64] = {0};
static char btTitle[64] = {0};
static char btAlbum[64] = {0};

static bool showTempMessage = false;
static char tempLabel[20] = {0};
static uint32_t tempMessageTimeout = 0;

static const int CONTENT_CX = 175;

static void drawBar(int16_t value, int16_t maxVal, uint16_t fillColor) {
    int barX = 290;
    int barY = 20;
    int barW = 20;
    int barH = 140;
    
    tft.fillRect(barX, barY, barW, barH, 0x18E3);
    
    int32_t fillH = ((int32_t)value * barH) / maxVal;
    if (fillH < 0) fillH = 0;
    if (fillH > barH) fillH = barH;
    
    int fillY = barY + barH - fillH;
    tft.fillRect(barX, fillY, barW, fillH, fillColor);
    tft.drawRect(barX, barY, barW, barH, 0xFFFF);
}

static void drawText(const char* text, uint16_t color, int y) {
    tft.loadFont(Arsenal_Bold40);
    tft.setTextColor(color);
    uint16_t w = tft.textWidth(text);
    tft.setCursor(CONTENT_CX - w / 2, y);
    tft.print(text);
    tft.unloadFont();
}

static void drawTrimmedText(const char* text, int centerX, int y, int maxWidth, uint16_t color) {
    if (!text || strlen(text) == 0) return;
    
    tft.loadFont(Arsenal_Bold25);
    uint16_t textW = tft.textWidth(text);
    
    if (textW <= maxWidth) {
        tft.setTextColor(color);
        tft.setCursor(centerX - textW / 2, y);
        tft.print(text);
    } else {
        char trimmed[64];
        strncpy(trimmed, text, sizeof(trimmed) - 4);
        trimmed[sizeof(trimmed) - 4] = '\0';
        
        while (tft.textWidth(trimmed) + tft.textWidth("...") > maxWidth && strlen(trimmed) > 3) {
            trimmed[strlen(trimmed) - 1] = '\0';
        }
        strcat(trimmed, "...");
        
        tft.setTextColor(color);
        tft.setCursor(60, y);
        tft.print(trimmed);
    }
    tft.unloadFont();
}

static void drawInputImage() {
    if (lastInput >= INPUT_COUNT) return;
    
    tft.fillRect(60, 55, 230, 115, 0x0000);
    
    if (lastInput == INPUT_BLUETOOTH && btConnected) {
        if (strlen(btTitle) > 0 || strlen(btArtist) > 0 || strlen(btAlbum) > 0) {
            uint16_t albumColor = btIsPlaying ? 0x9772 : 0xF800;
            uint16_t artistColor = btIsPlaying ? 0xFFE0  : 0xF800;
            uint16_t titleColor = btIsPlaying ? 0x867D  : 0xF800;
            
            drawTrimmedText(btAlbum, CONTENT_CX, 60, 230, albumColor);
            drawTrimmedText(btArtist, CONTENT_CX, 95, 230, artistColor);
            drawTrimmedText(btTitle, CONTENT_CX, 130, 230, titleColor);
        } else {
            tft.loadFont(Arsenal_Bold25);
            tft.setTextColor(0x07FF);
            uint16_t w = tft.textWidth("Connected");
            tft.setCursor(CONTENT_CX - w / 2, 90);
            tft.print("Connected");
            tft.unloadFont();
        }
        return;
    }
    
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
    
    int16_t drawX = CONTENT_CX - bmpWidth / 2;
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
    tft.fillRect(60, 10, 230, 45, 0x0000);
    drawText(inputNames[lastInput], 0xFFFF, 10);
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
    drawBar(0, 63, 0x001F);
    
    DEBUG_PRINTLN("[DISPLAY] Ініціалізовано ST7789 320x170");
}

void displayUpdateInput(AudioInput input) {
    if (input == lastInput) return;
    lastInput = input;
    showTempMessage = false;
    drawInputButtons();
    drawInputName();
    drawInputImage();
}

void displayUpdateValue(int16_t value, int16_t maxVal) {
    uint16_t color = isMuted ? 0xF800 : 0x001F;
    drawBar(value, maxVal, color);
}

void displaySetMute(bool muted) {
    isMuted = muted;
    uint16_t color = muted ? 0xF800 : 0x001F;
    drawBar(tda7318GetVolume(), 63, color);
}

void displayShowParam(const char* label, int16_t value, uint32_t timeoutMs) {
    showTempMessage = true;
    strncpy(tempLabel, label, sizeof(tempLabel) - 1);
    tempLabel[sizeof(tempLabel) - 1] = '\0';
    tempMessageTimeout = millis() + timeoutMs;
    
    tft.fillRect(60, 10, 230, 45, 0x0000);
    
    char buf[30];
    snprintf(buf, sizeof(buf), "%s %d", label, value);
    drawText(buf, 0x07FF, 10);
}

void displayLoop() {
    if (showTempMessage && millis() > tempMessageTimeout) {
        showTempMessage = false;
        drawInputName();
        drawInputImage();
        drawBar(tda7318GetVolume(), 63, isMuted ? 0xF800 : 0x001F);
    }
}

void displaySetStandby(bool standby) {
    if (standby) {
        tft.fillScreen(0x0000);
        digitalWrite(TFT_BL, LOW);
    } else {
        digitalWrite(TFT_BL, HIGH);
        drawInputButtons();
        drawInputName();
        drawInputImage();
        drawBar(0, 63, 0x001F);
    }
}
void displaySetBTConnected(bool connected) {
    if (connected == btConnected) return;
    btConnected = connected;
    
    if (lastInput == INPUT_BLUETOOTH) {
        drawInputImage();
    }
}

void displayUpdateBTTrackInfo(const char* artist, const char* title, const char* album) {
    if (lastInput != INPUT_BLUETOOTH || !btConnected) return;
    
    if (artist) strncpy(btArtist, artist, sizeof(btArtist) - 1);
    if (title) strncpy(btTitle, title, sizeof(btTitle) - 1);
    if (album) strncpy(btAlbum, album, sizeof(btAlbum) - 1);
    
    drawInputImage();
}

void displaySetBTPlaying(bool playing) {
    if (playing == btIsPlaying) return;
    btIsPlaying = playing;
    
    if (lastInput == INPUT_BLUETOOTH && btConnected) {
        drawInputImage();
    }
}
