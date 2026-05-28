#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <hardware/pwm.h>

#include <hw_config.h>

namespace {

LiquidCrystal_I2C gLcd(kLcdI2cAddr, kLcdCols, kLcdRows);

enum class Phase { Selecting, Running };

Phase gPhase = Phase::Selecting;
int32_t gRpm = 100;

uint32_t gLastLcdMs = 0;
int32_t gLastLcdRpm = -1;

uint8_t gEncPrevAb = 0;
volatile int32_t gEncPending = 0;
volatile uint32_t gEncLastStepUs = 0;

int gSwRaw = HIGH;
int gSwStable = HIGH;
uint32_t gSwTransitionMs = 0;

// PWM generator STEP
uint gPwmSlice = 0;
bool gPwmInitialized = false;
float gStepHzCurrent = 0.0f;
float gStepHzTarget = 0.0f;
uint32_t gLastRampUs = 0;

float stepsPerSecondFromRpm(float rpmSpindle) {
  const float motorRpm = rpmSpindle * kGearRatioMotorToSpindle;
  const float stepsPerRev = kFullStepsPerRevMotor * static_cast<float>(kDriverMicrosteps);
  float sps = (motorRpm / 60.0f) * stepsPerRev;
  if (sps < 0.0f) {
    sps = 0.0f;
  }
  return sps;
}

void motorEnable(bool on) {
  // LOW = enabled
  digitalWrite(kPinMotorEnable, on ? LOW : HIGH);
}

void pwmStepInit() {
  gpio_set_function(kPinMotorStep, GPIO_FUNC_PWM);
  gPwmSlice = pwm_gpio_to_slice_num(kPinMotorStep);
  pwm_set_enabled(gPwmSlice, false);
  gPwmInitialized = true;
}

void pwmStepSetFrequency(float hz) {
  if (!gPwmInitialized) {
    pwmStepInit();
  }

  if (hz <= 0.5f) {
    pwm_set_enabled(gPwmSlice, false);
    gpio_set_function(kPinMotorStep, GPIO_FUNC_SIO);
    digitalWrite(kPinMotorStep, LOW);
    gpio_set_function(kPinMotorStep, GPIO_FUNC_PWM);
    return;
  }

  const float pwmClk = 125000000.0f;
  float target = pwmClk / hz;  // = div * (top+1)

  float div = target / 65535.0f;
  if (div < 1.0f) div = 1.0f;
  if (div > 255.0f) div = 255.0f;

  uint32_t top = static_cast<uint32_t>(target / div);
  if (top < 2) top = 2;
  if (top > 65535) top = 65535;

  pwm_set_clkdiv(gPwmSlice, div);
  pwm_set_wrap(gPwmSlice, top - 1);
  pwm_set_gpio_level(kPinMotorStep, (top - 1) / 2);
  pwm_set_enabled(gPwmSlice, true);
}

void lcdHardwareInit() {
  Wire.setSDA(kPinLcdSda);
  Wire.setSCL(kPinLcdScl);
  Wire.begin();
  gLcd.begin(kLcdCols, kLcdRows);
  gLcd.backlight();
  gLcd.clear();
}

void lcdPrintPadded(uint8_t col, uint8_t row, const char* s) {
  gLcd.setCursor(col, row);
  uint8_t i = 0;
  for (; i < kLcdCols && s[i] != "\0"[0]; i++) {
    gLcd.write(static_cast<uint8_t>(s[i]));
  }
  for (; i < kLcdCols; i++) {
    gLcd.write(" "[0]);
  }
}

void lcdRenderSelecting(bool forceFullRedraw) {
  if (!forceFullRedraw && gRpm == gLastLcdRpm) {
    return;
  }
  gLastLcdRpm = gRpm;
  gLastLcdMs = millis();

  char l0[21];
  char l1[21];
  char l2[21];
  char l3[21];
  snprintf(l0, sizeof(l0), "RPM SELECT");
  snprintf(l1, sizeof(l1), "Range 1..1500");
  snprintf(l2, sizeof(l2), "Set: %4ld RPM", static_cast<long>(gRpm));
  snprintf(l3, sizeof(l3), "ENC=chg SW=start");

  lcdPrintPadded(0, 0, l0);
  lcdPrintPadded(0, 1, l1);
  lcdPrintPadded(0, 2, l2);
  lcdPrintPadded(0, 3, l3);
}

void lcdRenderRunning(bool forceFullRedraw) {
  const uint32_t now = millis();
  if (!forceFullRedraw && (now - gLastLcdMs) < kLcdRefreshWhileRunningMs) {
    return;
  }
  gLastLcdMs = now;

  char l0[21];
  char l1[21];
  char l2[21];
  char l3[21];
  snprintf(l0, sizeof(l0), "RUN");
  snprintf(l1, sizeof(l1), "RPM: %4ld", static_cast<long>(gRpm));
  snprintf(l2, sizeof(l2), "STEP: %6.0f Hz", static_cast<double>(gStepHzCurrent));
  snprintf(l3, sizeof(l3), "TMC STEP/DIR");

  lcdPrintPadded(0, 0, l0);
  lcdPrintPadded(0, 1, l1);
  lcdPrintPadded(0, 2, l2);
  lcdPrintPadded(0, 3, l3);
}

void encoderPollAndApply() {
  int32_t ticks = 0;
  uint32_t lastUs = 0;
  noInterrupts();
  ticks = gEncPending;
  gEncPending = 0;
  lastUs = gEncLastStepUs;
  interrupts();
  if (ticks == 0) {
    return;
  }

  const uint32_t nowUs = micros();
  const uint32_t dtUs = (lastUs == 0) ? 1000000u : (nowUs - lastUs);

  int32_t step = 1;
  if (dtUs < 5000u) {
    step = 25;
  } else if (dtUs < 12000u) {
    step = 10;
  } else if (dtUs < 25000u) {
    step = 5;
  } else if (dtUs < 60000u) {
    step = 2;
  }

  gRpm += ticks * step;
  if (gRpm < kRpmMin) gRpm = kRpmMin;
  if (gRpm > kRpmMax) gRpm = kRpmMax;

  if (gPhase == Phase::Selecting) {
    lcdRenderSelecting(false);
  }
}

void encoderIsr() {
  const uint8_t a = static_cast<uint8_t>(digitalRead(kPinEncClk) == HIGH);
  const uint8_t b = static_cast<uint8_t>(digitalRead(kPinEncDt) == HIGH);
  const uint8_t ab = static_cast<uint8_t>((a << 1) | b);

  static const int8_t kQuadDelta[16] = {
      0, +1, -1, 0, -1, 0, 0, +1, +1, 0, 0, -1, 0, -1, +1, 0};

  const int8_t delta = kQuadDelta[(static_cast<uint8_t>(gEncPrevAb << 2)) | ab];
  gEncPrevAb = ab;
  if (delta == 0) {
    return;
  }
  gEncPending += delta;
  gEncLastStepUs = micros();
}

bool encoderSwitchPressedEdge() {
  const int raw = digitalRead(kPinEncSw);
  const uint32_t now = millis();
  if (raw != gSwRaw) {
    gSwTransitionMs = now;
    gSwRaw = raw;
  }
  if ((now - gSwTransitionMs) < kEncoderButtonDebounceMs) {
    return false;
  }
  if (raw == gSwStable) {
    return false;
  }
  const int previous = gSwStable;
  gSwStable = raw;
  return previous == HIGH && gSwStable == LOW;
}

void motorStart() {
  digitalWrite(kPinMotorDir, kMotorDirCwLevel);
  motorEnable(true);

  gStepHzTarget = stepsPerSecondFromRpm(static_cast<float>(gRpm));
  if (gStepHzCurrent < 1.0f) {
    gStepHzCurrent = 1.0f;
  }
  gLastRampUs = micros();
  pwmStepSetFrequency(gStepHzCurrent);
}

void motorRampUpdate() {
  const uint32_t nowUs = micros();
  const float dt = (nowUs - gLastRampUs) / 1000000.0f;
  if (dt <= 0.0f) {
    return;
  }
  gLastRampUs = nowUs;

  gStepHzTarget = stepsPerSecondFromRpm(static_cast<float>(gRpm));

  float maxDelta = kMotorAccelerationStepsPerSec2 * dt;
  if (maxDelta < 1.0f) {
    maxDelta = 1.0f;
  }

  float diff = gStepHzTarget - gStepHzCurrent;
  if (diff > maxDelta) diff = maxDelta;
  if (diff < -maxDelta) diff = -maxDelta;

  const float next = gStepHzCurrent + diff;
  if (fabsf(next - gStepHzCurrent) >= 0.5f) {
    gStepHzCurrent = next;
    pwmStepSetFrequency(gStepHzCurrent);
  }
}

}  // namespace

