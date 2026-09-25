// Firmware-Updates aus GitHub-Releases. Das neueste Release des Repos (cfg.repo, "benutzer/repo")
// muss die Datei CYD_Monitor.bin enthalten - das erledigt der GitHub-Actions-Workflow bei jedem Tag v*.
#pragma once
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include "GithubCerts.h"

#ifndef GITHUB_REPO
#define GITHUB_REPO "hoktaar/cyd-monitor"
#endif

namespace ota {

const char *ASSET_NAME = "CYD_Monitor.bin";

String latestTag, assetUrl, lastError;
bool installRequested = false;

String repo() { return prefs.getString("repo", GITHUB_REPO); }

String normalize(String v) {
  v.trim();
  if (v.startsWith("v") || v.startsWith("V")) v.remove(0, 1);
  return v;
}

bool updateAvailable() { return latestTag.length() && assetUrl.length() && normalize(latestTag) != normalize(FIRMWARE_VERSION); }

// Liest den String-Wert von "key" ab Position from (GitHub-JSON hat keine Leerzeichen nach dem Doppelpunkt).
String jsonField(const String &json, const char *key, int from = 0) {
  String needle = "\"" + String(key) + "\":\"";
  int start = json.indexOf(needle, from);
  if (start < 0) return "";
  start += needle.length();
  String out;
  for (int i = start; i < (int)json.length() && json[i] != '"'; ++i) {
    if (json[i] == '\\' && i + 1 < (int)json.length()) ++i;
    out += json[i];
  }
  return out;
}

bool check() {
  latestTag = assetUrl = lastError = "";
  if (!repo().length()) {
    lastError = "Kein GitHub-Repository eingestellt";
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    lastError = "Kein WLAN";
    return false;
  }

  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CAS);
  HTTPClient http;
  http.begin(client, "https://api.github.com/repos/" + repo() + "/releases/latest");
  http.setUserAgent("CYD-Monitor");
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code != 200) {
    lastError = code == 404 ? "Kein Release gefunden" : "GitHub: " + (code > 0 ? "HTTP " + String(code) : http.errorToString(code));
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  latestTag = jsonField(body, "tag_name");
  int asset = body.indexOf("\"name\":\"" + String(ASSET_NAME) + "\"");
  if (asset >= 0) assetUrl = jsonField(body, "browser_download_url", asset);
  if (!assetUrl.length()) lastError = String("Release enthält keine ") + ASSET_NAME;
  return lastError.isEmpty();
}

void drawProgress(size_t done, size_t total) {
  static int last = -1;
  int pct = total ? done * 100 / total : 0;
  if (pct == last) return;
  last = pct;
  ui::bar(20, 130, 280, 16, pct, ui::ACCENT);
}

// Laeuft blockierend (aus loop() heraus); startet bei Erfolg neu.
void install() {
  installRequested = false;
  if (!updateAvailable() && !check()) return;

  tft.fillScreen(ui::BG);
  ui::textBox(0, 70, ui::W, 34, 26, "Update wird installiert", &FreeSansBold12pt7b, ui::TEXT, ui::BG, ui::CENTER);
  String info = normalize(FIRMWARE_VERSION) + "  ->  " + normalize(latestTag);
  ui::textBox(0, 100, ui::W, 24, 17, info.c_str(), &FreeSans9pt7b, ui::MUTED, ui::BG, ui::CENTER);
  ui::bar(20, 130, 280, 16, 0, ui::ACCENT);
  Serial.printf("ota=%s\n", assetUrl.c_str());

  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CAS);
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("CYD-Monitor");
  http.begin(client, assetUrl);
  int code = http.GET();
  int len = http.getSize();
  bool ok = false;
  if (code != 200 || len <= 0) {
    lastError = "Download: " + (code > 0 ? "HTTP " + String(code) : http.errorToString(code));
  } else if (!Update.begin(len)) {
    lastError = String("Update: ") + Update.errorString();
  } else {
    Update.onProgress(drawProgress);
    size_t written = Update.writeStream(*http.getStreamPtr());
    ok = written == (size_t)len && Update.end(true);
    if (!ok) {
      lastError = String("Update: ") + Update.errorString();
      Update.abort();
    }
  }
  http.end();

  if (ok) {
    ui::textBox(0, 160, ui::W, 24, 17, "Fertig - Neustart ...", &FreeSans9pt7b, ui::OK, ui::BG, ui::CENTER);
    delay(800);
    ESP.restart();
  }
  Serial.printf("ota_error=%s\n", lastError.c_str());
  ui::textBox(0, 160, ui::W, 24, 17, lastError.c_str(), &FreeSans9pt7b, ui::BAD, ui::BG, ui::CENTER);
  delay(4000);
}

String stateJson() {
  return "{\"current\":" + net::jsonStr(FIRMWARE_VERSION) + ",\"repo\":" + net::jsonStr(repo()) +
         ",\"latest\":" + net::jsonStr(latestTag) + ",\"available\":" + (updateAvailable() ? "true" : "false") +
         ",\"error\":" + net::jsonStr(lastError) + "}";
}

void begin() {
  net::server.on("/api/ota/check", HTTP_GET, [] {
    check();
    net::server.send(200, "application/json", stateJson());
  });
  net::server.on("/api/ota/install", HTTP_POST, [] {
    installRequested = true;
    net::server.send(200, "application/json", stateJson());
  });
}

}  // namespace ota
