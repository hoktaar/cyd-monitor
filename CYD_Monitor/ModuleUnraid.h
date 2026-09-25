// Unraid-Monitor ueber die offizielle Unraid-API (GraphQL, ab Unraid 7): CPU, RAM, Array und Cache.
// API-Key in Unraid unter Einstellungen -> Management Access -> API Keys anlegen (Rolle "viewer" genuegt).
#pragma once

class UnraidModule : public Module {
 public:
  const char *title() override { return "Unraid"; }

  const Field *fields(int &count) override {
    static const Field F[] = {
        {"ur.url", "Server-Adresse", "text", "http://192.168.1.10"},
        {"ur.key", "API-Key", "password", "Unraid: Einstellungen > Management Access > API Keys"},
    };
    count = 2;
    return F;
  }

  bool ready() override { return prefs.getString("ur.url", "").length() && prefs.getString("ur.key", "").length(); }
  uint32_t fetchInterval() override { return 5000; }

  String status() override {
    DataLock lock;
    if (error_.length()) return error_;
    return valid_ ? "Aktualisiert " + updated_ : "";
  }

  void fetch() override {
    http::Request req;
    req.begin(http::baseUrl(prefs.getString("ur.url", "")) + "/graphql");
    req.client.addHeader("Content-Type", "application/json");
    req.client.addHeader("x-api-key", prefs.getString("ur.key", ""));
    int code = req.client.POST(
        "{\"query\":\"{ metrics { cpu { percentTotal } memory { percentTotal total used } } "
        "array { capacity { kilobytes { free total } } caches { name fsSize fsFree } } }\"}");
    if (code != 200) {
      DataLock lock;
      error_ = "Unraid: " + http::describe(code);
      rev_++;
      req.client.end();
      return;
    }
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, req.client.getStream());
    req.client.end();

    DataLock lock;
    rev_++;
    if (err) {
      error_ = String("Unraid: ") + err.c_str();
      return;
    }
    if (doc["errors"][0]["message"].is<const char *>()) {
      error_ = String("Unraid: ") + doc["errors"][0]["message"].as<const char *>();
      if (doc["data"].isNull()) return;
    } else {
      error_ = "";
    }
    JsonObject m = doc["data"]["metrics"];
    cpu_ = jsonNumber(m["cpu"]["percentTotal"]);
    ram_ = jsonNumber(m["memory"]["percentTotal"]);
    ramUsed_ = jsonNumber(m["memory"]["used"]);
    ramTotal_ = jsonNumber(m["memory"]["total"]);
    JsonObject cap = doc["data"]["array"]["capacity"]["kilobytes"];
    arrayFree_ = jsonNumber(cap["free"]) * 1024;
    arrayTotal_ = jsonNumber(cap["total"]) * 1024;
    cacheFree_ = cacheTotal_ = 0;
    for (JsonObject c : doc["data"]["array"]["caches"].as<JsonArray>()) {
      cacheFree_ += jsonNumber(c["fsFree"]) * 1024;
      cacheTotal_ += jsonNumber(c["fsSize"]) * 1024;
    }
    valid_ = true;
    updated_ = nowHHMM();
  }

  void enter() override {
    shownRev_ = UINT32_MAX;
    tick();
  }

  void tick() override {
    DataLock lock;
    if (rev_ == shownRev_) return;
    shownRev_ = rev_;
    if (!valid_) {
      ui::textBox(10, 90, 300, 30, 20, error_.length() ? error_.c_str() : "Verbinde mit Unraid ...", &FreeSans9pt7b,
                  error_.length() ? ui::BAD : ui::MUTED, ui::BG, ui::CENTER);
      return;
    }
    String ram = ui::fmt(ramUsed_ / 1e9) + " / " + ui::fmt(ramTotal_ / 1e9, 0) + " GB";
    row(0, "CPU", ui::fmt(cpu_, 0) + " %", cpu_);
    row(1, "RAM", ram, ram_);
    row(2, "Array frei", ui::fmtBytes(arrayFree_) + " / " + ui::fmtBytes(arrayTotal_), usedPct(arrayFree_, arrayTotal_));
    if (cacheTotal_ > 0)
      row(3, "Cache frei", ui::fmtBytes(cacheFree_) + " / " + ui::fmtBytes(cacheTotal_), usedPct(cacheFree_, cacheTotal_));
  }

 private:
  static double usedPct(double free, double total) { return total > 0 ? 100.0 * (total - free) / total : 0; }

  void row(int i, const char *label, const String &value, double pct) {
    int y = 36 + i * 50;
    ui::textBox(10, y, 110, 22, 16, label, &FreeSansBold9pt7b, ui::TEXT, ui::BG);
    ui::textBox(120, y, 190, 22, 16, value.c_str(), &FreeSans9pt7b, ui::MUTED, ui::BG, ui::RIGHT);
    ui::bar(10, y + 25, 300, 14, (int)lround(pct), ui::levelColor((int)pct));
  }

  bool valid_ = false;
  double cpu_ = 0, ram_ = 0, ramUsed_ = 0, ramTotal_ = 0;
  double arrayFree_ = 0, arrayTotal_ = 0, cacheFree_ = 0, cacheTotal_ = 0;
  String error_, updated_;
  uint32_t rev_ = 0, shownRev_ = UINT32_MAX;
};
