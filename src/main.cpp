#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>

#include <hw_config.h>

namespace {

LiquidCrystal_I2C gLcd(kLcdI2cAddr, kLcdCols, kLcdRows);
AccelStepper gStepper(AccelStepper::DRIVER, kPinMotorStep, kPinMotorDir);

enum class Phase { Selecting, Running };

Phase gPhase = Phase::Selecting;
int32_t gRpm = 100;

uint32_t gLastLcdMs = 0;
int32_t gLastLcdRpm = -1;

uint8_t gEncPrevAb = 0;

int gSwRaw = HIGH;
int gSwStable = HIGH;
uint32_t gSwTransitionMs = 0;

float motorStepsPerSecondAtSpindleRpm(float rpmSpindle) {
  const float motorRpm = rpmSpindle * kGearRatioMotorToSpindle;
  const float stepsPerRev =
      kFullStepsPerRevMotor * static_cast<float>(kMicrostepping);
  float sps = (motorRpm / 60.0f) * stepsPerRev;
  if (sps < 1.0f) {
    sps = 1.0f;
  }
  return sps;
}

void motorBridgeEnable(bool on) {
  if (kPinMotorEnable < 0) {
    return;
  }
  // Zgodnie z KONFIG_SPRZETOWY.txt: LOW = sterownik włączony.
  digitalWrite(kPinMotorEnable, on ? LOW : HIGH);
}

void lcdHardwareInit() {
  // Pico (earlephilhower): I2C na GP0 / GP1 zgodnie z KONFIG_SPRZETOWY.txt.
  Wire.setSDA(kPinLcdSda);
  Wire.setSCL(kPinLcdScl);
  Wire.begin();
  gLcd.init();
  gLcd.backlight();
  gLcd.clear();
}

void lcdRenderSelecting(bool forceFullRedraw) {
  if (!forceFullRedraw && gRpm == gLastLcdRpm) {
    return;
  }
  gLastLcdRpm = gRpm;
  gLastLcdMs = millis();

  gLcd.clear();
  gLcd.setCursor(0, 0);
  gLcd.print(F("PICKUP WINDER"));
  gLcd.setCursor(0, 1);
  gLcd.print(F("RPM (1-1500):"));
  gLcd.setCursor(0, 2);
  char line[21];
  snprintf(line, sizeof(line), "   %4ld RPM", static_cast<long>(gRpm));
  gLcd.print(line);
  gLcd.setCursor(0, 3);
  gLcd.print(F("ENC=zmiana SW=start"));
}

void lcdRenderRunning(bool forceFullRedraw) {
  const uint32_t now = millis();
  if (!forceFullRedraw && (now - gLastLcdMs) < kLcdRefreshWhileRunningMs) {
    return;
  }
  gLastLcdMs = now;

  gLcd.clear();
  gLcd.setCursor(0, 0);
  gLcd.print(F("Praca silnika"));
  gLcd.setCursor(0, 1);
  char line[21];
  snprintf(line, sizeof(line), "RPM zadane: %4ld", static_cast<long>(gRpm));
  gLcd.print(line);
  gLcd.setCursor(0, 2);
  gLcd.print(F("TMC2209 STEP/DIR"));
  gLcd.setCursor(0, 3);
  gLcd.print(F("Hall A3144: GP9"));
}

void motorBeginConstantSpindleRpm() {
  motorBridgeEnable(true);

  const float maxStepsPerSec = motorStepsPerSecondAtSpindleRpm(static_cast<float>(gRpm));
  gStepper.setMaxSpeed(maxStepsPerSec);

  float accel = maxStepsPerSec * 1.15f;
  constexpr float kAccelFloor = 60.0f;
  if (accel < kAccelFloor) {
    accel = kAccelFloor;
  }
  if (accel > kMotorAccelerationStepsPerSec2) {
    accel = kMotorAccelerationStepsPerSec2;
  }
  gStepper.setAcceleration(accel);

  constexpr long kStepsVirtuallyInfinite = 300000000L;
  gStepper.moveTo(gStepper.currentPosition() + kStepsVirtuallyInfinite);
}

void encoderPollAndApply() {
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

  gRpm += static_cast<int32_t>(delta);
  if (gRpm < kRpmMin) {
    gRpm = kRpmMin;
  }
  if (gRpm > kRpmMax) {
    gRpm = kRpmMax;
  }

  if (gPhase == Phase::Selecting) {
    lcdRenderSelecting(false);
  }
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

}  // namespace

void setup() {
  pinMode(kPinEncClk, INPUT_PULLUP);
  pinMode(kPinEncDt, INPUT_PULLUP);
  pinMode(kPinEncSw, INPUT_PULLUP);
  pinMode(kPinHall3144, INPUT_PULLUP);

  pinMode(kPinMotorStep, OUTPUT);
  pinMode(kPinMotorDir, OUTPUT);
  if (kPinMotorEnable >= 0) {
    pinMode(kPinMotorEnable, OUTPUT);
    motorBridgeEnable(false);
  }

  lcdHardwareInit();
  lcdRenderSelecting(true);

  {
    const uint8_t a = static_cast<uint8_t>(digitalRead(kPinEncClk) == HIGH);
    const uint8_t b = static_cast<uint8_t>(digitalRead(kPinEncDt) == HIGH);
    gEncPrevAb = static_cast<uint8_t>((a << 1) | b);
  }
  gSwRaw = digitalRead(kPinEncSw);
  gSwStable = gSwRaw;
}

void loop() {
  if (gPhase == Phase::Selecting) {
    encoderPollAndApply();
    if (encoderSwitchPressedEdge()) {
      gPhase = Phase::Running;
      motorBeginConstantSpindleRpm();
      lcdRenderRunning(true);
    }
    return;
  }

  const bool more = gStepper.run();
  if (!more) {
    motorBridgeEnable(false);
    gPhase = Phase::Selecting;
    gLastLcdRpm = -1;
    lcdRenderSelecting(true);
    return;
  }
  lcdRenderRunning(false);
}
