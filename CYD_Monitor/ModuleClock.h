// Digitaluhr. Die Zeit kommt per NTP (WLAN) oder vom PC ("time=<Unixzeit UTC>"); Zeitzone per cfg.tz.
#pragma once
#include <time.h>

class ClockModule : public Module {
 public:
  const char *title() override { return "Uhr"; }

  void enter() override {
    for (int &d : shown_) d = -2;
    lastKey_ = -1;
    lastDay_ = -3;
    barW_ = -1;
    tick();
  }

  void tick() override {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    bool valid = t.tm_year + 1900 >= 2024;

    long key = valid ? (long)now : (long)(millis() / 1000);
    if (key == lastKey_) return;
    lastKey_ = key;

    int digits[4] = {-1, -1, -1, -1};
    if (valid) {
      digits[0] = t.tm_hour / 10;
      digits[1] = t.tm_hour % 10;
      digits[2] = t.tm_min / 10;
      digits[3] = t.tm_min % 10;
    }
    for (int i = 0; i < 4; ++i) {
      if (digits[i] == shown_[i]) continue;
      ui::sevenSeg(X[i], Y, DW, DH, DT, digits[i], ui::TEXT, ui::DIM);
      shown_[i] = digits[i];
    }

    uint16_t colon = (!valid || t.tm_sec % 2 == 0) ? ui::ACCENT : ui::DIM;
    tft.fillRect(ui::W / 2 - 5, Y + 30, 10, 10, colon);
    tft.fillRect(ui::W / 2 - 5, Y + DH - 40, 10, 10, colon);

    int w = valid ? ((t.tm_sec + 1) * BAR_W) / 60 : 0;
    if (w < barW_ || barW_ < 0) tft.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, ui::DIM);
    if (w > 0) tft.fillRect(BAR_X, BAR_Y, w, BAR_H, ui::ACCENT);
    barW_ = w;

    int day = valid ? t.tm_yday : -2;
    if (day != lastDay_) {
      lastDay_ = day;
      char text[64];
      if (valid) {
        static const char *DAYS[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
        static const char *MONTHS[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli",
                                       "August", "September", "Oktober", "November", "Dezember"};
        snprintf(text, sizeof(text), "%s, %d. %s %d", DAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon], t.tm_year + 1900);
      } else {
        snprintf(text, sizeof(text), "Warte auf Uhrzeit ...");
      }
      ui::textBox(0, DATE_Y, ui::W, 34, 25, text, &FreeSans12pt7b, valid ? ui::TEXT : ui::MUTED, ui::BG, ui::CENTER);
    }
  }

 private:
  static constexpr int Y = 46, DW = 58, DH = 108, DT = 12;
  static constexpr int X[4] = {21, 89, 173, 241};
  static constexpr int BAR_X = 21, BAR_Y = 168, BAR_W = 278, BAR_H = 5;
  static constexpr int DATE_Y = 184;

  int shown_[4];
  long lastKey_ = -1;
  int lastDay_ = -3;
  int barW_ = -1;
};

constexpr int ClockModule::X[4];
