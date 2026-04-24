#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// TFT_eSPI tft = TFT_eSPI(); 

// pin definitions 
bool pin15Touched = false;
bool pin12Touched = false;
bool pin13Touched = false;
bool pin2Touched = false;
bool pin33Touched = false;

const char* apSSID = "DRUM_PAD_WIFI";
const char* apPassword = "drumpad123";
const char* laptopIP = "192.168.4.2";
const int laptopPort = 5005;
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  delay(1000); // give me time to bring up serial monitor
  Serial.println("ESP32 Touch Test");
}

void loop() {
  char buffer[160];
  snprintf(buffer, sizeof(buffer), 
             "15:%d | 12:%d | 13:%d | 2:%d | 33:%d", 
             touchRead(15), touchRead(12), touchRead(13), touchRead(2), touchRead(33));

    Serial.println(buffer);
  delay(1000);
  if (udp.beginPacket(laptopIP, laptopPort)) {
      udp.print(buffer);
      udp.endPacket();
    }
}

void sendMessage(const char* msg) {
  if (udp.beginPacket(laptopIP, laptopPort)) {
    udp.print(msg);
    udp.endPacket();
    Serial.print(" >>> "); Serial.println(msg);
  }
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}