#pragma once

/**
 * Konfiguracja sprzętu — zgodna z docs/KONFIG_SPRZETOWY.txt.
 * Wersja BEZ UART: driver TMC pracuje w trybie STEP/DIR, ustawienia prądu/mikrokroku
 * realizujesz zworkami/VREF na module.
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

// --- Driver TMC (STEP/DIR, bez UART) ---
static constexpr int kPinMotorStep = 2;    // GP2 — STEP (hardware PWM)
static constexpr int kPinMotorDir = 3;     // GP3 — DIR
static constexpr int kPinMotorEnable = 10; // GP10 — EN (LOW = sterownik włączony)

static constexpr int kMotorDirCwLevel = HIGH;  // wg KONFIG_SPRZETOWY: HIGH = CW

// Mikrokrok ustawiony sprzętowo na driverze (MS1/MS2...). Musi zgadzać się z tym parametrem.
static constexpr uint16_t kDriverMicrosteps = 8;  // 4 albo 8

// --- Czujnik Hall A3144 ---
static constexpr int kPinHall3144 = 9;     // GP9 — INPUT_PULLUP

// --- Parametry mechaniczne ---
static constexpr float kFullStepsPerRevMotor = 200.0f;
static constexpr float kGearRatioMotorToSpindle = 1.0f;

static constexpr int kRpmMin = 1;
static constexpr int kRpmMax = 1500;

// Maks. przyspieszenie częstotliwości STEP (steps/s^2). Mniejsze = łagodniej.
static constexpr float kMotorAccelerationStepsPerSec2 = 12000.0f;

// LCD w trakcie pracy silnika — nie częściej niż ten interwał (ms).
static constexpr uint32_t kLcdRefreshWhileRunningMs = 1000;
static constexpr uint32_t kEncoderButtonDebounceMs = 40;
