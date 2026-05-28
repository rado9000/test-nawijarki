#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <hardware/pwm.h>

#include <TMCStepper.h>
#include <SerialUART.h>

#include <hw_config.h>

namespace {

LiquidCrystal_I2C gLcd(kLcdI2cAddr, kLcdCols, kLcdRows);

// UART do konfiguracji TMC (impulsy STEP generowane sprzętowo PWM na RP2040).

#if 1
// Serial1 w arduino-pico to SerialUART, ma setPinout().
static SerialUART& gTmcSerial = Serial1;
#endif

#if 1
TMC2208Stepper gDriver2208(&gTmcSerial, kTmcRsenseOhm);
TMC2209Stepper gDriver2209(&gTmcSerial, kTmcRsenseOhm, 0);
#endif

enum class Phase { Selecting, Running };

Phase gPhase = Phase::Selecting;
int32_t gRpm = 100;

uint32_t gLastLcdMs = 0;
int32_t gLastLcdRpm = -1;

uint8_t gEncPrevAb = 0;

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
  const float stepsPerRev = kFullStepsPerRevMotor * static_cast<float>(kTmcMicrosteps);
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

void tmcUartInit() {
  gTmcSerial.setPinout(kPinTmcUartTx, kPinTmcUartRx);
  gTmcSerial.begin(kTmcUartBaud);
}

void tmcConfigure() {
  if (kUseTmc2209) {
    auto& driver = gDriver2209;
    driver.begin();
    driver.pdn_disable(true);
    driver.mstep_reg_select(true);
    driver.toff(kTmcToff);
    driver.rms_current(kTmcRmsCurrentmA);
    driver.microsteps(kTmcMicrosteps);
    driver.en_spreadCycle(true);
    driver.pwm_autoscale(true);
    driver.TCOOLTHRS(0xFFFFF);
  } else {
    auto& driver = gDriver2208;
    driver.begin();
    driver.pdn_disable(true);
    driver.mstep_reg_select(true);
    driver.toff(kTmcToff);
    driver.rms_current(kTmcRmsCurrentmA);
    driver.microsteps(kTmcMicrosteps);
    driver.en_spreadCycle(true);
    driver.pwm_autoscale(true);
  }
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
  gLcd.print(F("RPM silnika"));
  gLcd.setCursor(0, 1);
  gLcd.print(F("Zakres 1..1500"));
  gLcd.setCursor(0, 2);
  char line[21];
  snprintf(line, sizeof(line), "Ustaw: %4ld RPM", static_cast<long>(gRpm));
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
  gLcd.print(F("Praca"));
  gLcd.setCursor(0, 1);
  char line1[21];
  snprintf(line1, sizeof(line1), "RPM: %4ld", static_cast<long>(gRpm));
  gLcd.print(line1);
  gLcd.setCursor(0, 2);
  char line2[21];
  snprintf(line2, sizeof(line2), "STEP: %6.0f Hz", static_cast<double>(gStepHzCurrent));
  gLcd.print(line2);
  gLcd.setCursor(0, 3);
  gLcd.print(kUseTmc2209 ? F("TMC2209 UART") : F("TMC2208 UART"));
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
  if (gRpm < kRpmMin) gRpm = kRpmMin;
  if (gRpm > kRpmMax) gRpm = kRpmMax;

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

  // Limit pochodnej prędkości: steps/s^2.
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
  gSwRaw = digitalRead(kPinEncSw);
  gSwStable = gSwRaw;

  tmcUartInit();
  tmcConfigure();

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

  // Running: brak ciężkiej logiki w pętli — PWM generuje STEP w HW.
  motorRampUpdate();
  lcdRenderRunning(false);
}
