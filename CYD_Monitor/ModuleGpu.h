// Nvidia-GPU (VRAM, Last, Temperatur, Leistung) ueber den Container "nvidia_gpu_exporter"
// (Unraid Community Apps, Standardport 9835). Liest das Prometheus-Textformat von /metrics.
#pragma once

class GpuModule : public Module {
 public:
  const char *title() override { return "GPU"; }

  const Field *fields(int &count) override {
    static const Field F[] = {{"gpu.url", "Exporter-Adresse", "text", "http://192.168.1.10:9835"}};
    count = 1;
    return F;
  }

  bool ready() override { return prefs.getString("gpu.url", "").length() > 0; }
  uint32_t fetchInterval() override { return 3000; }

  String status() override {
    DataLock lock;
    if (error_.length()) return error_;
    return valid_ ? name_ + ", aktualisiert " + updated_ : "";
  }

  void fetch() override {
    String url = http::baseUrl(prefs.getString("gpu.url", ""));
    if (!url.endsWith("/metrics")) url += "/metrics";
    http::Request req;
    req.begin(url);
    int code = req.client.GET();
    if (code != 200) {
      DataLock lock;
      error_ = "Exporter: " + http::describe(code);
      rev_++;
      req.client.end();
      return;
    }
    double util = -1, used = -1, total = -1, temp = -1, power = -1;
    String name;
    WiFiClient &s = req.client.getStream();
    s.setTimeout(3000);
    while (s.connected() || s.available()) {
      String line = s.readStringUntil('\n');
      if (!line.startsWith("nvidia_smi_")) continue;
      double v = atof(line.c_str() + line.lastIndexOf(' ') + 1);
      if (line.startsWith("nvidia_smi_utilization_gpu_ratio") && util < 0) util = v * 100;
      else if (line.startsWith("nvidia_smi_memory_used_bytes") && used < 0) used = v;
      else if (line.startsWith("nvidia_smi_memory_total_bytes") && total < 0) total = v;
      else if (line.startsWith("nvidia_smi_temperature_gpu") && temp < 0) temp = v;
      else if (line.startsWith("nvidia_smi_power_draw_watts") && power < 0) power = v;
      else if (line.startsWith("nvidia_smi_gpu_info") && !name.length()) {
        int a = line.indexOf("name=\"");
        if (a >= 0) name = line.substring(a + 6, line.indexOf('"', a + 6));
      }
    }
    req.client.end();

    DataLock lock;
    rev_++;
    if (total <= 0) {
      error_ = "Exporter: keine Nvidia-Werte gefunden";
      return;
    }
    util_ = util;
    used_ = used;
    total_ = total;
    temp_ = temp;
    power_ = power;
    name_ = name.length() ? name : "Nvidia-GPU";
    name_.replace("NVIDIA ", "");
    valid_ = true;
    error_ = "";
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
      ui::textBox(10, 90, 300, 30, 20, error_.length() ? error_.c_str() : "Verbinde mit Exporter ...", &FreeSans9pt7b,
                  error_.length() ? ui::BAD : ui::MUTED, ui::BG, ui::CENTER);
      return;
    }
    ui::textBox(10, 34, 300, 22, 16, name_.c_str(), &FreeSansBold9pt7b, ui::MUTED, ui::BG);

    double vram = 100 * used_ / total_;
    big(62, "VRAM", vram, ui::fmt(used_ / 1e9) + " / " + ui::fmt(total_ / 1e9) + " GB");
    big(132, "GPU-Last", util_, "");

    char buf[48] = "";
    if (temp_ >= 0 && power_ >= 0) snprintf(buf, sizeof(buf), "%d °C  ·  %d W", (int)lround(temp_), (int)lround(power_));
    else if (temp_ >= 0) snprintf(buf, sizeof(buf), "%d °C", (int)lround(temp_));
    ui::textBox(10, 202, 300, 30, 22, buf, &FreeSans12pt7b, ui::TEXT, ui::BG, ui::CENTER);
  }

 private:
  void big(int y, const char *label, double pct, const String &detail) {
    char p[12];
    snprintf(p, sizeof(p), "%d %%", (int)lround(pct));
    ui::textBox(10, y, 150, 24, 17, label, &FreeSansBold9pt7b, ui::TEXT, ui::BG);
    ui::textBox(10, y + 22, 150, 22, 16, detail.c_str(), &FreeSans9pt7b, ui::MUTED, ui::BG);
    ui::textBox(160, y, 150, 40, 32, p, &FreeSansBold18pt7b, ui::levelColor((int)pct), ui::BG, ui::RIGHT);
    ui::bar(10, y + 48, 300, 14, (int)lround(pct), ui::levelColor((int)pct));
  }

  bool valid_ = false;
  double util_ = 0, used_ = 0, total_ = 0, temp_ = -1, power_ = -1;
  String name_, error_, updated_;
  uint32_t rev_ = 0, shownRev_ = UINT32_MAX;
};
