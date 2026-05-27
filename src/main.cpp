#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>

#include <hw_config.h>

namespace {

LiquidCrystal_I2C gLcd(kLcdI2cAddr, 16, 2);
AccelStepper gStepper(AccelStepper::DRIVER, kPinMotorStep, kPinMotorDir);

enum class AppState { SelectingRpm, Running };

AppState gState = AppState::SelectingRpm;
int32_t gSelectedRpm = 100;
int32_t gLastDrawnRpm = -1;
uint32_t gLastLcdUpdateMs = 0;

int gLastEncClk = HIGH;
int gSwLastRaw = HIGH;
int gSwStable = HIGH;
uint32_t gSwLastTransitionMs = 0;

float motorStepsPerRev() {
  return kFullStepsPerRevMotor * static_cast<float>(kMicrostepping);
}

float rpmToStepperMaxSpeed(float rpmSpindle) {
  const float motorRpm = rpmSpindle * kGearRatioMotorToSpindle;
  const float stepsPerSec = (motorRpm / 60.0f) * motorStepsPerRev();
  if (stepsPerSec < 1.0f) {
    return 1.0f;
  }
  return stepsPerSec;
}

void motorDriverEnable(bool on) {
  if (kPinMotorEnable < 0) {
    return;
  }
  // A4988/DRV8825: pin ENABLE często aktywny w niskim stanie.
  digitalWrite(kPinMotorEnable, on ? LOW : HIGH);
}

void lcdInit() {
  Wire.begin(kPinLcdSda, kPinLcdScl);
  gLcd.init();
  gLcd.backlight();
  gLcd.clear();
}

void lcdDrawSelecting(bool force) {
  if (!force && gSelectedRpm == gLastDrawnRpm) {
    return;
  }
  gLastDrawnRpm = gSelectedRpm;
  gLastLcdUpdateMs = millis();

  gLcd.clear();
  gLcd.setCursor(0, 0);
  gLcd.print("Ustaw RPM");
  gLcd.setCursor(0, 1);
  char line[17];
  snprintf(line, sizeof(line), "%4d ENC  SW=start", static_cast<int>(gSelectedRpm));
  gLcd.print(line);
}

void lcdDrawRunning(bool force) {
  const uint32_t now = millis();
  if (!force && (now - gLastLcdUpdateMs) < kLcdRefreshWhileRunningMs) {
    return;
  }
  gLastLcdUpdateMs = now;

  gLcd.clear();
  gLcd.setCursor(0, 0);
  gLcd.print("Praca");
  gLcd.setCursor(0, 1);
  char line[17];
  snprintf(line, sizeof(line), "RPM %4d", static_cast<int>(gSelectedRpm));
  gLcd.print(line);
}

void startMotorAtSelectedRpm() {
  motorDriverEnable(true);
  const float maxSpeed = rpmToStepperMaxSpeed(static_cast<float>(gSelectedRpm));
  gStepper.setMaxSpeed(maxSpeed);
  // Przyspieszenie skalowane do prędkości: przy niskich RPM unikasz zbyt ostrego szarpnięcia.
  float accel = maxSpeed * 1.2f;
  const float kAccelMin = 80.0f;
  if (accel < kAccelMin) {
    accel = kAccelMin;
  }
  if (accel > kMotorAccelerationStepsPerSec2) {
    accel = kMotorAccelerationStepsPerSec2;
  }
  gStepper.setAcceleration(accel);
  // Długi ruch: AccelStepper sam płynnie rozpędzi silnik do setMaxSpeed i utrzyma stałe obroty.
  constexpr long kLongRunSteps = 200000000L;
  gStepper.moveTo(gStepper.currentPosition() + kLongRunSteps);
}

void pollEncoderRotation() {
  const int clk = digitalRead(kPinEncClk);
  if (clk == gLastEncClk) {
    return;
  }
  if (digitalRead(kPinEncDt) != clk) {
    gSelectedRpm++;
  } else {
    gSelectedRpm--;
  }
  if (gSelectedRpm < kRpmMin) {
    gSelectedRpm = kRpmMin;
  }
  if (gSelectedRpm > kRpmMax) {
    gSelectedRpm = kRpmMax;
  }
  gLastEncClk = clk;

  if (gState == AppState::SelectingRpm) {
    lcdDrawSelecting(false);
  }
}

bool pollEncoderButtonPressed() {
  const int raw = digitalRead(kPinEncSw);
  const uint32_t now = millis();
  if (raw != gSwLastRaw) {
    gSwLastTransitionMs = now;
    gSwLastRaw = raw;
  }
  if ((now - gSwLastTransitionMs) < kEncoderButtonDebounceMs) {
    return false;
  }
  if (raw == gSwStable) {
    return false;
  }
  const int previous = gSwStable;
  gSwStable = raw;
  return previous == HIGH && gSwStable == LOW;
}

}  // namespace

void setup() {
  pinMode(kPinEncClk, INPUT_PULLUP);
  pinMode(kPinEncDt, INPUT_PULLUP);
  pinMode(kPinEncSw, INPUT_PULLUP);

  if (kPinMotorEnable >= 0) {
    pinMode(kPinMotorEnable, OUTPUT);
    motorDriverEnable(false);
  }

  lcdInit();
  lcdDrawSelecting(true);

  gLastEncClk = digitalRead(kPinEncClk);
  gSwLastRaw = digitalRead(kPinEncSw);
  gSwStable = gSwLastRaw;
}

void loop() {
  pollEncoderRotation();

  if (gState == AppState::SelectingRpm) {
    if (pollEncoderButtonPressed()) {
      gState = AppState::Running;
      startMotorAtSelectedRpm();
      lcdDrawRunning(true);
    }
    return;
  }

  // Running: jak najwięcej czasu na stepper.run(), LCD tylko rzadko.
  if (!gStepper.run()) {
    // Teoretyczny koniec ruchu (przy bardzo długim moveTo praktycznie nieosiągalny).
    motorDriverEnable(false);
    gState = AppState::SelectingRpm;
    lcdDrawSelecting(true);
    return;
  }
  lcdDrawRunning(false);
}
