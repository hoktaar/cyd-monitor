// Plex: zeigt, was gerade laeuft (mit Fortschritt). Laeuft nichts, bei jedem Erscheinen einen zufaelligen
// neu hinzugefuegten Film bzw. eine neue Folge. Cover kommen verkleinert vom Plex-Server.
#pragma once
#include <vector>
#include "Jpeg.h"

class PlexModule : public Module {
 public:
  const char *title() override { return "Plex"; }

  const Field *fields(int &count) override {
    static const Field F[] = {
        {"plex.url", "Server-Adresse", "text", "http://192.168.1.10:32400"},
        {"plex.token", "Plex-Token", "password", "X-Plex-Token (siehe support.plex.tv, Artikel 204059436)"},
    };
    count = 2;
    return F;
  }

  bool ready() override { return prefs.getString("plex.url", "").length() && prefs.getString("plex.token", "").length(); }
  uint32_t fetchInterval() override { return 10000; }

  String status() override {
    DataLock lock;
    if (error_.length()) return error_;
    if (!valid_) return "";
    return playing_ ? "Läuft: " + current_.title : "Leerlauf, " + String(recent_.size()) + " neue Titel geladen";
  }

  void fetch() override {
    String base = http::baseUrl(prefs.getString("plex.url", "")), token = prefs.getString("plex.token", "");
    Item now;
    bool isPlaying = false;
    {
      http::Request req;
      req.begin(base + "/status/sessions");
      headers(req, token);
      int code = req.client.GET();
      if (code != 200) {
        req.client.end();
        return fail("Plex: " + http::describe(code));
      }
      DynamicJsonDocument doc(8192);
      DeserializationError err = deserializeJson(doc, req.client.getStream(), DeserializationOption::Filter(filter()));
      req.client.end();
      if (err) return fail(String("Plex: ") + err.c_str());
      JsonArray md = doc["MediaContainer"]["Metadata"];
      if (md.size() > 0) {
        JsonObject m = md[0];
        isPlaying = true;
        now = itemFrom(m);
        now.user = m["User"]["title"] | "";
        now.player = m["Player"]["title"] | "";
        now.paused = strcmp(m["Player"]["state"] | "", "paused") == 0;
        now.offset = m["viewOffset"] | 0;
        now.duration = m["duration"] | 0;
      }
    }

    bool needRecent;
    {
      DataLock lock;
      needRecent = !isPlaying && (recent_.empty() || millis() - recentAt_ > 30 * 60 * 1000UL);
    }
    if (needRecent && !loadRecent(base, token)) return;

    String thumb;
    {
      DataLock lock;
      if (isPlaying) {
        current_ = now;
        fetchedAt_ = millis();
      } else if (playing_ || pickNew_ || !current_.title.length()) {
        pick();
      }
      playing_ = isPlaying;
      pickNew_ = false;
      valid_ = true;
      error_ = "";
      rev_++;
      thumb = current_.thumb;
    }
    if (thumb.length() && thumb != posterThumb_) loadPoster(base, token, thumb);
  }

  void enter() override {
    {
      DataLock lock;
      pickNew_ = true;  // bei jedem Erscheinen ein anderer neuer Titel
    }
    fetchNow = true;
    shownRev_ = shownPosterRev_ = UINT32_MAX;
    shownKey_ = "";
    lastProgress_ = 0;
    tick();
  }

  void tick() override {
    DataLock lock;
    if (posterRev_ != shownPosterRev_) {
      shownPosterRev_ = posterRev_;
      tft.fillRect(10, 36, 120, 184, ui::BG);
      if (poster_.empty() || !jpeg::draw(poster_.data(), poster_.size(), 10, 38, 120, 180))
        tft.fillRoundRect(10, 38, 120, 180, 6, ui::PANEL);
    }
    if (rev_ != shownRev_) {
      shownRev_ = rev_;
      drawTexts();
    }
    if (playing_ && current_.duration && millis() - lastProgress_ >= 1000) {
      lastProgress_ = millis();
      drawProgress();
    }
  }

