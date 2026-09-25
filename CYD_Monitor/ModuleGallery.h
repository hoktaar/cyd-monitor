// Bildergalerie Modul: URLs in WebUI pflegen, random JPEG anzeigen
#pragma once
#include "Module.h"
#include "Jpeg.h"

class GalleryModule : public Module {
 public:
  const char *title() override { return "Galerie"; }
  String headerTitle() override { return "Galerie"; }
  int pageCount() override { return 1; }
  const Field *fields(int &count) override {
    static const Field F[] = {
      {"gallery.url0", "Bild URL 1", "text", ""},
      {"gallery.url1", "Bild URL 2", "text", ""},
      {"gallery.url2", "Bild URL 3", "text", ""},
      {"gallery.url3", "Bild URL 4", "text", ""},
      {"gallery.url4", "Bild URL 5", "text", ""},
      {"gallery.interval", "Wechsel Intervall (s)", "number", "30"},
    };
    count = 6;
    return F;
  }
  bool ready() override {
    for (int i=0;i<5;i++) {
      String k="gallery.url"+String(i);
      if (prefs.getString(k.c_str()).length()>0) return true;
    }
    return false;
  }
  uint32_t fetchInterval() override { return prefs.getUShort("gallery.interval",30)*1000; }
  String status() override { return String("Bild ") + String(currentIdx_+1) + "/"+String(urls_.size()); }
  void enter() override {
    DataLock lock;
    if (urls_.empty()) loadUrls();
    if (!poster_.empty()) return;
    fetchNow = true;
  }
  void tick() override {
    DataLock lock;
    if (!poster_.empty()) {
      jpeg::draw(poster_.data(), poster_.size(),10,38,120,180);
    } else {
      tft.fillRoundRect(10,38,120,180,6,ui::PANEL);
    }
    ui::textBox(140,60,172,28,19,currentTitle_.c_str(), &FreeSansBold12pt7b, ui::TEXT, ui::BG);
  }
  void fetch() override {
    DataLock lock;
    if (urls_.empty()) loadUrls();
    if (urls_.empty()) return;
    // random or sequential
    currentIdx_ = esp_random() % urls_.size();
    String url = urls_[currentIdx_];
    currentTitle_ = url.substring(url.lastIndexOf('/')+1);
    // lock unlock not needed
    loadImage(url);
  }
 private:
  void loadUrls() {
    urls_.clear();
    for (int i=0;i<5;i++) {
      String k="gallery.url"+String(i);
      String u=prefs.getString(k.c_str());
      if (u.length()>0) urls_.push_back(u);
    }
  }
  void loadImage(const String &url) {
    http::Request req;
    req.begin(url);
    if (req.client.GET()!=200) return;
    WiFiClient &s = req.client.getStream();
    std::vector<uint8_t> buf;
    uint8_t tmp[512];
    uint32_t start = millis();
    while ((s.connected()||s.available()) && millis()-start<12000 && buf.size()<120000) {
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
  std::vector<String> urls_;
  std::vector<uint8_t> poster_;
  size_t currentIdx_ = 0;
  String currentTitle_;
};
