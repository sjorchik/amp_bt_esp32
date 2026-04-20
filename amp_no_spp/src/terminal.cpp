#include "terminal.h"
#include "config.h"
#include "tda7318.h"
#include "input.h"
#include "display.h"

#define CMD_BUFFER_SIZE 64
static char cmdBuffer[CMD_BUFFER_SIZE];
static uint8_t cmdIndex = 0;

static void printHelp() {
    Serial.println("");
    Serial.println("========================================");
    Serial.println("  ESP32 Audio Controller - Команди");
    Serial.println("========================================");
    Serial.println("  help              - Допомога");
    Serial.println("  status            - Стан TDA7318");
    Serial.println("  vol <0-63>        - Гучність");
    Serial.println("  vol+/vol-         - +/- гучність");
    Serial.println("  bass <val>        - НЧ (-7..+7)");
    Serial.println("  treble <val>      - ВЧ (-7..+7)");
    Serial.println("  balance <val>     - Баланс (-31..+31)");
    Serial.println("  gain <0-3>        - Підсилення");
    Serial.println("  input <0-3>       - Вхід (0=BT,1=PC,2=TV,3=AUX)");
    Serial.println("  mute/unmute       - Mute");
    Serial.println("  param <0-4>       - Параметр енкодера");
    Serial.println("                    0=Vol,1=Bass,2=Tr,3=Bal,4=Gain");
    Serial.println("========================================");
    Serial.println("");
}

static void executeCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;
    
    int spaceIndex = cmd.indexOf(' ');
    String command = spaceIndex > 0 ? cmd.substring(0, spaceIndex) : cmd;
    String arg = spaceIndex > 0 ? cmd.substring(spaceIndex + 1) : "";
    
    command.toLowerCase();
    
    if (command == "help" || command == "h" || command == "?") {
        printHelp();
        return;
    }
    
    if (command == "status" || command == "st") {
        tda7318PrintState();
        return;
    }
    
    if (command == "param" || command == "p") {
        if (arg.length() == 0) {
            Serial.println("Поточний параметр: " + String(inputGetParameter()) + " - " + inputGetParameterName());
            return;
        }
        int val = arg.toInt();
        if (val < 0 || val >= PARAM_COUNT) {
            Serial.println("Помилка: 0-" + String(PARAM_COUNT - 1));
            return;
        }
        inputSetParameter((Parameter)val);
        Serial.println("Параметр встановлено: " + String(val) + " - " + inputGetParameterName());
        return;
    }
    
    if (command == "vol" || command == "volume") {
        if (arg.length() == 0) { Serial.println("Помилка: 0-63"); return; }
        int val = arg.toInt();
        if (val < 0 || val > 63) { Serial.println("Помилка: 0-63"); return; }
        tda7318SetVolume(val);
        return;
    }
    if (command == "vol+" || command == "v+") {
        int step = arg.length() > 0 ? arg.toInt() : 3;
        if (step < 1 || step > 30) step = 3;
        tda7318SetVolume(min(tda7318GetVolume() + step, 63));
        return;
    }
    if (command == "vol-" || command == "v-") {
        int step = arg.length() > 0 ? arg.toInt() : 3;
        if (step < 1 || step > 30) step = 3;
        tda7318SetVolume(max(tda7318GetVolume() - step, 0));
        return;
    }
    
    if (command == "bass" || command == "b") {
        if (arg.length() == 0) { Serial.println("Помилка: -7..+7"); return; }
        int val = arg.toInt();
        if (val < -7 || val > 7) { Serial.println("Помилка: -7..+7"); return; }
        tda7318SetBass(val);
        return;
    }
    if (command == "bass+" || command == "b+") { tda7318SetBass(min(tda7318GetBass() + 1, 7)); return; }
    if (command == "bass-" || command == "b-") { tda7318SetBass(max(tda7318GetBass() - 1, -7)); return; }
    
    if (command == "treble" || command == "t") {
        if (arg.length() == 0) { Serial.println("Помилка: -7..+7"); return; }
        int val = arg.toInt();
        if (val < -7 || val > 7) { Serial.println("Помилка: -7..+7"); return; }
        tda7318SetTreble(val);
        return;
    }
    if (command == "treble+" || command == "t+") { tda7318SetTreble(min(tda7318GetTreble() + 1, 7)); return; }
    if (command == "treble-" || command == "t-") { tda7318SetTreble(max(tda7318GetTreble() - 1, -7)); return; }
    
    if (command == "balance" || command == "bal") {
        if (arg.length() == 0) { Serial.println("Помилка: -31..+31"); return; }
        int val = arg.toInt();
        if (val < -31 || val > 31) { Serial.println("Помилка: -31..+31"); return; }
        tda7318SetBalance(val);
        return;
    }
    if (command == "bal+" || command == "bl+") { tda7318SetBalance(min(tda7318GetBalance() + 1, 31)); return; }
    if (command == "bal-" || command == "bl-") { tda7318SetBalance(max(tda7318GetBalance() - 1, -31)); return; }
    if (command == "bal-c" || command == "bl-c") { tda7318SetBalance(0); return; }
    
    if (command == "gain" || command == "g") {
        if (arg.length() == 0) { Serial.println("Помилка: 0-3"); return; }
        int val = arg.toInt();
        if (val < 0 || val > 3) { Serial.println("Помилка: 0-3"); return; }
        tda7318SetGain(val);
        return;
    }
    
    if (command == "input" || command == "in") {
        if (arg.length() == 0) { Serial.println("Помилка: 0-3"); return; }
        int val = arg.toInt();
        if (val < 0 || val > 3) { Serial.println("Помилка: 0-3"); return; }
        tda7318SetInput((AudioInput)val);
        return;
    }
    
    if (command == "mute" || command == "m") { tda7318Mute(); return; }
    if (command == "unmute" || command == "um") { tda7318UnMute(); return; }
    if (command == "mute-toggle" || command == "mt") { tda7318SetMute(!tda7318IsMuted()); return; }
    
    Serial.println("Помилка: невідома команда. Введіть 'help'.");
}

void terminalInit() {
    cmdIndex = 0;
}

void terminalHandleInput() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (cmdIndex > 0) {
                cmdBuffer[cmdIndex] = '\0';
                String cmd = String(cmdBuffer);
                cmdIndex = 0;
                Serial.print("> ");
                Serial.println(cmd);
                executeCommand(cmd);
                Serial.print("cmd> ");
            }
        } else if (c == '\b' || c == 127) {
            if (cmdIndex > 0) {
                cmdIndex--;
                Serial.print("\b \b");
            }
        } else if (c >= 32 && c < 127 && cmdIndex < CMD_BUFFER_SIZE - 1) {
            cmdBuffer[cmdIndex++] = c;
            Serial.print(c);
        }
    }
}
