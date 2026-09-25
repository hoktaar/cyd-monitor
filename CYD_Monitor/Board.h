// Hardware des ESP32-2432S028 ("Cheap Yellow Display"): Display, Touch, RGB-LED, Backlight.
#pragma once
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Preferences.h>

namespace pins {
constexpr int TFT_SCLK = 14, TFT_MISO = 12, TFT_MOSI = 13, TFT_CS = 15, TFT_DC = 2, TFT_BL = 21;
constexpr int T_CLK = 25, T_MOSI = 32, T_MISO = 39, T_CS = 33, T_IRQ = 36;
constexpr int LED_R = 4, LED_G = 16, LED_B = 17;  // active LOW
}

// Bei der CYD-Variante mit USB-C + Micro-USB ist das Panel nativ quer eingebaut: Querformat bedeutet hier
// *kein* Achsentausch (MV). Orientierung 0 = USB-Buchsen rechts, 1 = links. cfg.madctl ueberschreibt das.
class CydDisplay : public Adafruit_ILI9341 {
 public:
  using Adafruit_ILI9341::Adafruit_ILI9341;

  void setRotation(uint8_t r) override {
    static const uint8_t MADCTL[2] = {0x88, 0x48};  // BGR | MY bzw. BGR | MX
    rotation = r & 1;
    uint8_t m = madctlOverride ? madctlOverride : MADCTL[rotation];
    bool swapped = m & 0x20;  // MV: Achsen getauscht -> hochkant
    _width = swapped ? 240 : 320;
    _height = swapped ? 320 : 240;
    sendCommand(ILI9341_MADCTL, &m, 1);
  }

  uint8_t madctlOverride = 0;
};

SPIClass tftSpi(HSPI);
CydDisplay tft(&tftSpi, pins::TFT_DC, pins::TFT_CS, -1);
Preferences prefs;

namespace board {

constexpr int BL_CHANNEL = 0;

void setBacklight(uint8_t level) { ledcWrite(BL_CHANNEL, level); }

void setLed(bool r, bool g, bool b) {
  digitalWrite(pins::LED_R, !r);
  digitalWrite(pins::LED_G, !g);
  digitalWrite(pins::LED_B, !b);
}

void begin() {
  prefs.begin("cyd", false);

  pinMode(pins::LED_R, OUTPUT);
  pinMode(pins::LED_G, OUTPUT);
  pinMode(pins::LED_B, OUTPUT);
  setLed(false, false, false);

  ledcSetup(BL_CHANNEL, 5000, 8);
  ledcAttachPin(pins::TFT_BL, BL_CHANNEL);
  setBacklight(prefs.getUChar("bright", 255));

  tftSpi.begin(pins::TFT_SCLK, pins::TFT_MISO, pins::TFT_MOSI, pins::TFT_CS);
  tft.begin(40000000);
  // Gamma-Korrektur fuer dieses Panel, sonst wirken die Farben blass
  uint8_t gamma = 2;
  tft.sendCommand(ILI9341_GAMMASET, &gamma, 1);
  delay(120);
  gamma = 1;
  tft.sendCommand(ILI9341_GAMMASET, &gamma, 1);
  tft.madctlOverride = prefs.getUChar("madctl", 0);
  tft.setRotation(prefs.getUChar("orient", 0));
  tft.invertDisplay(prefs.getBool("inv", true));

  pinMode(pins::T_CLK, OUTPUT);
  pinMode(pins::T_MOSI, OUTPUT);
  pinMode(pins::T_CS, OUTPUT);
  pinMode(pins::T_MISO, INPUT);
  pinMode(pins::T_IRQ, INPUT);
  digitalWrite(pins::T_CS, HIGH);
  digitalWrite(pins::T_CLK, LOW);
}

}  // namespace board

// XPT2046-Touchcontroller per Bit-Banging (eigene Pins, kein zweiter SPI-Bus noetig).
namespace touch {

uint16_t transfer(uint8_t cmd) {
  for (int i = 7; i >= 0; --i) {
    digitalWrite(pins::T_MOSI, (cmd >> i) & 1);
    digitalWrite(pins::T_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(pins::T_CLK, LOW);
    delayMicroseconds(1);
  }
  uint16_t v = 0;
  for (int i = 0; i < 16; ++i) {
    digitalWrite(pins::T_CLK, HIGH);
    delayMicroseconds(1);
    v = (v << 1) | digitalRead(pins::T_MISO);
    digitalWrite(pins::T_CLK, LOW);
    delayMicroseconds(1);
  }
  return v >> 3;
}

// Liefert Rohwerte (0..4095) und true, solange der Bildschirm gedrueckt ist.
bool read(uint16_t &rawX, uint16_t &rawY) {
  if (digitalRead(pins::T_IRQ) == HIGH) return false;
  digitalWrite(pins::T_CS, LOW);
  int z = transfer(0xB1) + 4095 - transfer(0xC1);
  uint32_t sx = 0, sy = 0;
  for (int i = 0; i < 4; ++i) {
    sx += transfer(0xD1);
    sy += transfer(0x91);
  }
  transfer(0xD0);  // Power-Down-Modus mit aktivem IRQ
  digitalWrite(pins::T_CS, HIGH);
  rawX = sx / 4;
  rawY = sy / 4;
  return z > 300;
}

}  // namespace touch
