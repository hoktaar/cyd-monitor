# CYD Monitor

Modularer Info-Bildschirm für das **ESP32-2432S028** („Cheap Yellow Display“, 2,8" Touch, 320×240).
Die Anzeigen wechseln automatisch als Slides. Eingestellt wird alles über eine Web-Oberfläche.

## Funktionen

- **Slides:**
  - **Uhr:** Siebensegment, Datum auf Deutsch, Zeit per NTP
  - **Wetter:** animierte Symbole, 3-Tage-Vorhersage (Open-Meteo, ohne API-Key), Ort per Suche in der Web-Oberfläche
  - **Plex:** Was gerade läuft, mit Cover und Fortschritt. Sonst ein zufälliger neu hinzugefügter Film bzw. eine neue Folge
  - **Unraid:** CPU, RAM, freier Platz auf Array und Cache (offizielle Unraid-API ab Unraid 7)
  - **GPU:** Nvidia VRAM, Last, Temperatur, Leistung (über den Container `nvidia_gpu_exporter`)
  - **Netzwerk:** Status + QR-Code zur Web-Oberfläche
- Nicht eingerichtete Module werden in der Rotation übersprungen.
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

- ESP32-Core **2.0.2** („ESP32 Dev Module“), Partitionsschema **Minimal SPIFFS** (1,9 MB App mit OTA)
- Bibliotheken: Adafruit ILI9341 1.5.10, Adafruit GFX Library 1.10.14, Adafruit BusIO 1.11.3, ArduinoJson 6.19.3

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs --output-dir build CYD_Monitor
arduino-cli upload  --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs -p COM3 --input-dir build CYD_Monitor
```

Ein Board, das noch mit dem Standard-Partitionsschema läuft, muss einmal per USB geflasht werden.

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
3. Einstellungen: `fields()` liefert die Felder für die Web-Oberfläche (gespeichert in Preferences, Schlüssel ≤ 15 Zeichen).
   `ready()` gibt an, ob das Modul eingerichtet ist.
4. Daten aus dem Netz: `fetchInterval()` und `fetch()` laufen in einem Hintergrund-Task. Ergebnisse unter `DataLock`
   ablegen und nur in `tick()` zeichnen (siehe `ModuleUnraid.h`).
5. Daten vom PC: Ein Provider in `host/cyd-host.ps1` liefert `schluessel=wert`, das Modul liest `data.get("schluessel")`.

## Einrichtung der Datenquellen

- **Plex:** Server-Adresse (z. B. `http://192.168.1.10:32400`) und
  [X-Plex-Token](https://support.plex.tv/articles/204059436-finding-an-authentication-token-x-plex-token/).
- **Unraid:** *Settings → Management Access → API Keys* einen Key anlegen (Rolle *viewer* genügt),
  dazu die Server-Adresse (z. B. `http://192.168.1.10`).
- **GPU:** Aus den Community Apps den Container `nvidia_gpu_exporter` installieren (Nvidia-Treiber-Plugin nötig).
  Adresse z. B. `http://192.168.1.10:9835`.

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
