// JSON Remote Slide: fetches JSON from URL, displays title/text, optional image/QR
#pragma once
#include "Module.h"
#include "Jpeg.h"

class JsonRemoteModule : public Module {
 public:
  const char *title() override { return "Remote"; }
  String headerTitle() override { return "Remote"; }
  int pageCount() override { return 1; }
  const Field *fields(int &count) override {
    static const Field F[] = {
      {"remote.url", "JSON URL", "text", "http://example.com/remote.json"},
      {"remote.interval", "Abfrage Intervall (s)", "number", "60"},
      {"remote.timeout", "Timeout (s)", "number", "8"},
    };
    count = 3;
    return F;
  }
  bool ready() override { return prefs.getString("remote.url","").length() > 0; }
  uint32_t fetchInterval() override { return prefs.getUShort("remote.interval",60)*1000; }
  String status() override { return String("Letzte Aktualisierung: ") + lastUpdate_; }
  void enter() override { fetchNow = true; }
  void tick() override {
    DataLock lock;
    tft.fillRect(10,36,120,184,ui::BG);
    if (poster_.empty()) {
      tft.fillRoundRect(10,38,120,180,6,ui::PANEL);
    } else {
      jpeg::draw(poster_.data(), poster_.size(),10,38,120,180);
    }
    int y = 60;
    ui::textBox(140,y,172,28,19,title_.c_str(), &FreeSansBold12pt7b, ui::TEXT, ui::BG); y+=32;
    ui::textBox(140,y,172,22,16,text_.c_str(), &FreeSans9pt7b, ui::MUTED, ui::BG); y+=24;
    if (color_.length()) {
      // color not used directly
    }
  }
  void fetch() override {
    String url = prefs.getString("remote.url","");
    if (url.length()==0) return;
    http::Request req;
    req.begin(url);
    if (req.client.GET()!=200) { lastError_="HTTP Fehler"; return; }
    String body;
    WiFiClient &s = req.client.getStream();
    while (s.available()) body+= (char)s.read();
    req.client.end();
    // parse simple JSON
    title_ = jsonField(body,"title");
    text_ = jsonField(body,"text");
    String img = jsonField(body,"image");
    String qr = jsonField(body,"qr");
    if (img.length()>0) {
      loadImage(img);
    } else if (qr.length()>0) {
      // could generate QR, skip for brevity
    }
    lastUpdate_ = String(millis());
  }
 private:
  String jsonField(const String &json, const char *key) {
    String needle = "\""+String(key)+"\":\"";
    int s = json.indexOf(needle);
    if (s<0) return "";
    s += needle.length();
    String out;
    for (int i=s;i<json.length() && json[i]!='"';++i) {
      if (json[i]=='\\' && i+1<json.length()) { ++i; out+=json[i]; }
      else out+=json[i];
    }
    return out;
  }
  void loadImage(const String &url) {
    // very simplified: download and decode JPEG
    http::Request req;
    req.begin(url);
    if (req.client.GET()!=200) return;
    WiFiClient &s = req.client.getStream();
    std::vector<uint8_t> buf;
    uint8_t tmp[512];
    uint32_t start = millis();
    while ((s.connected()||s.available()) && millis()-start<8000 && buf.size()<60000) {
      int n = s.read(tmp,sizeof(tmp));
      if (n>0) buf.insert(buf.end(),tmp,tmp+n);
      else delay(2);
    }
    req.client.end();
    if (buf.size()>0) {
      DataLock lock;
      poster_.swap(buf);
    }
  }
  String title_, text_, color_, lastUpdate_, lastError_;
  std::vector<uint8_t> poster_;
};
