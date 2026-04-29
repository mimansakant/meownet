#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <string.h>
#include "monitor.h"

// ─── Hardware ────────────────────────────────────────────────────────────────
const uint8_t SENSOR_PINS[]  = {15, 12, 32, 33, 27};
const int     NUM_CHANNELS   = 5;
const unsigned long DEBOUNCE_MS = 200;

// ─── Ball tracking ───────────────────────────────────────────────────────────
static uint32_t      ballCount[NUM_CHANNELS]    = {0};
static bool          lastState[NUM_CHANNELS]    = {false};
static unsigned long lastTriggerMs[NUM_CHANNELS] = {0};

// ─── RTLola ──────────────────────────────────────────────────────────────────
static Memory rtlola_memory;

// ─── ESP-NOW ─────────────────────────────────────────────────────────────────
// After flashing each motor ESP32, read its MAC from Serial and paste it here.
static uint8_t motor0Mac[] = {0xA0, 0xDD, 0x6C, 0x73, 0xA4, 0x50};  // Left motor
static uint8_t motor1Mac[] = {0xA0, 0xDD, 0x6C, 0x74, 0xC0, 0x50};  // Right motor

// cmd=1 starts oscillation, cmd=0 stops it.
static void sendMotorCommand(uint8_t* mac, int8_t cmd) {
    esp_err_t result = esp_now_send(mac, (uint8_t*)&cmd, sizeof(cmd));
    Serial.printf("[ESP-NOW] %s %s\n",
                  cmd ? "START" : "STOP",
                  result == ESP_OK ? "sent" : "FAILED");
}

// ─── RTLola verdict handler ───────────────────────────────────────────────────
static void handleVerdict(Verdict& verdict) {
    // Trigger 0: ch1+ch2 heavy → right motor starts, left motor stops
    if (verdict.trigger_0_is_present && verdict.trigger_0) {
        sendMotorCommand(motor1Mac, 1);
        sendMotorCommand(motor0Mac, 0);
        Serial.println("[DIST] Left heavy — right motor starting, left stopping");
        memset(ballCount, 0, sizeof(ballCount));
    }

    // Trigger 1: ch4+ch5 heavy → left motor starts, right motor stops
    if (verdict.trigger_1_is_present && verdict.trigger_1) {
        sendMotorCommand(motor0Mac, 1);
        sendMotorCommand(motor1Mac, 0);
        Serial.println("[DIST] Right heavy — left motor starting, right stopping");
        memset(ballCount, 0, sizeof(ballCount));
    }

    // Trigger 2: balanced → stop both motors
    if (verdict.trigger_2_is_present && verdict.trigger_2) {
        sendMotorCommand(motor0Mac, 0);
        sendMotorCommand(motor1Mac, 0);
        Serial.println("[DIST] Balanced — both motors stopping");
        memset(ballCount, 0, sizeof(ballCount));
    }
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) delay(10);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        pinMode(SENSOR_PINS[i], INPUT_PULLDOWN);
    }

    memset(&rtlola_memory, 0, sizeof(rtlola_memory));

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init failed!");
        return;
    }

    esp_now_peer_info_t peer = {};
    peer.channel = 0;
    peer.encrypt = false;

    memcpy(peer.peer_addr, motor0Mac, 6);
    esp_now_add_peer(&peer);

    memcpy(peer.peer_addr, motor1Mac, 6);
    esp_now_add_peer(&peer);

    Serial.println("[BALL COLLECTOR] Ready");
    Serial.printf("[INFO] MAC: %s\n", WiFi.macAddress().c_str());
    Serial.println("[INFO] Waiting for balls...\n");
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    bool anyBall = false;
    unsigned long now = millis();


    for (int i = 0; i < NUM_CHANNELS; i++) {
        bool current = (digitalRead(SENSOR_PINS[i]) == HIGH);

        if (current && !lastState[i]) {
            if (now - lastTriggerMs[i] >= DEBOUNCE_MS) {
                lastTriggerMs[i] = now;
                ballCount[i]++;
                anyBall = true;
                Serial.printf("[BALL] ch%d count=%lu\n", i + 1, ballCount[i]);
            }
        }
        lastState[i] = current;
    }

    if (anyBall) {
        InternalEvent event = {
            .count_1          = (double)ballCount[0],
            .count_1_is_present = true,
            .count_2          = (double)ballCount[1],
            .count_2_is_present = true,
            .count_3          = (double)ballCount[2],
            .count_3_is_present = true,
            .count_4          = (double)ballCount[3],
            .count_4_is_present = true,
            .count_5          = (double)ballCount[4],
            .count_5_is_present = true,
            .time             = now / 1000.0
        };

        Verdict verdict = cycle(&rtlola_memory, event);
        handleVerdict(verdict);
    }

    // Print status every 10 seconds
    static unsigned long lastStatus = 0;
    if (now - lastStatus > 10000) {
        Serial.printf("[STATUS] ch1=%lu ch2=%lu ch3=%lu ch4=%lu ch5=%lu\n",
                      ballCount[0], ballCount[1], ballCount[2],
                      ballCount[3], ballCount[4]);
        lastStatus = now;
    }

    delay(10);
}
