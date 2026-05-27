# test-nawijarki

Firmware (PlatformIO, ESP32) do nawijarki: wybór RPM enkoderem na LCD, start przyciskiem enkodera, płynny rozruch silnika krokowego (`AccelStepper`), rzadkie odświeżanie LCD w trakcie pracy.

## Konfiguracja sprzętu

Zobacz `docs/KONFIG_SPRZETOWY.txt` oraz `include/hw_config.h` (piny, adres LCD, mikrokroki, przekładnia).

## Budowanie

```bash
pio run -e esp32dev
```

## Wgrywanie

```bash
pio run -e esp32dev -t upload
```
