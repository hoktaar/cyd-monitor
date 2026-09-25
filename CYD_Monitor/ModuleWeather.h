// Animiertes Wetter von Open-Meteo (kostenlos, ohne API-Key). Ort wird in der Web-Oberflaeche gesucht.
#pragma once
#include "WeatherIcons.h"

class WeatherModule : public Module {
 public:
  const char *title() override { return "Wetter"; }

  const Field *fields(int &count) override {
    static const Field F[] = {{"wx", "Ort", "location", "Stadt suchen, z. B. Berlin"}};
    count = 1;
    return F;
  }

  bool ready() override { return prefs.getString("wx.la", "").length() > 0; }
  uint32_t fetchInterval() override { return 10 * 60 * 1000UL; }

  String status() override {
    DataLock lock;
    if (error_.length()) return error_;
    return valid_ ? "Aktualisiert " + updated_ : "";
  }

  void fetch() override {
    String lat = prefs.getString("wx.la", ""), lon = prefs.getString("wx.lo", "");
    if (!lat.length()) return;
    http::Request req;
    req.begin("http://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon +
              "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
              "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
              "&timezone=auto&forecast_days=4");
    int code = req.client.GET();
    if (code != 200) {
      DataLock lock;
      error_ = "Open-Meteo: " + http::describe(code);
      rev_++;
      return;
    }
    DynamicJsonDocument doc(6144);
    DeserializationError err = deserializeJson(doc, req.client.getStream());
    req.client.end();
    DataLock lock;
    rev_++;
    if (err) {
      error_ = String("Open-Meteo: ") + err.c_str();
      return;
    }
    JsonObject cur = doc["current"];
    temp_ = cur["temperature_2m"];
    feels_ = cur["apparent_temperature"];
    humidity_ = cur["relative_humidity_2m"];
    wind_ = cur["wind_speed_10m"];
    code_ = cur["weather_code"];
    day_ = cur["is_day"] | 1;
    JsonObject daily = doc["daily"];
    for (int i = 0; i < 4; ++i) {
      Day &d = days_[i];
      d.code = daily["weather_code"][i];
      d.max = daily["temperature_2m_max"][i];
      d.min = daily["temperature_2m_min"][i];
      d.rain = daily["precipitation_probability_max"][i] | 0;
      const char *date = daily["time"][i] | "";
      struct tm t = {};
      if (sscanf(date, "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday) == 3) {
        t.tm_year -= 1900;
        t.tm_mon -= 1;
        t.tm_hour = 12;
        mktime(&t);
        d.wday = t.tm_wday;
      }
    }
    valid_ = true;
    error_ = "";
    updated_ = nowHHMM();
  }

  void enter() override {
    shownRev_ = UINT32_MAX;
    tick();
  }

  void tick() override {
    uint32_t rev;
    {
      DataLock lock;
      rev = rev_;
    }
    if (rev != shownRev_) {
      shownRev_ = rev;
      drawTexts();
    }
    if (millis() - lastFrame_ >= 50 && valid_) {
      lastFrame_ = millis();
      if (!big_) big_ = new GFXcanvas16(100, 100);
      if (big_->getBuffer()) wx::draw(*big_, 10, 38, 100, wx::kindOf(code_), day_, millis() / 1000.0f);
    }
  }

 private:
  struct Day {
    int code = 0, rain = 0, wday = 0;
    float max = 0, min = 0;
  };

  void drawTexts() {
    DataLock lock;
    char buf[64];
    if (!valid_) {
      ui::textBox(10, 90, 300, 30, 20, error_.length() ? error_.c_str() : "Lade Wetter ...", &FreeSans12pt7b,
                  error_.length() ? ui::BAD : ui::MUTED, ui::BG, ui::CENTER);
      return;
    }
    const int X = 122, W = 190;
    ui::textBox(X, 36, W, 20, 14, prefs.getString("wx.n", "").c_str(), &FreeSans9pt7b, ui::MUTED, ui::BG);
    snprintf(buf, sizeof(buf), "%d°", (int)lroundf(temp_));
    ui::textBox(X, 54, W, 46, 38, buf, &FreeSansBold24pt7b, ui::TEXT, ui::BG);
    ui::textBox(X, 100, W, 26, 19, wx::describe(code_), &FreeSans12pt7b, ui::TEXT, ui::BG);
    snprintf(buf, sizeof(buf), "Gefühlt %d°", (int)lroundf(feels_));
    ui::textBox(X, 126, W, 20, 14, buf, &FreeSans9pt7b, ui::MUTED, ui::BG);
    snprintf(buf, sizeof(buf), "Wind %d km/h · %d %%", (int)lroundf(wind_), humidity_);
    ui::textBox(X, 146, W, 20, 14, buf, &FreeSans9pt7b, ui::MUTED, ui::BG);

    tft.drawFastHLine(10, 170, 300, ui::DIM);
    static const char *DAYS[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    if (!small_) small_ = new GFXcanvas16(36, 36);
    for (int i = 0; i < 3; ++i) {
      const Day &d = days_[i + 1];
      int x = 10 + i * 103;
      if (small_->getBuffer()) wx::draw(*small_, x, 184, 36, wx::kindOf(d.code), true, 0);
      ui::textBox(x + 42, 176, 60, 18, 14, DAYS[d.wday], &FreeSansBold9pt7b, ui::TEXT, ui::BG);
      snprintf(buf, sizeof(buf), "%d°/%d°", (int)lroundf(d.max), (int)lroundf(d.min));
      ui::textBox(x + 42, 196, 60, 18, 14, buf, &FreeSans9pt7b, ui::TEXT, ui::BG);
      snprintf(buf, sizeof(buf), "%d %%", d.rain);
      ui::textBox(x + 42, 216, 60, 18, 14, buf, &FreeSans9pt7b, ui::MUTED, ui::BG);
    }
  }

  // von fetch() geschrieben (unter DataLock)
  bool valid_ = false;
  float temp_ = 0, feels_ = 0, wind_ = 0;
  int humidity_ = 0, code_ = 0;
  bool day_ = true;
  Day days_[4];
  String error_, updated_;
  uint32_t rev_ = 0;

  uint32_t shownRev_ = UINT32_MAX;
  uint32_t lastFrame_ = 0;
  GFXcanvas16 *big_ = nullptr;
  GFXcanvas16 *small_ = nullptr;
};
