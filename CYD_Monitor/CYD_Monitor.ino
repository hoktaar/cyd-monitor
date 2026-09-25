// CYD Monitor: modularer Info-Bildschirm fuer das ESP32-2432S028.
// Daten kommen als Zeilen "schluessel=wert" per USB-Seriell oder per WLAN (POST /api/data),
// Einstellungen ueber die Web-Oberflaeche (http://cyd-monitor.local) oder als cfg.*-Zeilen.
// Die Anzeigen wechseln automatisch (cfg.cycle=<Sekunden>, 0 = aus); Tippen schaltet sofort weiter.

#include "version.h"
#include "Board.h"
#include "DataStore.h"
#include "Ui.h"
#include "Net.h"
#include "Ota.h"
#include "Qr.h"
#include "Fetch.h"
#include "Module.h"
#include "ModuleClock.h"
#include "ModuleWeather.h"
#include "ModulePlex.h"
#include "ModuleUnraid.h"
#include "ModuleGpu.h"
#include "ModuleNetwork.h"

ClockModule clockModule;
WeatherModule weatherModule;
PlexModule plexModule;
UnraidModule unraidModule;
GpuModule gpuModule;
NetworkModule networkModule;

Module *MODULES[] = {&clockModule, &weatherModule, &plexModule, &unraidModule, &gpuModule, &networkModule};
constexpr int MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
constexpr int NETWORK_PAGE = MODULE_COUNT - 1;

int current = 0;
uint32_t lastRx = 0;
uint32_t lastSwitch = 0;
uint16_t cycleSeconds = 10;  // 0 = kein automatisches Weiterschalten
uint16_t pageMask = 0xFFFF;  // welche Seiten in der Rotation sind
bool connectedShown = false;
bool apShown = false;
bool paused = false;  // Slide-Rotation angehalten (Symbol in der Kopfzeile oder Web-Oberflaeche)
uint16_t lastTouchX = 0, lastTouchY = 0;
int16_t lastTouchSX = -1, lastTouchSY = -1;
String lineBuf;

bool pageEnabled(int i) { return (pageMask >> i) & 1; }
// In der Rotation: eingeschaltet und eingerichtet
bool inRotation(int i) { return pageEnabled(i) && MODULES[i]->ready(); }

void showModule(int index, int sub = 0) {
  index = ((index % MODULE_COUNT) + MODULE_COUNT) % MODULE_COUNT;
  for (int n = 0; n < MODULE_COUNT && !net::ap && !inRotation(index); ++n) index = (index + 1) % MODULE_COUNT;
  current = index;
  lastSwitch = millis();
  int pos = 0, total = 0;
  for (int i = 0; i < MODULE_COUNT; ++i)
    if (inRotation(i)) {
      if (i < current) pos++;
      total++;
    }
  Module *m = MODULES[current];
  m->subPage = sub;
  tft.fillScreen(ui::BG);
  ui::header(m->headerTitle().c_str(), pos, max(total, 1), paused);
  ui::connectionDot(connectedShown);
  m->fetchNow = true;
  m->enter();
  Serial.printf("page=%d.%d\n", current, sub);
}

// Naechste Unterseite des aktuellen Moduls, sonst naechstes Modul
void nextSlide() {
  Module *m = MODULES[current];
  if (m->subPage + 1 < m->pageCount()) showModule(current, m->subPage + 1);
  else showModule(current + 1);
}

void setPaused(bool p) {
  paused = p;
  lastSwitch = millis();
  ui::pauseIcon(paused);
}

