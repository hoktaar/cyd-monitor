# Roadmap & Übergabe

Stand: 25.09.2026, Firmware **v1.6**. Diese Datei ist die Übergabe für die Weiterarbeit, auch für einen anderen Coding-Agenten.
Zuerst `README.md` lesen (Funktionen, Bauen, Schnittstellen), dann diese Datei.

## Aktueller Stand

| Bereich | Status |
|---|---|
| Uhr, Netzwerk (QR-Code), Web-Oberfläche, WLAN-Hotspot-Einrichtung | ✅ läuft |
| OTA-Update aus GitHub-Releases (HTTPS mit Zertifikatsprüfung) | ✅ getestet |
| Wetter (Open-Meteo, animiert, Ortssuche) | ✅ getestet |
| Unraid (GraphQL-API, CPU/RAM/Array/Cache) | ✅ läuft mit echtem Server |
| Plex (laufende Streams, ein Slide pro Stream, sonst zufälliger neuer Titel) | ✅ Streams werden erkannt. Layout v1.6 ist noch **nicht auf dem Display geprüft** |
| GPU (nvidia_gpu_exporter) | ⚠️ ungetestet, beim Nutzer noch nicht eingerichtet |
| Touch / Play-Pause-Symbol | ⚠️ ungetestet, Kalibrierung fehlt (siehe unten) |

**Das Board hat noch v1.5-Code** (lokaler Build mit Version „1.5.1“). v1.6 liegt als GitHub-Release bereit.
Sobald das Board wieder läuft: Web-Oberfläche → *Firmware-Update* → „Nach Updates suchen“ → „Installieren“.

## Offene Aufgaben (priorisiert)

1. **v1.6 installieren und auf dem Display prüfen:** Farben (Plex-Cover dürfen kein Negativ sein), Plex-Layout
   (Serie / „Staffel X · Folge Y“ / umbrochener Folgentitel), „Plex · 1/2“ bei mehreren Streams. Am besten mit Foto vom Nutzer.
2. **Touch kalibrieren:** Unklar, ob Touch überhaupt reagiert. Beim Mitschnitt kamen keine `touch=`-Zeilen über Seriell.
   - Die Web-Oberfläche zeigt unter *Slides* „Letzter Touch: x, y (roh …)“, und `/api/state` liefert `touch`.
   - Den Nutzer oben links und unten rechts tippen lassen, dann `cfg.tcal=x0,x1,y0,y1,tausch` setzen (Rohwerte, `tausch=1`
     wenn Roh-X die Bildschirm-Y-Achse ist). Danach die Standardwerte in `readTouch()` (`CYD_Monitor.ino`) fest eintragen.
   - Kommt gar nichts an: Bit-Banging in `Board.h` (`touch::read`, Pins 25/32/39/33/36) prüfen, eventuell IRQ-Pin ignorieren
     und nur über den Druckwert (z1/z2) erkennen.
3. **Plex-Zusatzinfos beim Abspielen:** Der Nutzer hat noch nicht gewählt. Vorschlag: „Endet um HH:MM“,
   Badges für Qualität (4K/1080p, HDR) und Direct Play / Transcode, Ton-Format. Die Daten stehen in `/status/sessions`
   unter `Media[0]` (`videoResolution`, `audioCodec`, `audioChannels`), `TranscodeSession` und `Session`. Filter in `ModulePlex.h` erweitern.
4. **GPU-Modul testen,** sobald der Container `nvidia_gpu_exporter` läuft. Metriknamen stehen in `ModuleGpu.h`.
5. **PIN-Schutz für die Web-Oberfläche** (empfohlen, Vereins-WLAN): Aktuell kann jeder im Netz Einstellungen ändern
   und Firmware hochladen (`/api/update`, `/api/settings`).
6. **GitHub-Pflege** (braucht den Nutzer-Login, geht nicht per Git):
   - About-Beschreibung: *Modularer Info-Bildschirm für das ESP32 Cheap Yellow Display (ESP32-2432S028): Uhr-, Wetter-, Plex- und Unraid-Slides, Web-Oberfläche, WLAN-Einrichtung per QR-Code und OTA-Updates aus GitHub-Releases.*
   - Topics: `esp32`, `cheap-yellow-display`, `esp32-2432s028`, `arduino`, `ili9341`, `ota-update`, `web-ui`, `captive-portal`, `qr-code`, `dashboard`, `iot`, `plex`, `unraid`
   - Den übrig gebliebenen Release-Entwurf `v1.5.1` löschen (Tag ist schon entfernt, enthielt alten Code).
7. **Ideen für später:** Home-Assistant-Modul, Auto-Dimmen über den Lichtsensor (GPIO 34), Nachtmodus,
   Modul-Baukasten (Anzeigen als JSON-Beschreibung ohne neue Firmware).

