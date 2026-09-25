# CYD Monitor

Modularer Info-Bildschirm für das **ESP32-2432S028** („Cheap Yellow Display“, 2,8" Touch, 320×240).
Die Anzeigen wechseln automatisch als Slides. Eingestellt wird alles über eine Web-Oberfläche.

## Funktionen

- **Slides:** Uhr (Siebensegment, Datum auf Deutsch, Zeit per NTP) und Netzwerk (Status + QR-Code zur Web-Oberfläche)
- **Web-Oberfläche:** Design hell/dunkel, Helligkeit, Ausrichtung, Slide-Dauer, sichtbare Seiten, Zeitzone, WLAN
- **WLAN-Einrichtung:** Ohne WLAN-Verbindung öffnet das Board den Hotspot `CYD-Monitor`. QR-Code scannen oder
  mit dem Hotspot verbinden, dann `192.168.4.1` öffnen und das Heimnetz eintragen.
- **OTA-Updates von GitHub:** In der Web-Oberfläche „Nach Updates suchen“ → „Installieren“.
  Alternativ lässt sich eine `.bin`-Datei manuell hochladen.
- **Daten vom PC** (optional): `host/cyd-host.ps1` schickt Werte per USB oder WLAN (`schluessel=wert`).

## Hardware

ESP32-2432S028, Variante mit USB-C + Micro-USB (CH340C). Das Panel dieser Variante ist nativ quer eingebaut,
hat invertierte Farben und braucht eine Gamma-Korrektur. Das ist in `Board.h` berücksichtigt.
Wenn deine Variante falsch herum oder gespiegelt anzeigt, lässt sich das mit `cfg.madctl` bzw. `cfg.invert` anpassen.

## Bauen

Arduino IDE oder `arduino-cli` mit:

- ESP32-Core **2.0.2** (`esp32:esp32:esp32`, „ESP32 Dev Module“)
- Bibliotheken: Adafruit ILI9341 1.5.10, Adafruit GFX Library 1.10.14, Adafruit BusIO 1.11.3

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir build CYD_Monitor
arduino-cli upload  --fqbn esp32:esp32:esp32 -p COM3 --input-dir build CYD_Monitor
```

Per WLAN flashen, sobald die Firmware einmal drauf ist:

```bash
curl -F "firmware=@build/CYD_Monitor.ino.bin" http://<IP-des-Boards>/api/update
```

## Release und OTA-Update

GitHub Actions baut die Firmware bei jedem Push. Bei einem Versions-Tag entsteht ein Release mit `CYD_Monitor.bin`:

```bash
git tag v1.4
git push origin v1.4
```

Danach in der Web-Oberfläche unter *Firmware-Update* auf „Nach Updates suchen“ tippen.
Das Board lädt das Release über HTTPS mit Zertifikatsprüfung herunter (`GithubCerts.h`).

## Eigene Module

1. Neue Klasse von `Module` ableiten (`title()`, `enter()`, `tick()`), siehe `ModuleClock.h`.
2. In `CYD_Monitor.ino` zur Liste `MODULES` hinzufügen.
3. Braucht das Modul Daten vom PC, liefert ein Provider in `host/cyd-host.ps1` sie als `schluessel=wert`.
   Das Modul liest sie über `data.get("schluessel")`.

## Schnittstellen

| Pfad | Zweck |
|---|---|
| `GET /` | Web-Oberfläche |
| `GET /api/state` | Einstellungen und Status (JSON) |
| `POST /api/settings` | Einstellungen setzen (`cfg.theme=0`, `cfg.cycle=10`, …) |
| `POST /api/data` | Daten setzen (Zeilen `schluessel=wert`) |
| `GET /api/ota/check`, `POST /api/ota/install` | Update von GitHub |
| `POST /api/update` | Firmware-Datei hochladen |

Über USB-Seriell (115200 Baud) gelten dieselben Zeilen, dazu `ping` und `status`.
