# test-nawijarki

Firmware **Raspberry Pi Pico (RP2040)** do nawijarki pickupów: wybór RPM enkoderem na LCD 20×4 (I2C), start przyciskiem enkodera, płynny rozruch silnika krokowego przez **TMC2209** (STEP/DIR), rzadkie odświeżanie LCD w trakcie pracy.

## Konfiguracja sprzętu

Źródło prawdy: [`docs/KONFIG_SPRZETOWY.txt`](docs/KONFIG_SPRZETOWY.txt) — piny i parametry są powielone w [`include/hw_config.h`](include/hw_config.h).

## Środowisko budowania

Projekt używa **PlatformIO** z rdzeniem **earlephilhower/arduino-pico** (I2C LCD na **GP0/GP1** zgodnie z konfigiem; oficjalne `arduino-mbed` ma domyślny `Wire` na innych pinach).

```bash
pio run -e pico
```

Wgranie (UF2):

```bash
pio run -e pico -t upload
```
