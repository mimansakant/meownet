#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi/UDP config
const char* apSSID = "BALL_TRACKER_WIFI";
const char* apPassword = "balltrack123";
const char* laptopIP = "192.168.4.2";
const int laptopPort = 5005;
WiFiUDP udp;

// Channel pin definitions
const int CHANNEL_PINS[] = {15, 12, 32, 33, 27};
const int NUM_CHANNELS = 5;
int ballCount[NUM_CHANNELS] = {0};
bool lastState[NUM_CHANNELS] = {false};

void sendUDP(const char* msg) {
  if (udp.beginPacket(laptopIP, laptopPort)) {
    udp.print(msg);
    udp.endPacket();
    Serial.print(" >>> "); Serial.println(msg);
  }
}

void setup() {
  Serial.begin(115200);

  // Setup pins
  for (int i = 0; i < NUM_CHANNELS; i++) {
    pinMode(CHANNEL_PINS[i], INPUT_PULLDOWN);
  }

  // Setup WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  delay(1000);
  Serial.println("Ball Tracker Debug Mode Started");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    bool currentState = digitalRead(CHANNEL_PINS[i]) == HIGH;

    // Only trigger on rising edge (ball just arrived)
    if (currentState && !lastState[i]) {
      ballCount[i]++;

      // Build and send a message for this specific event
      char eventMsg[80];
      snprintf(eventMsg, sizeof(eventMsg), "BALL channel:%d count:%d", i, ballCount[i]);
      sendUDP(eventMsg);
    }

    lastState[i] = currentState;
  }

  // Also send a full status snapshot every 2 seconds
  static unsigned long lastSnapshot = 0;
  if (millis() - lastSnapshot > 2000) {
    char snapshot[160];
    snprintf(snapshot, sizeof(snapshot),
      "COUNTS ch0:%d ch1:%d ch2:%d ch3:%d ch4:%d",
      ballCount[0], ballCount[1], ballCount[2], ballCount[3], ballCount[4]);
    sendUDP(snapshot);
    lastSnapshot = millis();
  }

  delay(10); // debounce
}