# software-arm-lib
Repository for the ARM library.

## Catch.hpp Unit Test of the sblib
for unit testing the slib following projects must be in the workspace and opened:
-[lib-test-cases](test/lib-test-cases)
-[sblib-test](test/sblib)
-[sblib](sblib)
-[Catch](Catch)

Please do not push directly to this repository. If you want to make changes, then make a private fork of this repository, do your changes there, and make a pull request.


# Selfbus RP2354 Firmware Port

Diese Dokumentation beschreibt den bisherigen Stand der Migration der Selfbus-Firmware auf die RP2354-Plattform mit Pico SDK, CMake, CLion und SBDAP/OpenOCD.

Sie ist so geschrieben, dass sie direkt als `README.md` oder als Datei unter `docs/` in GitHub verwendet werden kann.

---

## Ziel der Migration

Die ursprüngliche Selfbus-Firmware basiert auf:

- NXP LPC1115
- MCUXpresso

Die neue Zielplattform ist:

- Raspberry Pi RP2354B
- Raspberry Pi Pico SDK
- CMake + Ninja
- CLion
- OpenOCD + CMSIS-DAP (SBDAP)

Ziel ist eine moderne, portable und sauber strukturierte Firmware-Basis, auf der zukünftige Selfbus-Geräte leichter weiterentwickelt werden können.

---

## Aktueller Stand

Bisher wurden folgende Bereiche erfolgreich aufgebaut und getestet:

- CMake-Projekt mit Pico SDK
- Build in CLion
- Flashen auf Pico 2 als Testplattform
- USB-CDC-Ausgabe
- UART0-Abstraktion
- Debugging mit SBDAP + OpenOCD + GDB + CLion
- Hardware-Abstraktionsschicht für RP2354
- Debounced Inputs
- Button-/Taster-Events mit `Press`, `Release`, `Short`, `Long`, `Double`

Die Zieloptimierung erfolgt nicht für das Pico-2-Board als Produktivplattform, sondern für den **RP2354B**. Das Pico 2 dient aktuell als Bring-up- und Testboard.

---

## Projektstruktur

```text
.
├─ CMakeLists.txt
├─ examples/
│  └─ bringup_blinky_uart/
│     └─ main.cpp
├─ openocd/
│  └─ sbdap_pico2.cfg
├─ platform/
│  └─ rp2354/
│     ├─ include/
│     │  ├─ sb_button.h
│     │  ├─ sb_cpu.h
│     │  ├─ sb_flash.h
│     │  ├─ sb_gpio.h
│     │  ├─ sb_input.h
│     │  ├─ sb_irq.h
│     │  ├─ sb_time.h
│     │  └─ sb_uart.h
│     └─ src/
│        ├─ sb_button.cpp
│        ├─ sb_cpu.cpp
│        ├─ sb_flash.cpp
│        ├─ sb_gpio.cpp
│        ├─ sb_input.cpp
│        ├─ sb_irq.cpp
│        ├─ sb_time.cpp
│        └─ sb_uart.cpp
└─ tools/
   ├─ start_gdb_sbdap_pico2.bat
   └─ start_openocd_sbdap_pico2.bat
```

---

## Entwicklungsumgebung

### Build

- **IDE:** CLion
- **Buildsystem:** CMake
- **Generator:** Ninja
- **Toolchain:** `arm-none-eabi-gcc`
- **SDK:** Raspberry Pi Pico SDK

### Debug

- **Debugger-Hardware:** Selfbus Debug Adapter (SBDAP)
- **Protokoll:** SWD / CMSIS-DAP
- **Server:** OpenOCD
- **Frontend:** GDB / CLion

Damit ist vollständiges Debugging möglich:

- Flashen aus der IDE
- Breakpoints
- Step Into / Step Over
- Register- und Variableninspektion

---

## Testplattform und Zielplattform

### Aktuelle Testplattform

Aktuell wird mit einem **Raspberry Pi Pico 2** getestet, da damit Bring-up, USB-CDC, UART und Debugging schnell verifiziert werden können.

### Eigentliche Zielplattform

Die Architektur wird auf den **RP2354B** ausgelegt.

Wichtig dabei:

- Der Code soll **nicht auf Pico-2-spezifische Besonderheiten optimiert** werden.
- Flash-bezogene Logik wird **relativ zur konfigurierten Flashgröße** implementiert.
- Dadurch bleibt die Firmware sowohl auf dem Pico 2 als Testboard als auch später auf einem RP2354B-Zielboard korrekt lauffähig.

---

## Architektur

Die Firmware ist in Schichten aufgebaut.

