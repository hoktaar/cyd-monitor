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
#include "Module.h"
#include "ModuleClock.h"
#include "ModuleNetwork.h"

ClockModule clockModule;
NetworkModule networkModule;

Module *MODULES[] = {&clockModule, &networkModule};
constexpr int MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
constexpr int NETWORK_PAGE = 1;

int current = 0;
uint32_t lastRx = 0;
uint32_t lastSwitch = 0;
uint16_t cycleSeconds = 10;  // 0 = kein automatisches Weiterschalten
uint16_t pageMask = 0xFFFF;  // welche Seiten in der Rotation sind
bool connectedShown = false;
bool apShown = false;
String lineBuf;

bool pageEnabled(int i) { return (pageMask >> i) & 1; }

void showModule(int index) {
  index = ((index % MODULE_COUNT) + MODULE_COUNT) % MODULE_COUNT;
  for (int n = 0; n < MODULE_COUNT && !net::ap && !pageEnabled(index); ++n) index = (index + 1) % MODULE_COUNT;
  current = index;
  lastSwitch = millis();
  tft.fillScreen(ui::BG);
  ui::header(MODULES[current]->title(), current, MODULE_COUNT);
  ui::connectionDot(connectedShown);
  MODULES[current]->enter();
  Serial.printf("page=%d\n", current);
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
    prefs.putBool("inv", value.toInt());
    tft.invertDisplay(value.toInt());
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
  add("invert", String(prefs.getBool("inv", true) ? 1 : 0));
  add("cycle", String(cycleSeconds));
  add("pages", String(pageMask));
  String mods = "[";
  for (int i = 0; i < MODULE_COUNT; ++i) mods += (i ? "," : "") + net::jsonStr(MODULES[i]->title());
  add("modules", mods + "]");
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
    Serial.printf("touch=%u,%u\n", x, y);
    showModule(current + 1);
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

  if (!net::ap && cycleSeconds && millis() - lastSwitch >= cycleSeconds * 1000UL) showModule(current + 1);

  for (Module *m : MODULES) m->background();
  MODULES[current]->tick();
  delay(5);
}
