// Gemeinsames fuer Module, die Daten aus dem Netz holen: Sperre zwischen Hintergrund-Task und Display,
// HTTP-Anfragen (http oder https im LAN) und kleine Helfer.
#pragma once
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

SemaphoreHandle_t dataMutex = nullptr;

// Schuetzt Moduldaten, die der Hintergrund-Task schreibt und loop() liest.
struct DataLock {
  DataLock() { xSemaphoreTake(dataMutex, portMAX_DELAY); }
  ~DataLock() { xSemaphoreGive(dataMutex); }
};

namespace http {

// Eine Anfrage; https-Ziele im Heimnetz haben meist selbstsignierte Zertifikate -> ohne Pruefung.
struct Request {
  WiFiClient plain;
  WiFiClientSecure secure;
  HTTPClient client;

  bool begin(const String &url) {
    client.setUserAgent("CYD-Monitor");
    client.setTimeout(8000);
    client.setConnectTimeout(5000);
    client.useHTTP10(true);  // kein Chunked-Encoding -> Antwort direkt als Stream lesbar
    if (url.startsWith("https://")) {
      secure.setInsecure();
      return client.begin(secure, url);
    }
    return client.begin(plain, url);
  }
};

String describe(int code) { return code > 0 ? "HTTP " + String(code) : HTTPClient::errorToString(code); }

String urlEncode(const String &s) {
  static const char *HEX_DIGITS = "0123456789ABCDEF";
  String out;
  for (uint8_t c : s) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else {
      out += '%';
      out += HEX_DIGITS[c >> 4];
      out += HEX_DIGITS[c & 15];
    }
  }
  return out;
}

// "http://host:port/" -> ohne abschliessenden Schraegstrich
String baseUrl(String url) {
  url.trim();
  while (url.endsWith("/")) url.remove(url.length() - 1);
  return url;
}

}  // namespace http

// Zahl aus JSON, auch wenn sie als String kommt (GraphQL-BigInt)
double jsonNumber(JsonVariantConst v) { return v.is<const char *>() ? atof(v.as<const char *>()) : v.as<double>(); }

String nowHHMM() {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  return buf;
}
