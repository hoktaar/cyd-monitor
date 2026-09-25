// Basisklasse fuer eine Anzeige. Neue Anzeige = neue Klasse + Eintrag in MODULES (CYD_Monitor.ino).
#pragma once

// Einstellungsfeld fuer die Web-Oberflaeche; key ist zugleich der Preferences-Schluessel (max. 15 Zeichen).
// type: "text", "password" (wird nie ausgeliefert), "number" oder "location" (Ortssuche, setzt key.n/.la/.lo)
struct Field {
  const char *key;
  const char *label;
  const char *type;
  const char *hint;
};

class Module {
 public:
  virtual ~Module() {}
  virtual const char *title() = 0;
  // Wird beim Umschalten auf diese Seite aufgerufen; der Inhaltsbereich ist bereits leer.
  virtual void enter() = 0;
  // Wird laufend aufgerufen, solange die Seite sichtbar ist (nur hier auf das Display zeichnen).
  virtual void tick() = 0;
  // Wird immer aufgerufen, auch wenn die Seite nicht sichtbar ist (z. B. fuer die LED).
  virtual void background() {}

  // Unterseiten (z. B. ein Slide pro Plex-Stream): die Rotation zeigt alle nacheinander.
  virtual int pageCount() { return 1; }
  int subPage = 0;  // wird vor enter() gesetzt
  virtual String headerTitle() { return title(); }

  // Einstellungen fuer die Web-Oberflaeche
  virtual const Field *fields(int &count) {
    count = 0;
    return nullptr;
  }
  // false = noch nicht eingerichtet, wird in der Rotation uebersprungen
  virtual bool ready() { return true; }
  // Kurzer Status fuer die Web-Oberflaeche (z. B. letzter Fehler)
  virtual String status() { return ""; }

  // Datenabruf im Hintergrund-Task (Kern 0): hier nicht zeichnen, Ergebnisse unter DataLock ablegen.
  virtual uint32_t fetchInterval() { return 0; }  // ms, 0 = kein Abruf
  virtual void fetch() {}
  volatile bool fetchNow = true;  // naechsten Abruf sofort ausfuehren (z. B. nach Aenderung der Einstellungen)
};
