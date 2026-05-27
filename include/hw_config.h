#pragma once

/**
 * Konfiguracja sprzętu — dopasuj do połączeń z docs/KONFIG_SPRZETOWY.txt
 * (adres LCD I2C, piny enkodera, STEP/DIR/ENABLE, parametry silnika).
 */

// --- LCD I2C (np. PCF8574, adres często 0x27 lub 0x3F) ---
static constexpr uint8_t kLcdI2cAddr = 0x27;
static constexpr int kPinLcdSda = 21;
static constexpr int kPinLcdScl = 22;

// --- Enkoder obrotowy z przyciskiem ---
static constexpr int kPinEncClk = 18;   // A / CLK
static constexpr int kPinEncDt = 19;    // B / DT
static constexpr int kPinEncSw = 23;    // przycisk (do masy, INPUT_PULLUP)

// --- Sterownik silnika krokowego (STEP/DIR, np. A4988 / DRV8825 / TMC2209 w trybie STEP) ---
static constexpr int kPinMotorStep = 25;
static constexpr int kPinMotorDir = 26;
static constexpr int kPinMotorEnable = 27;  // -1 jeśli nieużywany; A4988: LOW = włączony

// --- Parametry mechaniczne / elektryczne (dostosuj do swojej przekładni i mikrokroków) ---
// Pełne kroki na obrót wirnika (typowo 200), × mikrokroki sterownika (np. 1/16).
static constexpr float kFullStepsPerRevMotor = 200.0f;
static constexpr int kMicrostepping = 16;
// Jeśli jest przekładnia: mnożnik obrotów silnika na 1 obrót wrzeciona (np. 10:1 → 10).
static constexpr float kGearRatioMotorToSpindle = 1.0f;

// --- Ograniczenia RPM (żądane przez użytkownika) ---
static constexpr int kRpmMin = 1;
static constexpr int kRpmMax = 1500;

// --- Płynność rozruchu (AccelStepper: przyspieszenie w krokach/s²) ---
// Im mniejsza wartość, tym łagodniejszy start (dłuższy dojazd do zadanej prędkości).
static constexpr float kMotorAccelerationStepsPerSec2 = 18000.0f;

// --- LCD: odświeżanie w trybie pracy silnika (ms); wyższa = mniej zakłóceń / mniej obciążenia CPU ---
static constexpr uint32_t kLcdRefreshWhileRunningMs = 1000;

// --- Wejścia ---
static constexpr uint32_t kEncoderButtonDebounceMs = 40;
