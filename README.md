# gallIR – Elektronischer Schießstand (Proof of Concept)

## Prinzip
- **Gewehr (ESP32)**: Taster + **TSOP34836** (36-kHz-IR-Empfänger, in einem Rohr/Tubus montiert, damit er nur in Zielrichtung "sieht").
- **Schießstand (ESP8266 NodeMCU)**: pro Zielscheibe eine **IR-LED** (36 kHz moduliert) + Beleuchtungs-LED.
- Ablauf:
  1. Taster am Gewehr → ESP-NOW `SHOT` (Broadcast, Kanal 1).
  2. Stand lässt alle *scharfen* Ziele nacheinander (Zeitmultiplex) je einen **RC5-Frame** senden (Adresse = Ziel-ID, ~25 ms/Frame) für `FIRE_DURATION_MS`.
  3. Gewehr dekodiert per TSOP die Ziel-ID → ESP-NOW `HIT{targetId}` (3× gesendet, Broadcast hat kein ACK).
  4. Stand schaltet die Beleuchtung dieses Ziels aus und sendet für dieses Ziel nicht mehr.
  5. FLASH-Taster am NodeMCU (D3) = Reset, alle Ziele wieder scharf.
- Skalierung: weitere Zeile in `targets[]` (`src/range/main.cpp`) mit eigener ID 1..31 und eigenen Pins. Bei vielen Zielen `FIRE_DURATION_MS` (`include/protocol.h`) erhöhen (ca. 40 ms pro Ziel und Runde).

## Verdrahtung
**ESP32 (Gewehr)**
| Teil | Pin |
|---|---|
| TSOP34836 OUT | GPIO27 |
| TSOP34836 VS | 3V3 über 100 Ω, dazu 4,7 µF zwischen VS und GND (am TSOP) |
| TSOP34836 GND | GND |
| Taster | GPIO26 ↔ GND (interner Pull-up) |
| Treffer-LED | GPIO25 → 220 Ω → LED → GND |

TSOP-Pinbelegung (OUT/GND/VS) bitte gegen das Datenblatt prüfen, bevor du Spannung anlegst.

**NodeMCU (Stand)**
| Teil | Pin |
|---|---|
| IR-LED | D2 (GPIO4) → ~220 Ω → IR-LED → GND (ca. 9 mA, reicht für den Test auf kurze Distanz; später per Transistor) |
| Ziel-Beleuchtung (LED) | D1 (GPIO5) → 220 Ω → LED → GND |
| Reset | FLASH-Taster (D3) onboard |

## Bauen / Flashen (WSL2)
USB-Boards in WSL2: mit [usbipd-win](https://github.com/dorssel/usbipd-win) `usbipd attach --wsl --busid <ID>` (Windows, Admin), dann erscheint `/dev/ttyUSB0`.

```bash
pip install platformio
pio run -e rifle -t upload      # ESP32
pio run -e range -t upload      # ESP8266
pio device monitor -b 115200
```

## Inbetriebnahme in Stufen
1. **IR-Strecke**: `range_irtest` auf den NodeMCU flashen (sendet dauernd), `rifle` auf den ESP32. IR-LED auf den TSOP richten (5–30 cm). Monitor des ESP32 muss `IR: RC5 addr=1 cmd=0x2A (Ziel)` zeigen.
2. **Gesamtablauf**: `range` flashen, Taster am Gewehr drücken → `TREFFER Ziel 1`, am Stand geht die Beleuchtung aus.

Hinweis: Das Projekt wurde ohne Hardware/Compiler-Lauf erstellt (Cloud-Container ohne PlatformIO-Zugriff); erste Kompilierung kann noch Kleinigkeiten zeigen.
