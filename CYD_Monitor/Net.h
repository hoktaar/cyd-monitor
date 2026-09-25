// WLAN, Einrichtungs-Hotspot (Captive Portal), mDNS (cyd-monitor.local), NTP, Webserver und Firmware-Update.
// Sobald keine WLAN-Verbindung besteht (ohne gespeichertes WLAN sofort, sonst nach 15 s), oeffnet das Board
// den Hotspot "CYD-Monitor"; die Web-Oberflaeche ist dann unter 192.168.4.1 erreichbar.
#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "WebPage.h"

// in CYD_Monitor.ino
void handleLine(String line);
String stateJson();

namespace net {

const char *HOSTNAME = "cyd-monitor";
const char *AP_SSID = "CYD-Monitor";
const char *DEFAULT_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";  // Deutschland

WebServer server(80);
DNSServer dns;
bool ap = false;
bool online = false;
uint32_t offlineSince = 0;
uint32_t lastRetry = 0;
uint32_t restartAt = 0;
uint32_t bootId = 0;
uint32_t rev = 1;  // steigt bei jeder Statusaenderung (fuer die Netzwerk-Seite)

String ssid() { return prefs.getString("ssid", ""); }
String tz() { return prefs.getString("tz", DEFAULT_TZ); }

String jsonStr(const String &s) {
  String out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out + "\"";
}

void startAp() {
  if (ap) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);
  dns.start(53, "*", WiFi.softAPIP());
  ap = true;
  rev++;
  Serial.printf("wifi=ap %s\n", WiFi.softAPIP().toString().c_str());
}

void stopAp() {
  if (!ap) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  ap = false;
  rev++;
}

void applyTimezone() {
  setenv("TZ", tz().c_str(), 1);
  tzset();
}

// Neu verbinden mit den gespeicherten Zugangsdaten (nach Aenderung ueber die Web-Oberflaeche).
void scheduleRestart() { restartAt = millis() + 1500; }

void handleData() {
  String body = server.arg("plain");
  int start = 0;
  while (start < (int)body.length()) {
    int nl = body.indexOf('\n', start);
    if (nl < 0) nl = body.length();
    handleLine(body.substring(start, nl));
    start = nl + 1;
  }
  server.send(200, "text/plain", "boot=" + String(bootId));
}

void handleSettings() {
  for (int i = 0; i < server.args(); ++i) {
    String key = server.argName(i);
    if (key == "plain") continue;
    if (key == "wifi.pass" && server.arg(i).isEmpty() && !server.hasArg("wifi.ssid")) continue;
    handleLine(key + "=" + server.arg(i));
  }
  server.send(200, "application/json", stateJson());
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i) json += ",";
    json += "{\"ssid\":" + jsonStr(WiFi.SSID(i)) + ",\"rssi\":" + String(WiFi.RSSI(i)) +
            ",\"open\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  WiFi.scanDelete();
  server.send(200, "application/json", json + "]");
}

void begin() {
  bootId = esp_random() & 0x7FFFFFFF;
  applyTimezone();

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  String s = ssid();
  if (s.length()) WiFi.begin(s.c_str(), prefs.getString("pass", "").c_str());
  else startAp();
  offlineSince = lastRetry = millis();

  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/api/state", HTTP_GET, [] { server.send(200, "application/json", stateJson()); });
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/data", HTTP_POST, handleData);
  server.on(
      "/api/update", HTTP_POST,
      [] {
        bool ok = !Update.hasError();
        server.send(ok ? 200 : 500, "text/plain", ok ? "ok" : Update.errorString());
        if (ok) scheduleRestart();
      },
      [] {
        HTTPUpload &up = server.upload();
        if (up.status == UPLOAD_FILE_START) {
          Serial.printf("update=%s\n", up.filename.c_str());
          Update.begin(UPDATE_SIZE_UNKNOWN);
        } else if (up.status == UPLOAD_FILE_WRITE) {
          Update.write(up.buf, up.currentSize);
        } else if (up.status == UPLOAD_FILE_END) {
          Update.end(true);
        } else if (up.status == UPLOAD_FILE_ABORTED) {
          Update.abort();
        }
      });
  server.on("/api/restart", HTTP_POST, [] {
    server.send(200, "text/plain", "ok");
    scheduleRestart();
  });
  server.onNotFound([] {
    if (ap) {
      server.sendHeader("Location", "http://192.168.4.1/");
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "nicht gefunden");
    }
  });
  server.begin();
}

void loop() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != online) {
    online = connected;
    rev++;
    if (connected) {
      MDNS.begin(HOSTNAME);
      MDNS.addService("http", "tcp", 80);
      configTzTime(tz().c_str(), "pool.ntp.org", "time.google.com");
      Serial.printf("wifi=%s\n", WiFi.localIP().toString().c_str());
    }
  }
  // Hotspot immer dann an, wenn kein WLAN besteht; aus, sobald verbunden und niemand mehr am Hotspot haengt
  if (connected) offlineSince = 0;
  else if (!offlineSince) offlineSince = millis();
  if (!connected && !ap && millis() - offlineSince > 15000) startAp();
  if (connected && ap && WiFi.softAPgetStationNum() == 0) stopAp();

  // Heimnetz regelmaessig neu versuchen - aber nicht, waehrend jemand am Hotspot haengt (Kanalwechsel stoert)
  if (!connected && ssid().length() && millis() - lastRetry > 30000 &&
      (!ap || WiFi.softAPgetStationNum() == 0)) {
    lastRetry = millis();
    WiFi.begin(ssid().c_str(), prefs.getString("pass", "").c_str());
  }

  if (ap) dns.processNextRequest();
  server.handleClient();
  if (restartAt && millis() > restartAt) ESP.restart();
}

}  // namespace net
