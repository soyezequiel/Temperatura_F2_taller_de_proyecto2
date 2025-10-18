// web_send.h
#pragma once
#include <Arduino.h>
#include <WiFi.h>

inline void sendPROGMEM(WiFiClient &client, const char* pgm) {
  const size_t CHUNK = 512;
  size_t len = strlen_P(pgm);
  for (size_t off = 0; off < len; off += CHUNK) {
    char buf[CHUNK + 1];
    size_t n = min(CHUNK, len - off);
    memcpy_P(buf, pgm + off, n);
    buf[n] = 0;
    client.print(buf);
    vTaskDelay(1); // ceder CPU
  }
}
