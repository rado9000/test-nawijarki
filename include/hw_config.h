#pragma once

/**
 * Konfiguracja sprzętu — zgodna z docs/KONFIG_SPRZETOWY.txt.
 * STEP/DIR sterowane sprzętowo (PWM), UART używany tylko do konfiguracji drivera TMC.
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

// --- Driver TMC (STEP/DIR + UART konfiguracja) ---
static constexpr int kPinMotorStep = 2;    // GP2 — STEP (hardware PWM)
static constexpr int kPinMotorDir = 3;     // GP3 — DIR
static constexpr int kPinMotorEnable = 10; // GP10 — EN (LOW = sterownik włączony)
static constexpr int kMotorDirCwLevel = HIGH;  // wg KONFIG_SPRZETOWY: HIGH = CW

// UART do PDN_UART (wg Twojego okablowania)
static constexpr int kPinTmcUartTx = 4;  // GP4 (TX) -> PDN_UART przez ~1k
static constexpr int kPinTmcUartRx = 5;  // GP5 (RX) <- PDN_UART
static constexpr uint32_t kTmcUartBaud = 115200;

// Wybór drivera
static constexpr bool kUseTmc2209 = false;  // true jeśli 2209
static constexpr float kTmcRsenseOhm = 0.11f;

// Ustawienia drivera (rejestry przez UART)
static constexpr uint16_t kTmcRmsCurrentmA = 900;
static constexpr uint8_t kTmcToff = 5;
static constexpr uint16_t kTmcMicrosteps = 8;   // 4 albo 8
static constexpr bool kTmcForceSpreadCycle = true;

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

// Minimalny odstęp impulsów enkodera (us) do odfiltrowania drgań styków.
static constexpr uint32_t kEncoderMinPulseUs = 800;

// Start silnika: zaczynamy od tej częstotliwości STEP (Hz), potem rampa do celu.
static constexpr float kMotorStartStepHz = 30.0f;
