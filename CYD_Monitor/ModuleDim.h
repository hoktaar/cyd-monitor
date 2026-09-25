// Dimming & Nightmode Modul: Auto-Helligkeit per Lichtsensor, Nachtzeit, manueller Override
#pragma once
#include "Module.h"
#include "Ui.h"

class DimModule : public Module {
 public:
  const char *title() override { return "Dim & Night"; }
  String headerTitle() override { return "Dim & Night"; }
  int pageCount() override { return 1; }
  const Field *fields(int &count) override {
    static const Field F[] = {
      {"dim.enable", "Dimmen aktiv", "bool", "1"},
      {"dim.light_en", "Auto per Lichtsensor", "bool", "1"},
      {"dim.night_en", "Nachtmodus", "bool", "1"},
      {"dim.night_start", "Nacht-Start HH:MM", "text", "22:00"},
      {"dim.night_end", "Nacht-Ende HH:MM", "text", "06:30"},
      {"dim.min_bright", "Min Helligkeit", "number", "30"},
      {"dim.max_bright", "Max Helligkeit", "number", "255"},
      {"dim.night_bright", "Nacht Helligkeit", "number", "20"},
      {"dim.day_min", "Tag Min Lux", "number", "10"},
      {"dim.day_max", "Tag Max Lux", "number", "500"},
    };
    count = 9;
    return F;
  }
  bool ready() override { return true; }
  uint32_t fetchInterval() override { return 5000; }
  String status() override {
    DataLock lock;
    return String("Helligkeit ") + String(curBright_) + " / Nacht " + (night_ ? "an" : "aus");
  }
  void enter() override {}
  void tick() override {
    DataLock lock;
    if (curBright_ != lastBright_) {
      board::setBacklight(curBright_);
      prefs.putUChar("bright", curBright_);
      lastBright_ = curBright_;
    }
    tft.fillRect(10, 36, 120, 184, ui::BG);
    // Header Text
    ui::textBox(140, 60, 172, 28, 19, "Auto-Helligkeit", &FreeSansBold12pt7b, ui::TEXT, ui::BG);
    ui::textBox(140, 90, 172, 22, 16, String("Aktuell: ") + String(curBright_) + " / " + (night_ ? "Nacht" : "Tag"), &FreeSans9pt7b, ui::MUTED, ui::BG);
    ui::textBox(140, 120, 172, 22, 16, String("Lichtsensor: ") + String(lux_) + " lux", &FreeSans9pt7b, ui::MUTED, ui::BG);
    ui::textBox(140, 150, 172, 22, 16, String("PWM: ") + String(curBright_), &FreeSans9pt7b, ui::MUTED, ui::BG);
  }
  void fetch() override {
    bool en = prefs.getBool("dim.enable", true);
    if (!en) return;
    // read light sensor on GPIO34
    int raw = analogRead(34);
    // map raw 0-4095 to lux approx
    lux_ = (raw * 1000 / 4095);
    // night time check
    night_ = false;
    if (prefs.getBool("dim.night_en", true)) {
      String s = prefs.getString("dim.night_start", "22:00");
      String e = prefs.getString("dim.night_end", "06:30");
      int sh = s.substring(0,2).toInt();
      int sm = s.substring(3,5).toInt();
      int eh = e.substring(0,2).toInt();
      int em = e.substring(3,5).toInt();
      time_t now = time(nullptr);
      struct tm t; localtime_r(&now, &t);
      int mins = t.tm_hour*60 + t.tm_min;
      int smins = sh*60 + sm;
      int emins = eh*60 + em;
      if (emins < smins) {
        night_ = mins >= smins || mins < emins;
      } else {
        night_ = mins >= smins && mins < emins;
      }
    }
    uint8_t minB = prefs.getUChar("dim.min_bright", 30);
    uint8_t maxB = prefs.getUChar("dim.max_bright", 255);
    uint8_t target;
    if (night_) {
      target = prefs.getUChar("dim.night_bright", 20);
    } else if (prefs.getBool("dim.light_en", true)) {
      int minLux = prefs.getInt("dim.day_min", 10);
      int maxLux = prefs.getInt("dim.day_max", 500);
      int mapped = map(constrain(lux_, minLux, maxLux), minLux, maxLux, minB, maxB);
      target = mapped;
    } else {
      target = maxB;
    }
    // smoothing
    curBright_ = (curBright_ * 7 + target) / 8;
  }
 private:
  uint8_t curBright_ = 255;
  uint8_t lastBright_ = 255;
  int lux_ = 0;
  bool night_ = false;
};
