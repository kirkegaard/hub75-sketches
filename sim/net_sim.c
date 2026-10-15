#include "net.h"

#include <curl/curl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The desktop side of net.h: a real HTTP(S) GET through libcurl, so a
   sketch that fetches can be developed and dumped without the board.
   The body is copied out of curl's callback into one growing buffer. */

typedef struct {
  char *p;
  size_t len;
} Buffer;

static int begun = 0;

static size_t on_write(char *data, size_t size, size_t nmemb, void *user) {
  size_t n = size * nmemb;
  Buffer *b = (Buffer *)user;
  char *grown;

  /* Refuse an over-large body the same way the board does. */
  if (b->len + n > NET_MAX_BODY) {
    return 0;
  }
  grown = (char *)realloc(b->p, b->len + n + 1);
  if (!grown) {
    return 0;
  }
  b->p = grown;
  memcpy(b->p + b->len, data, n);
  b->len += n;
  b->p[b->len] = '\0';
  return n;
}

int net_begin(void) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  begun = 1;
  fprintf(stderr, "net: libcurl ready\n");
  return 1;
}

NetStatus net_status(void) { return begun ? NET_READY : NET_OFF; }

const char *net_status_text(void) {
  return begun ? "NET OK" : "NET OFF";
}

const char *net_ip_text(void) { return ""; }

int net_ready(void) { return begun; }

int fetch(const char *url, NetResponse *out) {
  CURL *curl;
  CURLcode rc;
  Buffer b;
  long status = 0;

  if (!out) {
    return 0;
  }
  out->body = NULL;
  out->len = 0;
  out->status = 0;
  out->ok = 0;

  curl = curl_easy_init();
  if (!curl) {
    out->status = -1;
    return 0;
  }

  b.p = NULL;
  b.len = 0;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &b);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "hub75-sketcher/1.0");
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

  /* Log the URL without its query string: it can carry an API key. */
  {
    const char *q = strchr(url, '?');
    int n = q ? (int)(q - url) : (int)strlen(url);
    fprintf(stderr, "net: GET %.*s\n", n, url);
  }

  rc = curl_easy_perform(curl);
  if (rc == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    out->status = (int)status;
    out->body = b.p;
    out->len = (int)b.len;
    out->ok = (status >= 200 && status < 300) ? 1 : 0;
    if (!out->body) {
      out->body = (char *)calloc(1, 1);
    }
    fprintf(stderr, "net: -> HTTP %d, %d bytes\n", out->status, out->len);
  } else {
    free(b.p);
    out->status = -1;
    fprintf(stderr, "net: -> curl error %d (%s)\n", (int)rc,
            curl_easy_strerror(rc));
  }

  curl_easy_cleanup(curl);
  return out->ok;
}

void net_free(NetResponse *out) {
  if (!out) {
    return;
  }
  free(out->body);
  out->body = NULL;
  out->len = 0;
  out->status = 0;
  out->ok = 0;
}

/* The desktop has no second task, so the request runs inline and the
   result is buffered for fetch_poll(). */
static NetResponse pending;
static int pending_ready = 0;

int fetch_start(const char *url) {
  NetResponse r;

  if (pending_ready) {
    return 0;
  }
  fetch(url, &r);
  pending = r;
  pending_ready = 1;
  return 1;
}

int fetch_busy(void) { return 0; }

int fetch_poll(NetResponse *out) {
  if (!pending_ready) {
    return 0;
  }
  *out = pending;
  memset(&pending, 0, sizeof pending);
  pending_ready = 0;
  return 1;
}
