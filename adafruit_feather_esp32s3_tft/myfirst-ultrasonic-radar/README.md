# myfirst-ultrasonic-radar

HC-SR04 ultrasonic distance sensor wired to an Adafruit Feather ESP32-S3 TFT.
Distance is measured continuously and shown on the built-in TFT (cm and
inches) along with a color-coded range bar.

## Wiring

| HC-SR04 pin | Feather pin | Notes |
|---|---|---|
| VCC  | 3V  | Most HC-SR04 modules work at 3.3V with somewhat reduced range/reliability. Use 5V/USB if you need full range. |
| GND  | GND | |
| TRIG | A0  | ESP32 3.3V output is a valid HIGH for the HC-SR04's trigger input, so this connects directly. |
| ECHO | A1  | **Only if VCC is 5V:** go through a voltage divider (1k from ECHO to A1, 2k from A1 to GND). ECHO idles at VCC, and the ESP32-S3's GPIOs are not 5V tolerant. Skip the divider if the sensor is running from 3.3V. |

## Behavior

- Triggers the sensor and reads the echo about 10 times/second (HC-SR04 needs ≥60ms between triggers).
- Displays distance in cm and inches, plus a range bar (red < 10cm, yellow < 50cm, green beyond).
- Shows "OUT OF RANGE" when no echo returns within the ~400cm max range.
- Also logs each reading over serial at 115200 baud.

## Build / upload

```
pio run
pio run -t upload
pio device monitor
```