void setup() {
  pinMode(kPinEncClk, INPUT_PULLUP);
  pinMode(kPinEncDt, INPUT_PULLUP);
  pinMode(kPinEncSw, INPUT_PULLUP);
  pinMode(kPinHall3144, INPUT_PULLUP);

  pinMode(kPinMotorDir, OUTPUT);
  pinMode(kPinMotorEnable, OUTPUT);
  motorEnable(false);

  lcdHardwareInit();
  lcdRenderSelecting(true);

  {
    const uint8_t a = static_cast<uint8_t>(digitalRead(kPinEncClk) == HIGH);
    const uint8_t b = static_cast<uint8_t>(digitalRead(kPinEncDt) == HIGH);
    gEncPrevAb = static_cast<uint8_t>((a << 1) | b);
  }
  attachInterrupt(digitalPinToInterrupt(kPinEncClk), encoderIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(kPinEncDt), encoderIsr, CHANGE);
  gSwRaw = digitalRead(kPinEncSw);
  gSwStable = gSwRaw;

  pwmStepInit();
}

void loop() {
  if (gPhase == Phase::Selecting) {
    encoderPollAndApply();
    if (encoderSwitchPressedEdge()) {
      gPhase = Phase::Running;
      motorStart();
      lcdRenderRunning(true);
    }
    return;
  }

  motorRampUpdate();
  lcdRenderRunning(false);
}
