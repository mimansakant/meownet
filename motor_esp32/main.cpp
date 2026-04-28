/**
 * Motor ESP32 - Cat Plinko Ball Deflector
 * 
 * OVERVIEW:
 * =========
 * Receives ESP-NOW messages from the sensor ESP32 and adjusts
 * a stepper motor arm to change ball distribution.
 * 
 * HARDWARE:
 * =========
 * - ESP32
 * - Stepper motor + ULN2003 driver board (or A4988)
 * - 3D printed arm attached to shaft coupler


#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define MOTOR_IN1 13
#define MOTOR_IN2 26
#define MOTOR_IN3 27
#define MOTOR_IN4 5

#define STEPS_PER_REV 2048
#define STEPS_PER_ADJUSTMENT 256
#define STEP_DELAY_US 1200

int currentPosition = 0;
int maxPosition = 512;

typedef struct {
    uint8_t  motor_id;
    int8_t   adjustment;
    uint32_t ball_count;
    char     message[64];
} MonitorFlag;

const int stepSequence[4][4] = {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1}
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
    newPosition = max(-maxPosition, min(maxPosition, newPosition));
    int actualSteps = newPosition - currentPosition;

    if (actualSteps != 0) {
        Serial.printf("[MOTOR] Moving %d steps (position: %d → %d)\n",
                      actualSteps, currentPosition, newPosition);
        moveSteps(actualSteps);
        currentPosition = newPosition;
    } else {
        Serial.println("[MOTOR] Already at limit, not moving");
    }
}

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(MonitorFlag)) {
        Serial.println("[ESP-NOW] Received unexpected data size");
        return;
    }

    MonitorFlag flag;
    memcpy(&flag, data, sizeof(flag));

    Serial.printf("[ESP-NOW] motor_id=%d adjustment=%+d count=%lu msg=%s\n",
                  flag.motor_id, flag.adjustment, flag.ball_count, flag.message);

    adjustMotor(flag.adjustment);
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
