#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "net.h"

#include "soc/rtc_cntl_reg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Built with BROWNOUT=0 to stop the detector resetting the chip on a
   weak 5V supply (a laptop USB port, say). The rail still has to hold
   up well enough for the chip to run; this only removes the trip. The
   write runs from a global constructor so it lands before Protomatter
   and WiFi start drawing current. */
#ifndef HUB75_BROWNOUT_OFF
#define HUB75_BROWNOUT_OFF 0
#endif

#if HUB75_BROWNOUT_OFF
struct BrownoutOff {
  BrownoutOff() { WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); }
};
static BrownoutOff g_brownout_off;
#endif

/* The board side of net.h: WiFi plus one HTTP(S) GET. HTTPS uses
   setInsecure(), which skips certificate checks. That is fine for a
   public read-only API on a hobby panel, but it does mean the transport
   is not authenticated.

   Kept deliberately hard to wedge:
   - the join does not block; the sketch keeps drawing and polls status
   - a dropped or failed link is retried on a timer, not given up on
   - a response is read with a hard size cap and a stall timeout, so a
     huge or hung reply cannot exhaust the heap or spin forever
   The lifecycle is mirrored to Serial (115200) for debugging. */

static int g_called = 0;
static int g_started = 0;
static int g_gave_up = 0;
static unsigned long g_start_ms = 0;
static unsigned long g_last_retry = 0;
static char g_ip[20];
static NetStatus g_last = NET_OFF;

enum { NET_JOIN_TIMEOUT_MS = 20000 };
enum { NET_RETRY_MS = 10000 };
enum { NET_HTTP_TIMEOUT_MS = 15000 };
enum { NET_HTTP_CONNECT_MS = 5000 };

extern "C" int net_begin(void) {
  g_called = 1;

  if (HUB75_WIFI_SSID[0] == '\0') {
    Serial.println("net: no WiFi credentials (copy secrets.example.h to "
                   "secrets.h)");
    g_started = 0;
    return 0;
  }

#if HUB75_BROWNOUT_OFF
  Serial.println("net: brownout detector disabled at boot");
#endif

  Serial.printf("net: joining \"%s\"\n", HUB75_WIFI_SSID);
  WiFi.mode(WIFI_STA);
  /* A HUB75 panel plus WiFi TX can pull more than a USB port supplies,
     and the brownout detector resets the chip when the rail sags. This
     modest cap shaves the peak but does not make up for a weak supply:
     the real fix is a proper 5V source for the panel. */
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setAutoReconnect(true);
  WiFi.begin(HUB75_WIFI_SSID, HUB75_WIFI_PASS);

  g_started = 1;
  g_gave_up = 0;
  g_start_ms = millis();
  g_last_retry = g_start_ms;
  return 1;
}

extern "C" NetStatus net_status(void) {
  NetStatus st;
  wl_status_t ws;

  if (!g_called) {
    return NET_OFF;
  }
  if (!g_started) {
    return NET_FAILED;
  }

  ws = WiFi.status();

  if (ws == WL_CONNECTED) {
    g_gave_up = 0;
    st = NET_READY;
  } else if (!g_gave_up && millis() - g_start_ms < NET_JOIN_TIMEOUT_MS) {
    st = NET_CONNECTING;
  } else {
    st = NET_FAILED;

    /* Re-arm on a timer: a wrong password, a dropped link, or a router
       that was down should all recover without a power cycle. */
    if (millis() - g_last_retry > NET_RETRY_MS) {
      g_last_retry = millis();
      g_start_ms = millis();
      g_gave_up = 0;
      Serial.println("net: retrying WiFi");
      WiFi.disconnect();
      WiFi.begin(HUB75_WIFI_SSID, HUB75_WIFI_PASS);
    }
  }

  if (st != g_last) {
    switch (st) {
    case NET_CONNECTING:
      Serial.println("net: connecting...");
      break;
    case NET_READY:
      Serial.print("net: connected, IP ");
      Serial.println(WiFi.localIP());
      break;
    case NET_FAILED:
      Serial.printf("net: failed (WiFi status %d)\n", (int)ws);
      break;
    default:
      break;
    }
    g_last = st;
  }

  return st;
}

extern "C" const char *net_status_text(void) {
  switch (net_status()) {
  case NET_OFF:
    return "NET OFF";
  case NET_CONNECTING:
    return "WIFI CONNECTING";
  case NET_READY:
    return "WIFI OK";
  case NET_FAILED:
  default:
    return HUB75_WIFI_SSID[0] ? "WIFI FAILED" : "NO WIFI CREDS";
  }
}

extern "C" const char *net_ip_text(void) {
  if (net_status() != NET_READY) {
    return "";
  }
  snprintf(g_ip, sizeof g_ip, "%s", WiFi.localIP().toString().c_str());
  return g_ip;
}

extern "C" int net_ready(void) { return net_status() == NET_READY; }