// Hintergrund-Task (Kern 0): ruft die Daten der Module ab. Sichtbare Seite im eigenen Takt,
// die anderen hoechstens jede Minute, damit beim Umschalten nichts veraltet ist.
void fetchTask(void *) {
  static uint32_t last[MODULE_COUNT] = {0};
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      for (int i = 0; i < MODULE_COUNT; ++i) {
        Module *m = MODULES[i];
        uint32_t interval = m->fetchInterval();
        if (!interval || !inRotation(i)) continue;
        if (i != current) interval = max(interval, 60000u);
        if (m->fetchNow || !last[i] || millis() - last[i] >= interval) {
          m->fetchNow = false;
          last[i] = millis();
          m->fetch();
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void handleLine(String line) {
  line.trim();
  if (line == "ping") {
    Serial.println("pong");
    return;
  }
  if (line == "status") {
    Serial.println("status=" + stateJson());
    return;
  }
  int eq = line.indexOf('=');
  if (eq <= 0) return;
  String key = line.substring(0, eq);
  String value = line.substring(eq + 1);

  if (key == "cfg.theme") {
    prefs.putUChar("theme", value.toInt());
    ui::applyTheme(value.toInt() == 0);
    showModule(current);
  } else if (key == "cfg.invert") {
    prefs.putBool("uinv", value.toInt());
    tft.invertDisplay(!value.toInt());
  } else if (key == "cfg.orient") {
    tft.madctlOverride = 0;
    prefs.remove("madctl");
    prefs.putUChar("orient", value.toInt());
    tft.setRotation(value.toInt());
    showModule(current);
  } else if (key == "cfg.madctl") {
    tft.madctlOverride = strtol(value.c_str(), nullptr, 0);
    prefs.putUChar("madctl", tft.madctlOverride);
    tft.setRotation(tft.getRotation());
    showModule(current);
  } else if (key == "cfg.bright") {
    prefs.putUChar("bright", constrain(value.toInt(), 10, 255));
    board::setBacklight(constrain(value.toInt(), 10, 255));
  } else if (key == "cfg.cycle") {
    cycleSeconds = value.toInt();
    prefs.putUShort("cycle", cycleSeconds);
    lastSwitch = millis();
  } else if (key == "cfg.pause") {
    setPaused(value.toInt());
  } else if (key == "cfg.tcal") {
    // Touch-Kalibrierung "x0,x1,y0,y1,tausch": Rohwerte am linken/rechten bzw. oberen/unteren Rand
    prefs.putString("tcal", value);
  } else if (key == "cfg.pages") {
    pageMask = value.toInt() ? value.toInt() : 0xFFFF;
    prefs.putUShort("pages", pageMask);
    if (!pageEnabled(current)) showModule(current + 1);
  } else if (key == "cfg.tz") {
    prefs.putString("tz", value);
    net::applyTimezone();
    showModule(current);
  } else if (key == "page") {
    showModule(value.toInt());
    prefs.putUChar("page", current);
  } else if (key == "cfg.repo") {
    value.trim();
    if (value.isEmpty()) prefs.remove("repo");  // zurueck auf GITHUB_REPO
    else prefs.putString("repo", value);
  } else if (key.startsWith("mod.")) {
    // Moduleinstellung, z. B. mod.plex.url=http://...  (leerer Wert loescht)
    String pk = key.substring(4);
    if (pk.length() > 15) return;
    value.trim();
    if (value.isEmpty()) prefs.remove(pk.c_str());
    else prefs.putString(pk.c_str(), value);
    for (Module *m : MODULES) m->fetchNow = true;
    showModule(current);
  } else if (key == "wifi.ssid") {
    prefs.putString("ssid", value);
    net::scheduleRestart();
  } else if (key == "wifi.pass") {
    prefs.putString("pass", value);
  } else {
    lastRx = millis();
    if (key == "time") {
      struct timeval tv = {(time_t)value.toInt(), 0};
      settimeofday(&tv, nullptr);
    } else {
      data.set(key, value);
    }
  }
}

String stateJson() {
  String j = "{";
  auto add = [&](const char *k, const String &v) {
    if (j.length() > 1) j += ",";
    j += "\"" + String(k) + "\":" + v;
  };
  add("version", net::jsonStr(FIRMWARE_VERSION));
  add("theme", String(prefs.getUChar("theme", 0)));
  add("bright", String(prefs.getUChar("bright", 255)));
  add("orient", String(tft.getRotation()));
  add("invert", String(prefs.getBool("uinv", false) ? 1 : 0));
  add("cycle", String(cycleSeconds));
  add("pages", String(pageMask));
  add("paused", paused ? "true" : "false");
  add("touch", "{\"rawX\":" + String(lastTouchX) + ",\"rawY\":" + String(lastTouchY) + ",\"x\":" + String(lastTouchSX) +
                   ",\"y\":" + String(lastTouchSY) + "}");
  String mods = "[";
  for (int i = 0; i < MODULE_COUNT; ++i) mods += (i ? "," : "") + net::jsonStr(MODULES[i]->title());
  add("modules", mods + "]");
  String ready = "[";
  for (int i = 0; i < MODULE_COUNT; ++i) ready += String(i ? "," : "") + (MODULES[i]->ready() ? "true" : "false");
  add("ready", ready + "]");
  // Moduleinstellungen: Passwoerter werden nie ausgeliefert, nur ob sie gesetzt sind
  String cfg = "[";
  for (int i = 0; i < MODULE_COUNT; ++i) {
    int count;
    const Field *f = MODULES[i]->fields(count);
    if (!count) continue;
    if (cfg.length() > 1) cfg += ",";
    cfg += "{\"title\":" + net::jsonStr(MODULES[i]->title()) + ",\"status\":" + net::jsonStr(MODULES[i]->status()) + ",\"fields\":[";
    for (int k = 0; k < count; ++k) {
      String type = f[k].type;
      String stored = prefs.getString(type == "location" ? (String(f[k].key) + ".n").c_str() : f[k].key, "");
      cfg += String(k ? "," : "") + "{\"key\":" + net::jsonStr(f[k].key) + ",\"label\":" + net::jsonStr(f[k].label) +
             ",\"type\":" + net::jsonStr(type) + ",\"hint\":" + net::jsonStr(f[k].hint) + "," +
             (type == "password" ? "\"set\":" + String(stored.length() ? "true" : "false")
                                 : "\"value\":" + net::jsonStr(stored)) + "}";
    }
    cfg += "]}";
  }
  add("config", cfg + "]");
  add("tz", net::jsonStr(net::tz()));
  add("repo", net::jsonStr(ota::repo()));
  add("ssid", net::jsonStr(net::ssid()));
  add("wifi", net::jsonStr(net::online ? "ok" : net::ap ? "ap" : "connecting"));
  add("ip", net::jsonStr(net::online ? WiFi.localIP().toString() : net::ap ? WiFi.softAPIP().toString() : ""));
  add("rssi", String(net::online ? WiFi.RSSI() : 0));
  add("host", net::jsonStr(net::HOSTNAME));
  add("pcAgo", String(lastRx ? (long)((millis() - lastRx) / 1000) : -1L));
  add("uptime", String(millis() / 1000));
  return j + "}";
}

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handleLine(lineBuf);
      lineBuf = "";
    } else if (lineBuf.length() < 200) {
      lineBuf += c;
    }
  }
}

