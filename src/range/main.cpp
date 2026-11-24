// SCHIESSSTAND (ESP8266): SHOT empfangen -> Ziele senden IR; HIT empfangen -> Ziel-Licht aus
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "protocol.h"

// Zielscheiben - fuer weitere Ziele einfach Zeilen ergaenzen (id 1..31, eindeutig)
struct Target {
  uint8_t id;
  uint8_t pinIr;     // IR-LED (36 kHz Traeger per Software)
  uint8_t pinLight;  // Beleuchtung (an = scharf, aus = getroffen)
  bool active;
  IRsend *ir;
};
static Target targets[] = {
    {1, D2, D1, true, nullptr},
};
static const size_t NUM_TARGETS = sizeof(targets) / sizeof(targets[0]);
static const uint8_t PIN_RESET = D3;  // FLASH-Taster am NodeMCU (GPIO0)
static const uint16_t FRAME_GAP_MS = 15;

static volatile bool shotPending = false;
static volatile uint16_t pendingShotId = 0;
static volatile uint32_t hitMask = 0;  // Bit n = Ziel n getroffen
static volatile bool resetPending = false;

static void onRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(Msg)) return;
  Msg m;
  memcpy(&m, data, sizeof(m));
  if (m.magic != MSG_MAGIC) return;
  switch (m.type) {
    case MSG_SHOT:
      pendingShotId = m.shotId;
      shotPending = true;
      break;
    case MSG_HIT:
      if (m.targetId < 32) hitMask |= (1UL << m.targetId);
      break;
    case MSG_RESET:
      resetPending = true;
      break;
  }
}

static void applyHits() {
  uint32_t mask = hitMask;
  if (!mask) return;
  for (size_t i = 0; i < NUM_TARGETS; i++) {
    Target &t = targets[i];
    if (t.active && (mask & (1UL << t.id))) {
      t.active = false;
      digitalWrite(t.pinLight, LOW);
      Serial.printf("Ziel %u getroffen -> Licht aus\n", t.id);
    }
  }
  hitMask = 0;
}

static void resetAll() {
  for (size_t i = 0; i < NUM_TARGETS; i++) {
    targets[i].active = true;
    digitalWrite(targets[i].pinLight, HIGH);
  }
  hitMask = 0;
  Serial.println("RESET: alle Ziele scharf");
}

static void sendFrame(Target &t) {
  t.ir->sendRC5(t.ir->encodeRC5(t.id, IR_CMD_TARGET), kRC5Bits);
}

static void fire(uint16_t shotId) {
  Serial.printf("SHOT #%u -> Ziele senden\n", shotId);
  uint32_t start = millis();
  while (millis() - start < FIRE_DURATION_MS) {
    for (size_t i = 0; i < NUM_TARGETS; i++) {
      if (!targets[i].active) continue;
      sendFrame(targets[i]);
      delay(FRAME_GAP_MS);  // gibt dem WLAN-Stack Zeit (ESP-NOW-Callbacks)
      applyHits();
    }
    delay(1);
    applyHits();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RESET, INPUT_PULLUP);
  for (size_t i = 0; i < NUM_TARGETS; i++) {
    pinMode(targets[i].pinLight, OUTPUT);
    digitalWrite(targets[i].pinLight, HIGH);
    targets[i].ir = new IRsend(targets[i].pinIr);
    targets[i].ir->begin();
  }

#ifdef IR_LINK_TEST
  Serial.println("RANGE IR_LINK_TEST: sende alle 100 ms RC5-Frames");
#else
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(ESPNOW_CHANNEL);
  if (esp_now_init() != 0) Serial.println("ESP-NOW init FEHLER");
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_add_peer(const_cast<uint8_t *>(BROADCAST_MAC), ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL,
                   nullptr, 0);
  esp_now_register_recv_cb(onRecv);
  Serial.printf("RANGE bereit, MAC %s, %u Ziel(e)\n", WiFi.macAddress().c_str(),
                (unsigned)NUM_TARGETS);
#endif
}

void loop() {
#ifdef IR_LINK_TEST
  for (size_t i = 0; i < NUM_TARGETS; i++) sendFrame(targets[i]);
  delay(100);
#else
  if (shotPending) {
    shotPending = false;
    fire(pendingShotId);
  }
  applyHits();
  if (resetPending || digitalRead(PIN_RESET) == LOW) {
    resetPending = false;
    resetAll();
    delay(300);
  }
  delay(1);
#endif
}
