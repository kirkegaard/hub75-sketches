#ifndef HUB75_NET_H
#define HUB75_NET_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Network access for sketches. A sketch that fetches calls net_begin()
   once from setup(), then polls net_status() and calls fetch() from
   draw(). net_begin() does not block: it kicks off the link and returns,
   so a sketch can keep drawing (and show the status) while the board
   joins WiFi. Only fetch() blocks, and it can hold the panel for a
   second or two, so keep it on an interval instead of every frame.

   The simulator talks to the real network through libcurl, so a sketch
   behaves the same on the desktop and the board. Secrets (WiFi and any
   API tokens) come from a gitignored secrets.h next to the repo root;
   see secrets.example.h. Without it the macros below are empty and the
   board has nothing to connect with. */

#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#endif

#ifndef HUB75_WIFI_SSID
#define HUB75_WIFI_SSID ""
#endif
#ifndef HUB75_WIFI_PASS
#define HUB75_WIFI_PASS ""
#endif
#ifndef HUB75_LASTFM_USER
#define HUB75_LASTFM_USER ""
#endif
#ifndef HUB75_LASTFM_TOKEN
#define HUB75_LASTFM_TOKEN ""
#endif

/* The most a backend will buffer from one response. A body larger than
   this is refused rather than allocated, so a bad or hostile reply
   cannot exhaust the board's heap. */
#define NET_MAX_BODY 16384

/* An HTTP response. body is NUL-terminated and owned by the caller: pass
   the struct to net_free() when done. status is the HTTP code, or a
   negative transport error. ok is 1 for a 2xx with a body. */
typedef struct {
  char *body;
  int len;
  int status;
  int ok;
} NetResponse;

/* Where the link is. A sketch draws net_status_text() until NET_READY,
   then fetches. */
typedef enum {
  NET_OFF = 0,    /* net_begin() has not run yet */
  NET_CONNECTING, /* joining the network */
  NET_READY,      /* fetch() can run */
  NET_FAILED      /* no credentials, or the join gave up */
} NetStatus;

/* Start the network and return at once: 1 if it was kicked off, 0 if
   there is nothing to connect with. The simulator starts libcurl and is
   ready immediately; the board begins joining WiFi and reports progress
   through net_status(). */
int net_begin(void);

/* The current link state. */
NetStatus net_status(void);

/* A short uppercase label for the panel, e.g. "WIFI CONNECTING",
   "WIFI OK", "NO WIFI CREDS". */
const char *net_status_text(void);

/* The local IP address once connected, or "" (the simulator has none). */
const char *net_ip_text(void);

/* 1 once a request can be made (net_status() == NET_READY). */
int net_ready(void);

/* GET url into out. Returns out->ok. Blocking. */
int fetch(const char *url, NetResponse *out);

/* Non-blocking fetch. fetch_start() kicks off one request at a time and
   returns 1 if it took it; while it runs, fetch_busy() is 1 and the
   sketch keeps drawing, so it can animate a refresh indicator.
   fetch_poll() returns 1 exactly once, handing the response to the
   caller (release it with net_free), after which the slot is free.
   The simulator runs the request inline, so it is never busy. */
int fetch_start(const char *url);
int fetch_busy(void);
int fetch_poll(NetResponse *out);

/* Release out->body and clear the struct. Safe on a zeroed struct. */
void net_free(NetResponse *out);

#ifdef __cplusplus
}
#endif

#endif
