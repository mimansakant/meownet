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

#define STEPS_PER_ADJUSTMENT 1024
#define STEP_DELAY_US 3000

int currentPosition = 0;
int maxPosition = 512;

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
    static int stepIndex = 0;

    for (int i = 0; i < absSteps; i++) {
        stepIndex = (stepIndex + direction + 4) % 4;
        setMotorPins(stepIndex);
        delayMicroseconds(STEP_DELAY_US);
    }
    powerOffMotor();
}

void adjustMotor(int8_t adjustment) {
    int targetSteps = adjustment * STEPS_PER_ADJUSTMENT;
    int newPosition = currentPosition + targetSteps;
   
    // If at limit, reset to center
    if (newPosition > maxPosition || newPosition < -maxPosition) {
        Serial.println("[MOTOR] At limit — resetting to center");
        moveSteps(-currentPosition);
        currentPosition = 0;
        return;
    }
   
    int actualSteps = newPosition - currentPosition;
    if (actualSteps != 0) {
        Serial.printf("[MOTOR] Moving %d steps\n", actualSteps);
        moveSteps(actualSteps);
        currentPosition = newPosition;
    }
}

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    int8_t adjustment = (int8_t)data[0];
    Serial.printf("[ESP-NOW] Received %d bytes, adjustment=%d\n", len, adjustment);
    adjustMotor(adjustment);
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
    Serial.printf("[INFO] This ESP32 MAC: %s\n", WiFi.macAddress().c_str());
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}