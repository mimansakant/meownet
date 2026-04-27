#include <Arduino.h>
#include <TFT_eSPI.h>

// TTGO T-Display: 135 x 240 ST7789. We use it in landscape (240 x 135).
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite eye = TFT_eSprite(&tft);   // off-screen buffer = no flicker

// ----- screen / eye geometry -----
const int SCR_W = 240;
const int SCR_H = 135;

const int EYE_CX = SCR_W / 2;     // eye center
const int EYE_CY = SCR_H / 2;
const int EYE_RX = 90;            // iris radius (horizontal)
const int EYE_RY = 55;            // iris radius (vertical)

const int PUPIL_W = 14;           // slit pupil width
const int PUPIL_H = 90;           // slit pupil height (tall = cat-like)

// ----- colors -----
const uint16_t COL_BG     = TFT_BLACK;
const uint16_t COL_IRIS   = tft.color565(120, 220, 80);   // green
const uint16_t COL_IRIS_D = tft.color565(60, 140, 40);    // darker ring
const uint16_t COL_PUPIL  = TFT_BLACK;
const uint16_t COL_SHINE  = TFT_WHITE;
const uint16_t COL_LID    = tft.color565(20, 15, 25);     // near-black eyelid

// ----- animation state -----
unsigned long lastBlinkMs = 0;
unsigned long nextBlinkDelay = 3000;
bool blinking = false;
unsigned long blinkStartMs = 0;
const int BLINK_DURATION_MS = 220;   // full close+open

unsigned long lastLookMs = 0;
unsigned long nextLookDelay = 2500;
int lookTargetX = 0, lookTargetY = 0;
int lookX = 0, lookY = 0;            // current pupil offset
const int LOOK_RANGE_X = 25;
const int LOOK_RANGE_Y = 12;

// draw one frame into the sprite, then push to screen
void drawEye(float lidProgress) {
  // lidProgress: 0.0 = fully open, 1.0 = fully closed
  eye.fillSprite(COL_BG);

  // iris (filled ellipse)
  eye.fillSmoothRoundRect(0, 0, 0, 0, 0, COL_BG);  // no-op, just to silence unused
  // TFT_eSprite has fillEllipse
  eye.fillEllipse(EYE_CX, EYE_CY, EYE_RX, EYE_RY, COL_IRIS);
  eye.drawEllipse(EYE_CX, EYE_CY, EYE_RX, EYE_RY, COL_IRIS_D);
  eye.drawEllipse(EYE_CX, EYE_CY, EYE_RX - 1, EYE_RY - 1, COL_IRIS_D);

  // vertical slit pupil, offset by current look direction
  int px = EYE_CX + lookX;
  int py = EYE_CY + lookY;
  eye.fillRoundRect(px - PUPIL_W / 2, py - PUPIL_H / 2,
                    PUPIL_W, PUPIL_H, PUPIL_W / 2, COL_PUPIL);

  // little white shine for cuteness
  eye.fillCircle(px - 8, py - 25, 5, COL_SHINE);
  eye.fillCircle(px - 4, py - 18, 2, COL_SHINE);

  // eyelids closing from top and bottom
  if (lidProgress > 0.0f) {
    int lidH = (int)(SCR_H * 0.5f * lidProgress);
    eye.fillRect(0, 0, SCR_W, lidH, COL_LID);
    eye.fillRect(0, SCR_H - lidH, SCR_W, lidH, COL_LID);
  }

  eye.pushSprite(0, 0);
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);            // landscape
  tft.fillScreen(COL_BG);

  eye.setColorDepth(16);
  eye.createSprite(SCR_W, SCR_H);

  randomSeed(analogRead(34));    // any unused ADC pin
  drawEye(0.0f);
}

void loop() {
  unsigned long now = millis();

  // schedule next blink
  if (!blinking && now - lastBlinkMs > nextBlinkDelay) {
    blinking = true;
    blinkStartMs = now;
  }

  // schedule a new look target every couple seconds
  if (now - lastLookMs > nextLookDelay) {
    lookTargetX = random(-LOOK_RANGE_X, LOOK_RANGE_X + 1);
    lookTargetY = random(-LOOK_RANGE_Y, LOOK_RANGE_Y + 1);
    lastLookMs = now;
    nextLookDelay = random(1500, 4000);
  }

  // ease pupil toward target (slow drift)
  if (lookX < lookTargetX) lookX++;
  else if (lookX > lookTargetX) lookX--;
  if (lookY < lookTargetY) lookY++;
  else if (lookY > lookTargetY) lookY--;

  // figure out lid position
  float lid = 0.0f;
  if (blinking) {
    float t = (float)(now - blinkStartMs) / BLINK_DURATION_MS;
    if (t >= 1.0f) {
      blinking = false;
      lastBlinkMs = now;
      // sometimes a quick double-blink, sometimes a long pause
      nextBlinkDelay = random(2500, 6000);
      lid = 0.0f;
    } else if (t < 0.5f) {
      lid = t * 2.0f;             // closing
    } else {
      lid = (1.0f - t) * 2.0f;    // opening
    }
  }

  drawEye(lid);
  delay(20);   // ~50 fps cap
}
