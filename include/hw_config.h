#pragma once

/**
 * Konfiguracja sprzętu — 1:1 z docs/KONFIG_SPRZETOWY.txt (Pico + TMC2209 + LCD 20×4).
 */

// --- LCD I2C (PCF8574 backpack, 20×4) ---
static constexpr uint8_t kLcdI2cAddr = 0x27;
static constexpr uint8_t kLcdCols = 20;
static constexpr uint8_t kLcdRows = 4;
static constexpr int kPinLcdSda = 0;   // GP0
static constexpr int kPinLcdScl = 1;   // GP1

// --- Enkoder obrotowy z przyciskiem ---
static constexpr int kPinEncClk = 6;   // GP6 — kanał A / CLK
static constexpr int kPinEncDt = 7;    // GP7 — kanał B / DT
static constexpr int kPinEncSw = 8;    // GP8 — przycisk (LOW = wciśnięty, INPUT_PULLUP)

// --- TMC2209 (tryb STEP/DIR, UART nieużywany w firmware) ---
static constexpr int kPinMotorStep = 2;    // GP2 — STEP
static constexpr int kPinMotorDir = 3;     // GP3 — DIR (HIGH = CW wg dokumentu)
static constexpr int kPinMotorEnable = 10; // GP10 — EN (LOW = sterownik włączony)

// --- Czujnik Hall A3144 (na razie tylko pin z dokumentacji; rozszerzenie: licznik obrotów) ---
static constexpr int kPinHall3144 = 9;     // GP9 — INPUT_PULLUP, zbocze FALLING

// --- Silnik 17HS4401: 200 kroków/obrót × 1/8 microstep (MS1=LOW, MS2=LOW) = 1600 kroków/obrót ---
static constexpr float kFullStepsPerRevMotor = 200.0f;
static constexpr int kMicrostepping = 8;
// Mnożnik obrotów silnika na 1 obrót wrzeciona (bez przekładni = 1).
static constexpr float kGearRatioMotorToSpindle = 1.0f;

static constexpr int kRpmMin = 1;
static constexpr int kRpmMax = 1500;

// Przyspieszenie AccelStepper (kroki/s²); zmniejsz przy drganiach drutu.
static constexpr float kMotorAccelerationStepsPerSec2 = 12000.0f;

// LCD w trakcie pracy silnika — nie częściej niż ten interwał (ms).
static constexpr uint32_t kLcdRefreshWhileRunningMs = 1000;

static constexpr uint32_t kEncoderButtonDebounceMs = 40;
