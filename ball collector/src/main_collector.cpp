#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// ---- MAC addresses of the two motor ESP32s ----
// Replace these after running the MAC finder sketch on each motor ESP32
uint8_t motorA_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
uint8_t motorB_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02};

// ---- Pin definitions ----
const int CHANNEL_PINS[] = {15, 12, 32, 33, 27};
const int NUM_CHANNELS = 5;

// ---- Ball tracking ----
int ballCount[NUM_CHANNELS] = {0};
int totalBalls = 0;
bool lastState[NUM_CHANNELS] = {false};

// ---- ESP-NOW message structure ----
typedef struct {
  int counts[5];
  int total;
  float distribution[5]; // percentage per channel
} BallData;

BallData ballData;

// ---- Only broadcast when distribution shifts meaningfully ----
float lastSentDistribution[NUM_CHANNELS] = {0};
const float SEND_THRESHOLD = 0.05; // 5% shift triggers a send

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("ESP-NOW send: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void addPeer(uint8_t* mac) {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

bool distributionShiftedEnough() {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (abs(ballData.distribution[i] - lastSentDistribution[i]) > SEND_THRESHOLD) {
      return true;
    }
  }
  return false;
}

void broadcastData() {
  esp_now_send(motorA_MAC, (uint8_t*)&ballData, sizeof(ballData));
  esp_now_send(motorB_MAC, (uint8_t*)&ballData, sizeof(ballData));
  memcpy(lastSentDistribution, ballData.distribution, sizeof(lastSentDistribution));
  Serial.println("Broadcast sent");
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < NUM_CHANNELS; i++) {
    pinMode(CHANNEL_PINS[i], INPUT_PULLDOWN);
  }

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_send_cb(onSent);
  addPeer(motorA_MAC);
  addPeer(motorB_MAC);

  Serial.println("Ball collector ready");
}

void loop() {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    bool currentState = digitalRead(CHANNEL_PINS[i]) == HIGH;

    // rising edge = ball detected
    if (currentState && !lastState[i]) {
      ballCount[i]++;
      totalBalls++;

      // update distribution
      for (int j = 0; j < NUM_CHANNELS; j++) {
        ballData.counts[j] = ballCount[j];
        ballData.distribution[j] = (float)ballCount[j] / (float)totalBalls;
      }
      ballData.total = totalBalls;

      Serial.printf("Ball in ch%d | counts: %d %d %d %d %d | total: %d\n",
        i, ballCount[0], ballCount[1], ballCount[2], ballCount[3], ballCount[4], totalBalls);

      // only send if distribution has shifted enough
      if (distributionShiftedEnough()) {
        broadcastData();
      }
    }

    lastState[i] = currentState;
  }

  delay(10); // debounce
}