## Hardware-Eigenheiten (hart erarbeitet, nicht ändern ohne Test)

Board: ESP32-2432S028, Variante mit **USB-C + Micro-USB**, USB-Chip CH340C.

- **Panel ist nativ quer eingebaut:** Querformat = MADCTL **ohne** MV. `0x80` = USB rechts (Standard), `0x40` = USB links.
- **Farbreihenfolge RGB, nicht BGR** (kein 0x08-Bit). Mit BGR waren Rot und Blau vertauscht.
- **Das Panel braucht intern INVON** für normale Farben. In der Firmware heißt die Einstellung `uinv`
  („Nutzer will Negativ“): `invertDisplay(!uinv)`.
- Gamma-Korrektur (`GAMMASET 2 → 1`) in `Board.h`, sonst blasse Farben.
- Treiber: `Adafruit_ILI9341` mit Unterklasse `CydDisplay` (eigenes `setRotation`).
- Touch: XPT2046 per Bit-Banging (CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36). RGB-LED 4/16/17 (active low), Backlight 21.
- Das USB-C-Kabel muss ein Datenkabel sein. Mit einem reinen Ladekabel meldet Windows „Unbekanntes USB-Gerät“.

## Architektur in Kürze

- `CYD_Monitor.ino`: Modulliste `MODULES`, Rotation (`showModule`/`nextSlide`, Unterseiten per `pageCount()`),
  Hintergrund-Task `fetchTask` (Kern 0), `handleLine()` für alle `cfg.*`/`mod.*`/Daten-Zeilen, `stateJson()`.
- `Module.h`: Schnittstelle. `fields()` für Web-Einstellungen, `ready()`, `fetch()` im Hintergrund, `tick()` zeichnet.
  Moduldaten immer unter `DataLock` (`Fetch.h`) teilen, **nur in `tick()`/`enter()` zeichnen**.
- `Ui.h`: Theme-Farben, `textBox()` (flackerfrei, Umlaute/°/· per Hand gezeichnet), `wrap()`, `bar()`, `header()`.
- `Net.h`: WLAN, Hotspot + Captive Portal, mDNS, NTP, Webserver, Upload-OTA. `Ota.h`: GitHub-Releases. `WebPage.h`: Web-Oberfläche.
- `GithubCerts.h`: Stammzertifikate (USERTrust ECC, Sectigo E46, ISRG X1, USERTrust RSA).
  Wenn GitHub die CA wechselt, scheitert das OTA-Update. Der manuelle Upload funktioniert dann weiterhin.

## Bauen, Flashen, Veröffentlichen

- ESP32-Core **2.0.2**, FQBN `esp32:esp32:esp32:PartitionScheme=min_spiffs` (1,9 MB App, zwei OTA-Slots).
- Bibliotheken: Adafruit ILI9341 1.5.10, Adafruit GFX 1.10.14, Adafruit BusIO 1.11.3, ArduinoJson **6.19.3** (Version 6, nicht 7).
- Per WLAN flashen: `curl -F "firmware=@build/CYD_Monitor.ino.bin" http://<IP>/api/update`.
  Die zuletzt bekannte IP war `192.168.181.173`. `cyd-monitor.local` löst auf dem Windows-PC des Nutzers **nicht** auf.
- Release: `version.h` wird in CI aus dem Tag gesetzt. `git tag vX.Y && git push origin vX.Y`, GitHub Actions baut dann
  `CYD_Monitor.bin` ins Release. Danach kann das Board über die Web-Oberfläche updaten.
- Seriell (115200): `ping`, `status` (liefert State-JSON) und dieselben `schluessel=wert`-Zeilen wie per HTTP.

## Stolperfallen

- **`bit` ist ein Arduino-Makro.** Keine Funktionen oder Lambdas so nennen (hat den QR-Code kaputt gemacht).
- Textbreite nie auf kleinen Canvases mit aktivem Text-Wrap messen (`setTextWrap(false)`), sonst stimmt der Umbruch nicht.
- Adafruit-GFX-Schriften sind reines ASCII. Umlaute, `°` und `·` zeichnet `ui::printDE`. Neue Sonderzeichen dort ergänzen.
- Preferences-Schlüssel höchstens **15 Zeichen**. Passwörter und Tokens (`plex.token`, `ur.key`) nie in `stateJson` ausgeben.
- Unter Windows PowerShell 5.1 Commit-Nachrichten mit Anführungszeichen über eine Datei übergeben
  (`git commit -F .git/MSG`). Inline gehen sie kaputt, und ein direkt folgendes `git tag` markiert dann den falschen Commit.
- Beim Testen `cfg.cycle` nicht auf 0 lassen (wird gespeichert). Standard ist 10 Sekunden.