```text
Application / Device Logic
        │
Selfbus Firmware Layer
        │
Input / Button Layer
        │
Hardware Abstraction Layer (HAL)
        │
RP2354 Hardware + Pico SDK
```

Vorteile dieses Aufbaus:

- saubere Trennung von Gerätefunktion und Hardwarezugriff
- bessere Portierbarkeit
- einfachere Tests
- geringere Kopplung zwischen Modulen

---

## Hardware Abstraction Layer (HAL)

Alle direkten Hardwarezugriffe laufen über `sb_*`-Module.

### `sb_time`

Dateien:

- `platform/rp2354/include/sb_time.h`
- `platform/rp2354/src/sb_time.cpp`

Zweck:

- Zeitbasis der Firmware
- Millisekunden-Zeitstempel
- Delay-Funktion

Beispielhafte Funktionen:

```cpp
uint32_t sb_millis();
uint32_t sb_micros();
void sb_delay_ms(uint32_t ms);
```

Diese Funktionen werden für Debounce, Button-Erkennung, Zeitfenster und Polling verwendet.

---

### `sb_gpio`

Dateien:

- `platform/rp2354/include/sb_gpio.h`
- `platform/rp2354/src/sb_gpio.cpp`

Zweck:

- Initialisieren von GPIOs
- digitale Eingänge lesen
- digitale Ausgänge setzen

Beispielhafte Funktionen:

```cpp
sb_gpio_init(...);
sb_gpio_read(...);
sb_gpio_write(...);
```

Diese Abstraktion entkoppelt höhere Firmwareteile von den Pico-SDK-GPIO-Aufrufen.

---

### `sb_uart`

Dateien:

- `platform/rp2354/include/sb_uart.h`
- `platform/rp2354/src/sb_uart.cpp`

Zweck:

- UART0 initialisieren
- Daten senden und empfangen

Der UART-Layer wurde zunächst mit einem FTDI-Adapter bzw. mit Testausgaben verifiziert.

Er ist wichtig für spätere KNX-nahe Kommunikation oder externe serielle Schnittstellen.

---

### `sb_cpu`

Dateien:

- `platform/rp2354/include/sb_cpu.h`
- `platform/rp2354/src/sb_cpu.cpp`

Zweck:

- Interrupts sperren / wiederherstellen
- `WFI` (Wait For Interrupt)
- Systemreset

Beispielhafte Funktionen:

```cpp
uint32_t sb_irq_disable();
void sb_irq_restore(uint32_t state);
void sb_wait_for_interrupt();
void sb_system_reset();
```

Diese Funktionen werden für kritische Abschnitte und später für Low-Power-/Idle-Logik benötigt.

---

### `sb_flash`

Dateien:

- `platform/rp2354/include/sb_flash.h`
- `platform/rp2354/src/sb_flash.cpp`

Zweck:

- Zugriff auf persistenten Speicher im Flash
- Ablage von Konfigurationsdaten, Parametern und Zuständen

Aktuelles Konzept:

- Die **letzten 16 KB des Flash** sind für Selfbus-Storage reserviert.
- Die Position wird **nicht hart kodiert**, sondern aus der tatsächlichen Flashgröße berechnet.

Dadurch funktioniert derselbe Code auf:

- Pico 2 (4 MB Flash)
- RP2354B (2 MB Flash)

Beispielhafter Aufbau:

```text
Flash
├─ Firmware
└─ Selfbus Storage (letzte 16 KB)
```

Der Zugriff erfolgt über Funktionen wie:

```cpp
uint32_t sb_flash_storage_offset();
void sb_flash_read(...);
bool sb_flash_erase(...);
bool sb_flash_write(...);
```

---

### `sb_irq`

Dateien:

- `platform/rp2354/include/sb_irq.h`
- `platform/rp2354/src/sb_irq.cpp`

Zweck:

- GPIO-Interrupts abstrahieren
- Callback-Registrierung für steigende/fallende Flanken

Unterstützte Trigger:

- Rising
- Falling
- Both

Dieser Layer ist die Grundlage für interruptbasierte Eingangslogik.

---

## Input Layer

### `sb_input`

Dateien:

- `platform/rp2354/include/sb_input.h`
- `platform/rp2354/src/sb_input.cpp`

Zweck:

- debounced digitale Eingänge
- Callback bei stabilem Zustand
- Kapselung von GPIO + IRQ + Zeitbasis

Eigenschaften:

- Interrupt-basiert
- Software-Debounce
- Callback-basiertes Event-Modell

Beispielhafte Verwendung:

```cpp
SbInputConfig cfg{};
cfg.pin = 10;
cfg.pullup = true;
cfg.debounce_ms = 20;

sb_input_init(cfg, callback);
```

Damit wurde eine saubere Grundlage für Taster- und Binäreingänge geschaffen.

---

## Button Layer

### `sb_button`

Dateien:

- `platform/rp2354/include/sb_button.h`
- `platform/rp2354/src/sb_button.cpp`

Zweck:

- Erkennung typischer Tasterereignisse auf Basis von `sb_input`

Unterstützte Events:

- `Press`
- `Release`
- `Short`
- `Long`
- `Double`

Interne Zustandsmaschine:

```text
Idle
Down
WaitSecondPress
DownSecond
```

Typische Parameter:

- Debounce: `20 ms`
- Long Press: `500 ms`
- Double Click Window: `250 ms`

Damit wurde genau die Funktionalität umgesetzt, die für spätere Selfbus-Eingangsgeräte wichtig ist, insbesondere:

- kurzer Tastendruck
- langer Tastendruck
- Doppelklick

---

## Beispiel-Firmware

Das Bring-up-Beispiel liegt unter:

- `examples/bringup_blinky_uart/main.cpp`

Dieses Beispiel demonstriert aktuell:

- USB-CDC-Ausgabe
- UART0-Test
- LED-Blinken
- Button-Event-Ausgabe

Für den Button-Test wurde erfolgreich verifiziert:

- Taster an `GP10`
- interner Pull-up aktiv
- Taster gegen GND

Beispielausgabe auf USB-CDC:

```text
button pin=10 event=Press
button pin=10 event=Release
button pin=10 event=Short
```

Auch `Long` und `Double` wurden erfolgreich getestet.

---

## Debugging mit SBDAP

Das Debugging funktioniert mit:

- SBDAP als CMSIS-DAP Probe
- OpenOCD-Konfiguration unter `openocd/sbdap_pico2.cfg`
- CLion Embedded GDB Server

Zusätzlich wurden Hilfsskripte erstellt:

- `tools/start_openocd_sbdap_pico2.bat`
- `tools/start_gdb_sbdap_pico2.bat`

Damit wurde erfolgreich verifiziert:

- OpenOCD erkennt beide RP2350-Kerne
- Flashen per GDB/OpenOCD funktioniert
- Breakpoints in `main()` funktionieren
- Debugging direkt aus CLion funktioniert

---

## Ergebnis des aktuellen Standes

Bisher steht eine robuste technische Basis für die eigentliche Selfbus-Portierung.

Vorhanden sind jetzt:

- Buildsystem für RP2354 mit Pico SDK
- Debugging-Infrastruktur
- HAL für zentrale Hardwarefunktionen
- debounced Input-Layer
- Button-/Tasterlogik mit Short/Long/Double
- Flash-Abstraktion für persistenten Speicher

Das ist eine gute Grundlage, um im nächsten Schritt die eigentliche Firmwarearchitektur und danach bestehende Selfbus-Funktionsblöcke zu portieren.

---

## Nächste Schritte

### 1. Firmware-Architektur

Als nächster sinnvoller Schritt ist ein zentraler Event-/Poll-Loop vorgesehen, z. B.:

```text
sb_button_poll
sb_timer_poll
sb_knx_poll
sb_app_poll
```

Dadurch bekommt die Firmware eine einheitliche Laufzeitstruktur.

### 2. Integration bestehender Selfbus-Logik

Danach kann schrittweise Logik aus `software-arm-lib` portiert werden, z. B.:

- Switch-Logik
- Input-Verarbeitung
- Timing- und Aktorlogik
- KNX-nahe Kommunikationsschichten

### 3. Geräteportierung

Später können darauf konkrete Geräte aufsetzen, etwa:

- Binary Input
- MultiSensorAktor
- weitere Selfbus-Geräte

---

## Architekturdiagramm

> Hinweis: Das Architekturdiagramm kann z. B. unter `docs/images/selfbus-rp2354-architecture.png` im Repository abgelegt und dann hier referenziert werden.

Beispiel:

```md
![Selfbus RP2354 Architektur](docs/images/selfbus-rp2354-architecture.png)
```

---

## Zusammenfassung

Mit dem bisherigen Stand wurde die wichtigste Grundlage der Portierung geschaffen:

- moderne Toolchain
- funktionierendes Debugging
- saubere Hardwareabstraktion
- Eingangslogik mit Debounce und Button-Events

Damit ist der Wechsel von der alten LPC1115-/MCUXpresso-Welt auf eine moderne RP2354-basierte Architektur erfolgreich vorbereitet.