 private:
  struct Item {
    String title, subtitle, thumb, user, player;
    bool paused = false;
    uint32_t offset = 0, duration = 0;
  };

  static void headers(http::Request &req, const String &token) {
    req.client.addHeader("Accept", "application/json");
    req.client.addHeader("X-Plex-Token", token);
    req.client.addHeader("X-Plex-Product", "CYD Monitor");
    req.client.addHeader("X-Plex-Client-Identifier", "cyd-monitor");
  }

  static const JsonDocument &filter() {
    static StaticJsonDocument<512> f;
    if (f.isNull()) {
      static const char *KEYS[] = {"type", "title", "parentTitle", "grandparentTitle", "parentIndex", "index", "year",
                                   "thumb", "parentThumb", "grandparentThumb", "viewOffset", "duration"};
      for (const char *k : KEYS) f["MediaContainer"]["Metadata"][0][k] = true;
      f["MediaContainer"]["Metadata"][0]["User"]["title"] = true;
      f["MediaContainer"]["Metadata"][0]["Player"]["title"] = true;
      f["MediaContainer"]["Metadata"][0]["Player"]["state"] = true;
    }
    return f;
  }

  static Item itemFrom(JsonObject m) {
    Item it;
    String type = m["type"] | "";
    char buf[96];
    if (type == "episode") {
      it.title = m["grandparentTitle"] | "";
      snprintf(buf, sizeof(buf), "S%02d E%02d · %s", (int)(m["parentIndex"] | 0), (int)(m["index"] | 0),
               (const char *)(m["title"] | ""));
      it.subtitle = buf;
      it.thumb = m["grandparentThumb"] | (m["thumb"] | "");
    } else if (type == "season") {
      it.title = m["parentTitle"] | "";
      it.subtitle = String(m["title"] | "") + " · neue Folgen";
      it.thumb = m["thumb"] | (m["parentThumb"] | "");
    } else if (type == "track") {
      it.title = m["title"] | "";
      it.subtitle = m["grandparentTitle"] | "";
      it.thumb = m["parentThumb"] | (m["thumb"] | "");
    } else {
      it.title = m["title"] | "";
      int year = m["year"] | 0;
      it.subtitle = year ? String(year) : "";
      it.thumb = m["thumb"] | "";
    }
    return it;
  }

  bool loadRecent(const String &base, const String &token) {
    http::Request req;
    req.begin(base + "/library/recentlyAdded?X-Plex-Container-Start=0&X-Plex-Container-Size=40");
    headers(req, token);
    int code = req.client.GET();
    if (code != 200) {
      req.client.end();
      fail("Plex (neu): " + http::describe(code));
      return false;
    }
    DynamicJsonDocument doc(24576);
    DeserializationError err = deserializeJson(doc, req.client.getStream(), DeserializationOption::Filter(filter()));
    req.client.end();
    if (err) {
      fail(String("Plex (neu): ") + err.c_str());
      return false;
    }
    std::vector<Item> items;
    for (JsonObject m : doc["MediaContainer"]["Metadata"].as<JsonArray>()) {
      String type = m["type"] | "";
      if (type == "movie" || type == "episode" || type == "season" || type == "show") items.push_back(itemFrom(m));
    }
    DataLock lock;
    recent_.swap(items);
    recentAt_ = millis();
    return true;
  }

  // unter DataLock aufrufen
  void pick() {
    if (recent_.empty()) {
      current_ = Item();
      return;
    }
    size_t i = esp_random() % recent_.size();
    if (recent_.size() > 1 && recent_[i].title == current_.title) i = (i + 1) % recent_.size();
    current_ = recent_[i];
  }

  void loadPoster(const String &base, const String &token, const String &thumb) {
    std::vector<uint8_t> buf;
    http::Request req;
    req.begin(base + "/photo/:/transcode?width=120&height=180&upscale=1&url=" + http::urlEncode(thumb));
    headers(req, token);
    if (req.client.GET() == 200) {
      WiFiClient &s = req.client.getStream();
      uint8_t tmp[512];
      uint32_t start = millis();
      while ((s.connected() || s.available()) && millis() - start < 8000 && buf.size() < 60000) {
        int n = s.read(tmp, sizeof(tmp));
        if (n > 0) buf.insert(buf.end(), tmp, tmp + n);
        else delay(2);
      }
    }
    req.client.end();
    DataLock lock;
    poster_.swap(buf);
    posterThumb_ = thumb;
    posterRev_++;
  }

