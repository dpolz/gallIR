#pragma once
// Gemeinsame Definitionen fuer Gewehr (ESP32) und Schiessstand (ESP8266)
#include <stdint.h>

// --- ESP-NOW -------------------------------------------------------------
static const uint8_t ESPNOW_CHANNEL = 1;
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t MSG_MAGIC = 0xA5;

enum MsgType : uint8_t {
  MSG_SHOT = 1,   // Gewehr -> Stand : Abzug gedrueckt, Ziele sollen senden
  MSG_HIT = 2,    // Gewehr -> Stand : Ziel `targetId` wurde getroffen
  MSG_RESET = 3,  // beliebig -> Stand : alle Ziele wieder scharf
};

struct __attribute__((packed)) Msg {
  uint8_t magic;
  uint8_t type;
  uint16_t shotId;
  uint8_t targetId;
};

// --- IR (36 kHz, passend zum TSOP34836) -----------------------------------
// Jedes Ziel sendet einen RC5-Frame (~25 ms): Adresse = Ziel-ID (1..31),
// Command = IR_CMD_TARGET. Ziele senden nacheinander (Zeitmultiplex).
static const uint8_t IR_CMD_TARGET = 0x2A;

// Der Stand sendet nach einem SHOT so lange IR-Frames (reihum),
// das Gewehr akzeptiert Treffer etwas laenger (Funk-/Verarbeitungslatenz).
static const uint16_t FIRE_DURATION_MS = 300;
static const uint16_t HIT_WINDOW_MS = FIRE_DURATION_MS + 150;
