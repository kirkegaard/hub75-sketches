#ifndef HUB75_WS_H
#define HUB75_WS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A WebSocket client, the streaming cousin of net.h. A sketch opens one
   connection from setup() and polls for messages from draw(); the client
   runs on its own task and reconnects on its own, so the panel never
   freezes on the network.

   The protocol layer is deliberately thin: text frames in, text frames
   out. It knows nothing about what a server says; a sketch parses the
   payloads itself. The transport is the only per-platform part (a plain
   socket in the simulator, a TLS socket on the board). */

typedef enum {
  WS_OFF = 0,     /* ws_open() has not run */
  WS_CONNECTING,  /* connecting / handshaking */
  WS_OPEN,        /* ready to send and receive */
  WS_CLOSED       /* dropped; the client is retrying */
} WsStatus;

/* Open url (ws://host[:port]/path or wss://...). Starts the client once;
   later calls are ignored. */
void ws_open(const char *url);

/* Stop and forget the connection. */
void ws_close(void);

/* Current state. */
WsStatus ws_status(void);

/* Take the next received text message into buf (NUL-terminated). Returns
   its length, 0 if none is queued, or -1 if the connection is not open.
   Non-blocking. buf must be large enough for the biggest message. */
int ws_poll(char *buf, size_t cap);

/* Send a text message. Returns 1 on success. */
int ws_send(const char *text);

/* A short label: "OFF", "CONNECTING", "OPEN", "CLOSED". */
const char *ws_status_text(void);

/* Messages received, and successful connections (1 = the first). Useful
   for a debug overlay; a climbing reconnect count means the server is
   dropping us or the link is flapping. */
unsigned long ws_messages(void);
unsigned long ws_connects(void);

/* The background loop, shared. Runs forever, reconnecting on close. */
void ws_client_loop(void *arg);

/* --- transport hooks, implemented per platform --- */

void *ws_io_connect(const char *host, int port, int tls);
int ws_io_write(void *io, const void *data, size_t len);
/* Blocks until at least one byte, the timeout, or an error. Returns the
   byte count, 0 on timeout, -1 on error/close. */
int ws_io_read(void *io, void *data, size_t len, int timeout_ms);
void ws_io_close(void *io);
void ws_io_random(void *buf, size_t len);
void ws_io_delay(int ms);
/* Run fn(arg) on its own task/thread. */
void ws_task_create(void (*fn)(void *), void *arg);
/* A line for the console (Serial on the board, stderr in the sim). */
void ws_io_log(const char *msg);

#ifdef __cplusplus
}
#endif

#endif