  void fail(const String &msg) {
    DataLock lock;
    error_ = msg;
    rev_++;
  }

  // unter DataLock aufrufen
  void drawTexts() {
    const int X = 140, W = 172;
    if (!valid_) {
      tft.fillRect(X, 30, ui::W - X, ui::H - 30, ui::BG);
      String lines[4];
      int n = ui::wrap(error_.length() ? error_ : String("Verbinde mit Plex ..."), &FreeSans9pt7b, W, lines, 4);
      for (int i = 0; i < n; ++i)
        ui::textBox(X, 90 + i * 20, W, 20, 14, lines[i].c_str(), &FreeSans9pt7b, error_.length() ? ui::BAD : ui::MUTED, ui::BG);
      return;
    }
    String key = String(playing_) + current_.title + current_.subtitle;
    if (key != shownKey_) {
      tft.fillRect(X - 4, 30, ui::W - X + 4, ui::H - 30, ui::BG);
      shownKey_ = key;
    }
    if (!current_.title.length()) {
      ui::textBox(X, 90, W, 24, 16, "Keine neuen Titel", &FreeSans9pt7b, ui::MUTED, ui::BG);
      return;
    }
    const char *state = playing_ ? (current_.paused ? "Pausiert" : "Läuft gerade") : "Neu auf Plex";
    ui::textBox(X, 36, W, 20, 14, state, &FreeSansBold9pt7b, ui::ACCENT, ui::BG);
    int y = 60;
    String lines[3];
    int n = ui::wrap(current_.title, &FreeSansBold12pt7b, W, lines, 3);
    for (int i = 0; i < n; ++i, y += 26) ui::textBox(X, y, W, 26, 19, lines[i].c_str(), &FreeSansBold12pt7b, ui::TEXT, ui::BG);
    y += 4;
    n = ui::wrap(current_.subtitle, &FreeSans9pt7b, W, lines, 2);
    for (int i = 0; i < n; ++i, y += 20) ui::textBox(X, y, W, 20, 14, lines[i].c_str(), &FreeSans9pt7b, ui::MUTED, ui::BG);
    if (playing_ && current_.user.length()) {
      String who = current_.user + (current_.player.length() ? " · " + current_.player : "");
      ui::textBox(X, y + 4, W, 20, 14, who.c_str(), &FreeSans9pt7b, ui::MUTED, ui::BG);
    }
  }

  static String clock(uint32_t ms) {
    uint32_t s = ms / 1000;
    char buf[12];
    if (s >= 3600) snprintf(buf, sizeof(buf), "%u:%02u:%02u", s / 3600, (s / 60) % 60, s % 60);
    else snprintf(buf, sizeof(buf), "%u:%02u", s / 60, s % 60);
    return buf;
  }

  // unter DataLock aufrufen
  void drawProgress() {
    uint32_t pos = current_.offset + (current_.paused ? 0 : millis() - fetchedAt_);
    pos = min(pos, current_.duration);
    ui::bar(140, 200, 172, 8, (int)(100ULL * pos / current_.duration), ui::ACCENT);
    String t = clock(pos) + " / " + clock(current_.duration);
    ui::textBox(140, 212, 172, 22, 16, t.c_str(), &FreeSans9pt7b, ui::MUTED, ui::BG);
  }

  // von fetch() geschrieben (unter DataLock)
  bool valid_ = false, playing_ = false, pickNew_ = true;
  Item current_;
  std::vector<Item> recent_;
  uint32_t recentAt_ = 0, fetchedAt_ = 0;
  std::vector<uint8_t> poster_;
  String posterThumb_, error_;
  uint32_t rev_ = 0, posterRev_ = 0;

  uint32_t shownRev_ = UINT32_MAX, shownPosterRev_ = UINT32_MAX, lastProgress_ = 0;
  String shownKey_;
};
