// Schluessel/Wert-Speicher fuer alles, was der PC schickt ("claude.fh=3", "time=...").
// Jede Aenderung bekommt eine fortlaufende Revision, damit Module nur bei neuen Daten neu zeichnen.
#pragma once
#include <map>

class DataStore {
 public:
  void set(const String &key, const String &value) {
    Entry &e = entries_[key];
    if (e.rev && e.value == value) return;
    e.value = value;
    e.rev = ++rev_;
  }

  bool has(const String &key) const { return entries_.count(key) > 0; }

  String get(const String &key, const String &fallback = "") const {
    auto it = entries_.find(key);
    return it == entries_.end() ? fallback : it->second.value;
  }

  long getInt(const String &key, long fallback = 0) const {
    auto it = entries_.find(key);
    return it == entries_.end() ? fallback : it->second.value.toInt();
  }

  // Hoechste Revision aller Schluessel mit diesem Praefix (0 = noch nie gesetzt).
  uint32_t rev(const String &prefix) const {
    uint32_t r = 0;
    for (const auto &kv : entries_)
      if (kv.first.startsWith(prefix) && kv.second.rev > r) r = kv.second.rev;
    return r;
  }

 private:
  struct Entry {
    String value;
    uint32_t rev = 0;
  };
  std::map<String, Entry> entries_;
  uint32_t rev_ = 0;
};

DataStore data;
