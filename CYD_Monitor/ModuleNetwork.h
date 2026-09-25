// Netzwerkstatus mit QR-Code: verbunden -> Link zur Web-Oberflaeche, Hotspot -> WLAN-Beitritt zum Hotspot.
#pragma once

class NetworkModule : public Module {
 public:
  const char *title() override { return "Netzwerk"; }

  void enter() override {
    shownRev_ = 0;
    tick();
  }

  void tick() override {
    if (shownRev_ != net::rev) {
      shownRev_ = net::rev;
      drawAll();
    } else if (net::online && millis() - lastSignal_ > 10000) {
      drawSignal();
    }
  }

 private:
  static constexpr int COL_W = 138, QR_X = 152, QR_Y = 36, QR_SCALE = 5;

  void line(int i, const char *text, uint16_t color = 0) {
    ui::textBox(10, 76 + i * 24, COL_W, 22, 15, text, &FreeSans9pt7b, color ? color : ui::TEXT, ui::BG);
  }

  void drawSignal() {
    lastSignal_ = millis();
    char text[32];
    snprintf(text, sizeof(text), "Signal %d dBm", WiFi.RSSI());
    line(2, text);
  }

  void drawAll() {
    tft.fillRect(0, ui::HEADER_H, ui::W, ui::H - ui::HEADER_H, ui::BG);
    char url[64];
    if (net::ap) {
      ui::textBox(10, 38, COL_W, 30, 22, "Einrichten", &FreeSansBold12pt7b, ui::ACCENT, ui::BG);
      line(0, "1. QR scannen");
      line(1, "   oder WLAN");
      line(2, "   CYD-Monitor");
      line(3, "2. 192.168.4.1");
      line(4, "3. WLAN wählen");
      snprintf(url, sizeof(url), "WIFI:T:nopass;S:%s;;", net::AP_SSID);
    } else if (net::online) {
      ui::textBox(10, 38, COL_W, 30, 22, "Verbunden", &FreeSansBold12pt7b, ui::TEXT, ui::BG);
      line(0, WiFi.SSID().c_str());
      line(1, WiFi.localIP().toString().c_str());
      drawSignal();
      line(4, "QR scannen für", ui::MUTED);
      line(5, "Einstellungen", ui::MUTED);
      snprintf(url, sizeof(url), "http://%s/", WiFi.localIP().toString().c_str());
    } else {
      ui::textBox(10, 38, 300, 30, 22, "Verbinde ...", &FreeSansBold12pt7b, ui::TEXT, ui::BG);
      line(0, net::ssid().c_str());
      return;
    }
    if (qr::encode(url)) qr::draw(QR_X, QR_Y, QR_SCALE);
  }

  uint32_t shownRev_ = 0;
  uint32_t lastSignal_ = 0;
};
