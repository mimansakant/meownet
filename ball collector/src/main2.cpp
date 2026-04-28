#include <Arduino.h>
#include <Stepper.h>

// ---------- stepper setup ----------
const int stepsPerRevolution = 2048;  // 28BYJ-48 in 4-wire mode

// Stepper A — upper-left arm, reacts to slot 2 overflow
#define A_IN1 13
#define A_IN2 25
#define A_IN3 26
#define A_IN4 17
Stepper armA(stepsPerRevolution, A_IN1, A_IN3, A_IN2, A_IN4);

// Stepper B — lower-right arm, reacts to slot 4 overflow
#define B_IN1 21
#define B_IN2 22
#define B_IN3 2
#define B_IN4 3   // NOTE: GPIO 3 is normally UART RX. Serial input disabled.
Stepper armB(stepsPerRevolution, B_IN1, B_IN3, B_IN2, B_IN4);

// ---------- slot sensors (capacitive touch) ----------
const int SLOT_PINS[5] = {32, 33, 27, 12, 15};   // slots 1..5
const int NUM_SLOTS = 5;
const int BAD_SLOTS[2] = {1, 3};                 // indices for slot 2 and slot 4

// touchRead returns LOWER values when touched. Tune this on your board.
const int TOUCH_THRESHOLD = 40;

// ---------- rolling window ----------
const int WINDOW_SIZE = 10;
int recentSlots[WINDOW_SIZE];   // ring buffer of slot indices (0..4)
int windowCount = 0;            // how many entries currently valid (<= WINDOW_SIZE)
int windowHead = 0;             // next write position

// ---------- arm position tracking ----------
const int MAX_DEFLECT_STEPS = stepsPerRevolution / 4;
int armAPosition = 0;
int armBPosition = 0;

// ---------- ball-edge detection ----------
bool slotActive[5] = {false, false, false, false, false};
unsigned long lastTriggerMs[5] = {0, 0, 0, 0, 0};
const unsigned long DEBOUNCE_MS = 150;

// ---------- forward declarations (required for .cpp) ----------
void recordBall(int slotIndex);
int countInWindow(int slotIndex);
void moveArmTo(Stepper &motor, int &currentPos, int targetPos);
int desiredDeflection(int slotIndex);

void setup() {
  Serial.begin(115200);
  // (no Serial.read — GPIO 3 reassigned to stepper)

  armA.setSpeed(10);
  armB.setSpeed(10);

  Serial.println("Pachinko controller ready.");
}

void recordBall(int slotIndex) {
  recentSlots[windowHead] = slotIndex;
  windowHead = (windowHead + 1) % WINDOW_SIZE;
  if (windowCount < WINDOW_SIZE) windowCount++;
}

int countInWindow(int slotIndex) {
  int c = 0;
  for (int i = 0; i < windowCount; i++) {
    if (recentSlots[i] == slotIndex) c++;
  }
  return c;
}

void moveArmTo(Stepper &motor, int &currentPos, int targetPos) {
  int delta = targetPos - currentPos;
  if (delta != 0) {
    motor.step(delta);
    currentPos = targetPos;
  }
}

int desiredDeflection(int slotIndex) {
  if (windowCount == 0) return 0;
  float share = (float)countInWindow(slotIndex) / (float)windowCount;
  float expected = 1.0f / NUM_SLOTS;       // 0.20
  float excess = share - expected;         // 0..0.80
  if (excess <= 0) return 0;
  float scale = excess / (1.0f - expected);
  if (scale > 1.0f) scale = 1.0f;
  return (int)(scale * MAX_DEFLECT_STEPS);
}

void loop() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_SLOTS; i++) {
    int reading = touchRead(SLOT_PINS[i]);
    bool touched = (reading < TOUCH_THRESHOLD);

    if (touched && !slotActive[i] && (now - lastTriggerMs[i] > DEBOUNCE_MS)) {
      slotActive[i] = true;
      lastTriggerMs[i] = now;
      recordBall(i);
      Serial.print("Ball -> slot ");
      Serial.print(i + 1);
      Serial.print("  | counts: ");
      for (int j = 0; j < NUM_SLOTS; j++) {
        Serial.print(countInWindow(j));
        Serial.print(j == NUM_SLOTS - 1 ? "" : ",");
      }
      Serial.print("  (window=");
      Serial.print(windowCount);
      Serial.println(")");
    } else if (!touched && slotActive[i]) {
      slotActive[i] = false;
    }
  }

  int targetA = desiredDeflection(BAD_SLOTS[0]);  // slot 2
  int targetB = desiredDeflection(BAD_SLOTS[1]);  // slot 4
  moveArmTo(armA, armAPosition, targetA);
  moveArmTo(armB, armBPosition, targetB);
}