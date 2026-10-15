#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_random.h>

extern "C" {
#include "ws.h"
}

#include <stdlib.h>

/* The board transport: a WiFiClient, secure for wss://. The client task
   runs ws_client_loop(); these hooks only move bytes. */

struct FwIo {
  WiFiClient *client;
};

extern "C" void *ws_io_connect(const char *host, int port, int tls) {
  FwIo *io = (FwIo *)malloc(sizeof(FwIo));

  if (!io) {
    return NULL;
  }

  if (tls) {
    WiFiClientSecure *c = new WiFiClientSecure();
    c->setInsecure();
    if (!c->connect(host, (uint16_t)port)) {
      delete c;
      free(io);
      return NULL;
    }
    io->client = c;
  } else {
    WiFiClient *c = new WiFiClient();
    if (!c->connect(host, (uint16_t)port)) {
      delete c;
      free(io);
      return NULL;
    }
    io->client = c;
  }

  return io;
}

extern "C" int ws_io_write(void *p, const void *data, size_t len) {
  FwIo *io = (FwIo *)p;
  size_t w = io->client->write((const uint8_t *)data, len);
  return w == len ? (int)len : -1;
}

extern "C" int ws_io_read(void *p, void *data, size_t len, int timeout_ms) {
  FwIo *io = (FwIo *)p;
  unsigned long start = millis();

  for (;;) {
    int a = io->client->available();
    if (a > 0) {
      size_t want = (size_t)a < len ? (size_t)a : len;
      int r = io->client->readBytes((uint8_t *)data, want);
      return r > 0 ? r : -1;
    }
    if (!io->client->connected()) {
      return -1;
    }
    if ((int)(millis() - start) >= timeout_ms) {
      return 0;
    }
    delay(1);
  }
}

extern "C" void ws_io_close(void *p) {
  FwIo *io = (FwIo *)p;
  if (io) {
    io->client->stop();
    delete io->client;
    free(io);
  }
}

extern "C" void ws_io_random(void *buf, size_t len) {
  esp_fill_random(buf, len);
}

extern "C" void ws_io_delay(int ms) { delay((unsigned long)ms); }

extern "C" void ws_task_create(void (*fn)(void *), void *arg) {
  xTaskCreate(fn, "hub75-ws", 16384, arg, 5, NULL);
}

extern "C" void ws_io_log(const char *msg) { Serial.println(msg); }
