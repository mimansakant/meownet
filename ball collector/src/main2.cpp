#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <AccelStepper.h>

// ─── SLOT CONFIGURATION ──────────────────────────────────────────────────────
const int NUM_SLOTS             = 5;
const int SLOT_PINS[NUM_SLOTS]  = {15, 12, 13, 2, 33};

// "Every other slot is good." Index 0 = pin 15, 1 = pin 12, 2 = pin 13, 3 = pin 2, 4 = pin 33.
// PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
const bool GOOD_SLOT[NUM_SLOTS] = {true, false, true, false, true};

// ─── TOUCH / BALL DETECTION ───────────────────────────────────────────────────
// touchRead() returns LOWER values when the copper tape is touched by a ball.
// Set this just below your resting (no-ball) baseline reading.
// PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
const int TOUCH_THRESHOLD = 20;

// Minimum milliseconds between detections on the same slot (prevents one ball
// triggering multiple counts while it lingers on the tape).
// PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
const unsigned long DEBOUNCE_MS = 300;

// ─── STEPPER MOTOR PINS ───────────────────────────────────────────────────────
// Using STEP + DIR wiring (works with A4988, DRV8825, TMC2209, etc.).
// If you are using a ULN2003 with a 28BYJ-48, switch AccelStepper::DRIVER to
// AccelStepper::HALF4WIRE and replace these with your four IN pins.
// PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
#define MOTOR1_STEP_PIN  16
#define MOTOR1_DIR_PIN   17
#define MOTOR2_STEP_PIN  18
#define MOTOR2_DIR_PIN   19

// ─── STEPPER TUNING ───────────────────────────────────────────────────────────
// PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
const float MOTOR_MAX_SPEED    = 200.0;  // steps / second
const float MOTOR_ACCELERATION = 100.0;  // steps / second²

// How far each motor moves per corrective nudge (in steps).
// PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
const int NUDGE_STEPS = 50;

// How many extra bad-slot hits (vs good) before a nudge fires.
// PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
const int IMBALANCE_TRIGGER = 3;

// ─── WIFI / UDP ───────────────────────────────────────────────────────────────
const char* apSSID     = "DRUM_PAD_WIFI";
const char* apPassword = "drumpad123";
const char* laptopIP   = "192.168.4.2";
const int   laptopPort = 5005;
WiFiUDP udp;

// ─── MOTOR OBJECTS ────────────────────────────────────────────────────────────
// PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
// Change AccelStepper::DRIVER to AccelStepper::HALF4WIRE if using ULN2003.
AccelStepper motor1(AccelStepper::DRIVER, MOTOR1_STEP_PIN, MOTOR1_DIR_PIN);
AccelStepper motor2(AccelStepper::DRIVER, MOTOR2_STEP_PIN, MOTOR2_DIR_PIN);

// ─── STATE ────────────────────────────────────────────────────────────────────
int           ballCount[NUM_SLOTS]   = {0};
unsigned long lastHitTime[NUM_SLOTS] = {0};
bool          slotActive[NUM_SLOTS]  = {false};

int totalGoodHits = 0;
int totalBadHits  = 0;

// ─── HELPERS ──────────────────────────────────────────────────────────────────
void sendUDP(const char* msg) {
  if (udp.beginPacket(laptopIP, laptopPort)) {
    udp.print(msg);
    udp.endPacket();
  }
  Serial.println(msg);
}

// Called every time a new ball is detected.
// Compares cumulative good vs bad hits and nudges the motors if the board is
// sending too many balls into bad slots.
//
// Motor 1 handles left-side imbalance, Motor 2 handles right-side imbalance.
// The specific slots that count as "left" vs "right" are placeholders — map
// them to your physical board layout once you can test.
void adjustMotors() {
  // ── Left-side balance (motor 1) ──────────────────────────────────────────
  // PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
  // Slot index 1 (pin 12) is the left bad slot. Slot index 0 (pin 15) is the
  // left good slot. Adjust these indices to match your physical board.
  int leftImbalance = ballCount[1] - ballCount[0];  // positive = too many bad hits on left

  if (leftImbalance >= IMBALANCE_TRIGGER) {
    // Too many balls falling into the left bad slot — nudge motor 1
    // PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
    // Positive steps = which physical direction steers balls away from slot 1.
    motor1.move(NUDGE_STEPS);
    sendUDP("ADJUST: motor1 nudge + (left bad slot overflow)");
  } else if (leftImbalance <= -IMBALANCE_TRIGGER) {
    motor1.move(-NUDGE_STEPS);
    sendUDP("ADJUST: motor1 nudge - (left good slot starved)");
  }

  // ── Right-side balance (motor 2) ─────────────────────────────────────────
  // PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
  // Slot index 3 (pin 2) is the right bad slot. Slot index 4 (pin 33) is the
  // right good slot. Adjust these indices to match your physical board.
  int rightImbalance = ballCount[3] - ballCount[4];  // positive = too many bad hits on right

  if (rightImbalance >= IMBALANCE_TRIGGER) {
    // PLACEHOLDER VALUE HERE, UPDATE WHEN TESTING ON BOARD
    motor2.move(NUDGE_STEPS);
    sendUDP("ADJUST: motor2 nudge + (right bad slot overflow)");
  } else if (rightImbalance <= -IMBALANCE_TRIGGER) {
    motor2.move(-NUDGE_STEPS);
    sendUDP("ADJUST: motor2 nudge - (right good slot starved)");
  }
}

// ─── SETUP ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  delay(1000);
  Serial.println("Meownet Ball Detector — starting");

  motor1.setMaxSpeed(MOTOR_MAX_SPEED);
  motor1.setAcceleration(MOTOR_ACCELERATION);
  motor2.setMaxSpeed(MOTOR_MAX_SPEED);
  motor2.setAcceleration(MOTOR_ACCELERATION);
}

// ─── LOOP ─────────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── Ball detection ──────────────────────────────────────────────────────
  for (int i = 0; i < NUM_SLOTS; i++) {
    int  raw     = touchRead(SLOT_PINS[i]);
    bool touched = (raw < TOUCH_THRESHOLD);

    if (touched && !slotActive[i] && (now - lastHitTime[i] > DEBOUNCE_MS)) {
      ballCount[i]++;
      lastHitTime[i] = now;
      slotActive[i]  = true;

      if (GOOD_SLOT[i]) totalGoodHits++;
      else              totalBadHits++;

      char msg[96];
      snprintf(msg, sizeof(msg),
               "BALL slot=%d pin=%d count=%d %s | good=%d bad=%d",
               i, SLOT_PINS[i], ballCount[i],
               GOOD_SLOT[i] ? "GOOD" : "BAD",
               totalGoodHits, totalBadHits);
      sendUDP(msg);

      adjustMotors();
    }

    if (!touched) slotActive[i] = false;
  }

  // ── Keep motors stepping toward their targets ───────────────────────────
  motor1.run();
  motor2.run();

  // ── Periodic status broadcast (every 5 s) ──────────────────────────────
  static unsigned long lastStatus = 0;
  if (now - lastStatus >= 5000) {
    lastStatus = now;
    char status[160];
    snprintf(status, sizeof(status),
             "STATUS s0:%d s1:%d s2:%d s3:%d s4:%d | good:%d bad=%d | m1:%ld m2:%ld",
             ballCount[0], ballCount[1], ballCount[2], ballCount[3], ballCount[4],
             totalGoodHits, totalBadHits,
             motor1.currentPosition(), motor2.currentPosition());
    sendUDP(status);
  }
}