static int do_http_get(const char *url, NetResponse *out) {
  HTTPClient http;
  WiFiClient plain;
  WiFiClientSecure secure;
  WiFiClient *stream;
  bool started;
  char *buf;
  int code;
  int size;
  size_t n;
  unsigned long t0;

  if (!out) {
    return 0;
  }
  out->body = NULL;
  out->len = 0;
  out->status = 0;
  out->ok = 0;

  if (!net_ready()) {
    out->status = -1;
    Serial.println("net: fetch skipped, not connected");
    return 0;
  }

  if (strncmp(url, "https://", 8) == 0) {
    secure.setInsecure();
    started = http.begin(secure, url);
  } else {
    started = http.begin(plain, url);
  }

  if (!started) {
    out->status = -1;
    Serial.println("net: bad URL");
    return 0;
  }

  http.setTimeout(NET_HTTP_TIMEOUT_MS);
  http.setConnectTimeout(NET_HTTP_CONNECT_MS);
  http.setUserAgent("hub75-sketcher/1.0");

  /* Log the URL without its query string: it can carry an API key. */
  {
    const char *q = strchr(url, '?');
    int cut = q ? (int)(q - url) : (int)strlen(url);
    Serial.printf("net: GET %.*s\n", cut, url);
  }

  code = http.GET();
  out->status = code;

  if (code <= 0) {
    Serial.printf("net: -> request failed (%d)\n", code);
    http.end();
    return 0;
  }

  size = http.getSize();
  if (size > NET_MAX_BODY) {
    Serial.printf("net: -> body too large (%d bytes)\n", size);
    http.end();
    return 0;
  }

  buf = (char *)malloc(NET_MAX_BODY + 1);
  if (!buf) {
    Serial.println("net: out of memory");
    http.end();
    return 0;
  }

  stream = http.getStreamPtr();
  n = 0;
  t0 = millis();
  while (n < NET_MAX_BODY && millis() - t0 < NET_HTTP_TIMEOUT_MS) {
    size_t avail = stream ? stream->available() : 0;

    if (avail > 0) {
      size_t want = NET_MAX_BODY - n;
      int got = stream->readBytes(buf + n, avail < want ? avail : want);
      if (got <= 0) {
        break;
      }
      n += (size_t)got;
      t0 = millis();
    } else if (size >= 0 && n >= (size_t)size) {
      break;
    } else if (!http.connected()) {
      break;
    } else {
      delay(1);
    }
  }

  buf[n] = '\0';
  out->body = buf;
  out->len = (int)n;
  out->ok = (code >= 200 && code < 300) ? 1 : 0;

  Serial.printf("net: -> HTTP %d, %u bytes\n", code, (unsigned)n);
  http.end();
  return out->ok;
}

extern "C" int fetch(const char *url, NetResponse *out) {
  return do_http_get(url, out);
}

/* Background fetch: the blocking GET runs on its own task so draw() keeps
   running (and a refresh spinner keeps spinning) while a request is in
   flight. One request at a time, handed back through fetch_poll(). */

static SemaphoreHandle_t g_lock = NULL;
static SemaphoreHandle_t g_go = NULL;
static char g_url[256];
static NetResponse g_resp;
static int g_state = 0; /* 0 idle, 1 requested, 2 running, 3 ready */

static void net_task(void *arg) {
  (void)arg;
  for (;;) {
    NetResponse r;

    xSemaphoreTake(g_go, portMAX_DELAY);

    xSemaphoreTake(g_lock, portMAX_DELAY);
    g_state = 2;
    xSemaphoreGive(g_lock);

    memset(&r, 0, sizeof r);
    do_http_get(g_url, &r);

    xSemaphoreTake(g_lock, portMAX_DELAY);
    g_resp = r;
    g_state = 3;
    xSemaphoreGive(g_lock);
  }
}

static void net_task_start(void) {
  if (g_lock) {
    return;
  }
  g_lock = xSemaphoreCreateMutex();
  g_go = xSemaphoreCreateBinary();
  /* TLS wants a roomy stack; give the task its own. */
  xTaskCreate(net_task, "hub75-net", 16384, NULL, 5, NULL);
}

extern "C" int fetch_start(const char *url) {
  net_task_start();

  xSemaphoreTake(g_lock, portMAX_DELAY);
  if (g_state != 0) {
    xSemaphoreGive(g_lock);
    return 0;
  }
  snprintf(g_url, sizeof g_url, "%s", url);
  g_state = 1;
  xSemaphoreGive(g_lock);

  xSemaphoreGive(g_go);
  return 1;
}

extern "C" int fetch_busy(void) {
  int s;

  if (!g_lock) {
    return 0;
  }
  xSemaphoreTake(g_lock, portMAX_DELAY);
  s = g_state;
  xSemaphoreGive(g_lock);
  return s == 1 || s == 2;
}

extern "C" int fetch_poll(NetResponse *out) {
  if (!g_lock) {
    return 0;
  }
  xSemaphoreTake(g_lock, portMAX_DELAY);
  if (g_state == 3) {
    *out = g_resp;
    memset(&g_resp, 0, sizeof g_resp);
    g_state = 0;
    xSemaphoreGive(g_lock);
    return 1;
  }
  xSemaphoreGive(g_lock);
  return 0;
}

extern "C" void net_free(NetResponse *out) {
  if (!out) {
    return;
  }
  free(out->body);
  out->body = NULL;
  out->len = 0;
  out->status = 0;
  out->ok = 0;
}
