#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_ST7789.h>

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

String ssidTicker;
unsigned long tickerLastMillis = 500;
unsigned long scanLastMillis = 0;
unsigned long tempLastMillis = 0;
int tickerPosition = 0;
const unsigned long tickerInterval = 500;
const unsigned long scanInterval = 30000;
const unsigned long tempInterval = 1000;

float getInternalTemperatureC() {
  return temperatureRead();
}

void displayTest() {
  tft.fillScreen(ST77XX_RED);
  delay(250);
  tft.fillScreen(ST77XX_GREEN);
  delay(250);
  tft.fillScreen(ST77XX_BLUE);
  delay(250);

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 20);
  tft.print("DISPLAY TEST");
  tft.setCursor(0, 50);
  tft.print("If this is blank");
  tft.setCursor(0, 80);
  tft.print("check wiring");
  delay(2000);
}

void scanWifiNetworks() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.print("Scanning WiFi...");

  int n = WiFi.scanNetworks();
  ssidTicker = "";

  if (n <= 0) {
    ssidTicker = "No networks found";
  } else {
    for (int i = 0; i < n; ++i) {
      ssidTicker += WiFi.SSID(i);
      if (i < n - 1) {
        ssidTicker += "   ";
      }
    }
  }

  tickerPosition = 0;
  tickerLastMillis = millis();
  scanLastMillis = millis();
  WiFi.scanDelete();
}

void drawTemperature(float temperatureC) {
  char buffer[32];
  if (isnan(temperatureC)) {
    strcpy(buffer, "Temp sensor unavailable");
  } else {
    sprintf(buffer, "Chip temp: %.1f C", temperatureC);
  }
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.fillRect(0, 0, tft.width(), 30, ST77XX_BLACK);
  tft.print(buffer);
}

void drawTicker() {
  if (ssidTicker.length() == 0) {
    return;
  }

  int w = tft.width();
  int textHeight = 30;
  tft.fillRect(0, 34, w, textHeight, ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setTextSize(2);

  String displayText = ssidTicker + "   ";
  int totalLength = displayText.length();
  if (totalLength == 0) {
    return;
  }

  if (tickerPosition >= totalLength) {
    tickerPosition = 0;
  }

  String current = displayText.substring(tickerPosition);
  tft.setCursor(0, 40);
  tft.print(current);

  if (current.length() * 12 < w) {
    tft.print(displayText);
  }
}

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200, SERIAL_8N1, RX, TX);

  // The display's logic/backlight power rides on a switch FET gated by
  // TFT_I2C_POWER; it must be driven high before tft.init() or the panel
  // stays on whatever the factory demo last drew.
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);
  delay(10);

  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  displayTest();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  scanWifiNetworks();
  drawTemperature(getInternalTemperatureC());
}

void loop() {
  unsigned long now = millis();

  if (now - scanLastMillis >= scanInterval) {
    scanWifiNetworks();
  }

  if (now - tempLastMillis >= tempInterval) {
    drawTemperature(getInternalTemperatureC());
    tempLastMillis = now;
  }

  if (now - tickerLastMillis >= tickerInterval) {
    tickerPosition++;
    drawTicker();
    tickerLastMillis = now;
  }

  while (Serial.available()) {
    Serial0.write(Serial.read());
  }
  while (Serial0.available()) {
    Serial.write(Serial0.read());
  }
}

