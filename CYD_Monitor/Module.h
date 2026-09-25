// Basisklasse fuer eine Anzeige. Neue Anzeige = neue Klasse + Eintrag in MODULES (CYD_Monitor.ino).
#pragma once

class Module {
 public:
  virtual ~Module() {}
  virtual const char *title() = 0;
  // Wird beim Umschalten auf diese Seite aufgerufen; der Inhaltsbereich ist bereits leer.
  virtual void enter() = 0;
  // Wird laufend aufgerufen, solange die Seite sichtbar ist.
  virtual void tick() = 0;
  // Wird immer aufgerufen, auch wenn die Seite nicht sichtbar ist (z. B. fuer die LED).
  virtual void background() {}
};
