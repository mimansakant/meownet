/**
 * Motor ESP32 - Cat Plinko Ball Deflector
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define MOTOR_IN1 32
#define MOTOR_IN2 27
#define MOTOR_IN3 26
#define MOTOR_IN4 13

#define STEPS_PER_ADJUSTMENT 4096
#define STEP_DELAY_US 5000

volatile bool oscillating = false;
static int stepIndex = 0;
static int dirMultiplier = 1;  // flipped for left motor based on MAC

typedef struct __attribute__((packed)) {
    int8_t adjustment;
} MotorFlag;

const int stepSequence[4][4] = {
    {1, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 1},
    {1, 0, 0, 1}
};

void setMotorPins(int step) {
    digitalWrite(MOTOR_IN1, stepSequence[step][0]);
    digitalWrite(MOTOR_IN2, stepSequence[step][1]);
    digitalWrite(MOTOR_IN3, stepSequence[step][2]);
    digitalWrite(MOTOR_IN4, stepSequence[step][3]);
}

void powerOffMotor() {
    digitalWrite(MOTOR_IN1, 0);
    digitalWrite(MOTOR_IN2, 0);
    digitalWrite(MOTOR_IN3, 0);
    digitalWrite(MOTOR_IN4, 0);
}

void moveSteps(int steps) {
    int direction = steps > 0 ? 1 : -1;
    int absSteps = abs(steps);

    for (int i = 0; i < absSteps; i++) {
        stepIndex = (stepIndex + direction + 4) % 4;
        setMotorPins(stepIndex);
        delayMicroseconds(STEP_DELAY_US);
    }
    powerOffMotor();
}

void moveForDuration(int direction, unsigned long durationMs) {
    unsigned long start = millis();
    while (millis() - start < durationMs) {
        stepIndex = (stepIndex + direction + 4) % 4;
        setMotorPins(stepIndex);
        delayMicroseconds(STEP_DELAY_US);
    }
    powerOffMotor();
}

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    int8_t cmd = (int8_t)data[0];
    if (cmd != 0) {
        oscillating = true;
        Serial.println("[ESP-NOW] Command: START oscillating");
    } else {
        oscillating = false;
        Serial.println("[ESP-NOW] Command: STOP oscillating");
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) delay(10);

    Serial.println("[MOTOR] Cat Plinko Motor ESP32 starting...");

    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_IN3, OUTPUT);
    pinMode(MOTOR_IN4, OUTPUT);
    powerOffMotor();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init FAILED");
        return;
    }

    esp_now_register_recv_cb(onDataReceived);
    Serial.println("[ESP-NOW] Ready — listening for commands");

    // Left motor MAC: A0:DD:6C:73:A4:50 — flip direction so it mirrors right motor
    uint8_t mac[6];
    WiFi.macAddress(mac);
    if (mac[3] == 0x73) dirMultiplier = -1;

    Serial.printf("[INFO] This ESP32 MAC: %s dirMultiplier=%d\n",
                  WiFi.macAddress().c_str(), dirMultiplier);

}

void loop() {
    if (oscillating) {
        Serial.println("[MOTOR] Moving left for 10s...");
        moveForDuration(-1 * dirMultiplier, 10000);
        if (!oscillating) return;
        Serial.println("[MOTOR] Moving right for 10s...");
        moveForDuration(+1 * dirMultiplier, 10000);
    } else {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}