void readTouch() {
  static bool wasDown = false;
  static uint32_t lastTap = 0;
  uint16_t x, y;
  bool down = touch::read(x, y);
  if (down && !wasDown && millis() - lastTap > 300) {
    lastTap = millis();
    // Rohwerte -> Bildschirm; Kalibrierung "x0,x1,y0,y1,tausch" (tausch=1: Roh-X ist die Bildschirm-Y-Achse)
    int x0 = 300, x1 = 3800, y0 = 300, y1 = 3800, swap = 0;
    sscanf(prefs.getString("tcal", "").c_str(), "%d,%d,%d,%d,%d", &x0, &x1, &y0, &y1, &swap);
    int ax = swap ? y : x, ay = swap ? x : y;
    lastTouchX = x;
    lastTouchY = y;
    lastTouchSX = constrain(map(ax, x0, x1, 0, ui::W), 0, ui::W - 1);
    lastTouchSY = constrain(map(ay, y0, y1, 0, ui::H), 0, ui::H - 1);
    Serial.printf("touch=%u,%u -> %d,%d\n", x, y, lastTouchSX, lastTouchSY);
    if (lastTouchSY < ui::HEADER_H + 10 && abs(lastTouchSX - ui::pauseIconX) < 24) setPaused(!paused);
    else nextSlide();
  }
  wasDown = down;
}

void setup() {
  Serial.begin(115200);
  board::begin();
  ui::applyTheme(prefs.getUChar("theme", 0) == 0);
  cycleSeconds = prefs.getUShort("cycle", 10);
  pageMask = prefs.getUShort("pages", 0xFFFF);
  net::begin();
  ota::begin();
  dataMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(fetchTask, "fetch", 16384, nullptr, 1, nullptr, 0);
  // Diagnose: liefert die QR-Matrix fuer ?text=... als Zeilen aus 0/1
  net::server.on("/api/qr", HTTP_GET, [] {
    if (!qr::encode(net::server.arg("text").c_str())) return net::server.send(400, "text/plain", "zu lang");
    String out;
    for (int y = 0; y < qr::SIZE; ++y) {
      for (int x = 0; x < qr::SIZE; ++x) out += qr::modules[y][x] ? '1' : '0';
      out += '\n';
    }
    net::server.send(200, "text/plain", out);
  });
  showModule(prefs.getUChar("page", 0));
  Serial.println("hello=CYD-Monitor " FIRMWARE_VERSION);
}

void loop() {
  readSerial();
  readTouch();
  net::loop();

  if (ota::installRequested) {
    ota::install();  // startet bei Erfolg neu
    showModule(current);
  }

  // Im Einrichtungsmodus bleibt die Netzwerk-Seite mit der Anleitung stehen
  if (net::ap != apShown) {
    apShown = net::ap;
    if (apShown) showModule(NETWORK_PAGE);
  }

  bool connected = lastRx && millis() - lastRx < 70000;
  if (connected != connectedShown) {
    connectedShown = connected;
    ui::connectionDot(connected);
  }

  if (!net::ap && !paused && cycleSeconds && millis() - lastSwitch >= cycleSeconds * 1000UL) nextSlide();

  for (Module *m : MODULES) m->background();
  MODULES[current]->tick();
  delay(5);
}
