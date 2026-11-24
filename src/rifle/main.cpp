// GEWEHR (ESP32): Taster -> ESP-NOW SHOT; TSOP34836 lesen -> ESP-NOW HIT
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include "protocol.h"

static const uint8_t PIN_TSOP = 27;     // TSOP OUT (idle HIGH, aktiv LOW)
static const uint8_t PIN_BUTTON = 26;   // Taster nach GND
static const uint8_t PIN_LED_HIT = 25;  // externe LED (+ Vorwiderstand) nach GND
static const uint8_t PIN_LED_SHOT = 2;  // Onboard-LED

IRrecv irrecv(PIN_TSOP);
decode_results results;

static uint16_t shotId = 0;
static uint32_t shotTime = 0;
static bool windowOpen = false;
static bool hitReported = false;

static void sendMsg(uint8_t type, uint8_t targetId, uint8_t repeat = 1) {
  Msg m{MSG_MAGIC, type, shotId, targetId};
  for (uint8_t i = 0; i < repeat; i++) {
    esp_now_send(BROADCAST_MAC, reinterpret_cast<uint8_t *>(&m), sizeof(m));
    if (i + 1 < repeat) delay(3);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_HIT, OUTPUT);
  pinMode(PIN_LED_SHOT, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FEHLER");
  }
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  irrecv.enableIRIn();
  Serial.printf("RIFLE bereit, MAC %s\n", WiFi.macAddress().c_str());
}

void loop() {
  static bool lastBtn = HIGH;
  static uint32_t lastPress = 0;

  bool btn = digitalRead(PIN_BUTTON);
  if (lastBtn == HIGH && btn == LOW && millis() - lastPress > 50) {
    lastPress = millis();
    shotId++;
    shotTime = lastPress;
    windowOpen = true;
    hitReported = false;
    digitalWrite(PIN_LED_HIT, LOW);
    digitalWrite(PIN_LED_SHOT, HIGH);
    sendMsg(MSG_SHOT, 0);
    Serial.printf("SHOT #%u gesendet\n", shotId);
  }
  lastBtn = btn;

  if (irrecv.decode(&results)) {
    bool isTarget = (results.decode_type == RC5 || results.decode_type == RC5X) &&
                    results.command == IR_CMD_TARGET;
    Serial.printf("IR: %s", typeToString(results.decode_type).c_str());
    Serial.printf(" addr=%u cmd=0x%02X %s\n", (unsigned)results.address,
                  (unsigned)results.command, isTarget ? "(Ziel)" : "(fremd)");
    if (isTarget && windowOpen && !hitReported) {
      hitReported = true;
      digitalWrite(PIN_LED_HIT, HIGH);
      sendMsg(MSG_HIT, (uint8_t)results.address, 3);  // 3x: Broadcast hat kein ACK
      Serial.printf("TREFFER Ziel %u nach %lu ms\n", (unsigned)results.address,
                    (unsigned long)(millis() - shotTime));
    }
    irrecv.resume();
  }

  if (windowOpen && millis() - shotTime > HIT_WINDOW_MS) {
    windowOpen = false;
    digitalWrite(PIN_LED_SHOT, LOW);
    if (!hitReported) Serial.printf("SHOT #%u: kein Treffer\n", shotId);
  }
}
