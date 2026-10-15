#include "ws.h"

#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef WS_SIM_TLS
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

/* The desktop transport: a TCP socket on a background thread, wrapped in
   OpenSSL when the URL is wss://. Without OpenSSL at build time
   (WS_SIM_TLS undefined) only plain ws:// works. Certificate checks are
   off, matching the board's setInsecure(). */

typedef struct {
  int fd;
#ifdef WS_SIM_TLS
  SSL_CTX *ctx;
  SSL *ssl;
#endif
} SimIo;

#ifdef WS_SIM_TLS
static int tls_ready = 0;
#endif

void *ws_io_connect(const char *host, int port, int tls) {
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *rp;
  char portstr[16];
  SimIo *io;
  int fd = -1;

  snprintf(portstr, sizeof portstr, "%d", port);
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host, portstr, &hints, &res) != 0) {
    return NULL;
  }
  for (rp = res; rp; rp = rp->ai_next) {
    fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0) {
      continue;
    }
    if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  if (fd < 0) {
    return NULL;
  }

  io = (SimIo *)calloc(1, sizeof *io);
  io->fd = fd;

#ifdef WS_SIM_TLS
  if (tls) {
    if (!tls_ready) {
      OPENSSL_init_ssl(0, NULL);
      tls_ready = 1;
    }
    io->ctx = SSL_CTX_new(TLS_client_method());
    if (!io->ctx) {
      close(fd);
      free(io);
      return NULL;
    }
    SSL_CTX_set_verify(io->ctx, SSL_VERIFY_NONE, NULL);
    io->ssl = SSL_new(io->ctx);
    SSL_set_fd(io->ssl, fd);
    SSL_set_tlsext_host_name(io->ssl, host);
    if (SSL_connect(io->ssl) != 1) {
      SSL_free(io->ssl);
      SSL_CTX_free(io->ctx);
      close(fd);
      free(io);
      return NULL;
    }
  }
#else
  if (tls) {
    close(fd);
    free(io);
    return NULL;
  }
#endif

  return io;
}

int ws_io_write(void *p, const void *data, size_t len) {
  SimIo *io = (SimIo *)p;
  const char *b = (const char *)data;
  size_t sent = 0;

  while (sent < len) {
#ifdef WS_SIM_TLS
    if (io->ssl) {
      int n = SSL_write(io->ssl, b + sent, (int)(len - sent));
      if (n <= 0) {
        return -1;
      }
      sent += (size_t)n;
      continue;
    }
#endif
    {
      ssize_t n = send(io->fd, b + sent, len - sent, 0);
      if (n <= 0) {
        return -1;
      }
      sent += (size_t)n;
    }
  }
  return (int)sent;
}

int ws_io_read(void *p, void *data, size_t len, int timeout_ms) {
  SimIo *io = (SimIo *)p;
  struct timeval start;
  struct timeval now;

  gettimeofday(&start, NULL);

  for (;;) {
    fd_set rf;
    struct timeval tv;
    int elapsed;
    int r;

#ifdef WS_SIM_TLS
    /* A TLS record can hold more than one frame; drain what OpenSSL has
       buffered before waiting on the socket again. */
    if (io->ssl && SSL_pending(io->ssl) > 0) {
      int n = SSL_read(io->ssl, data, (int)len);
      if (n > 0) {
        return n;
      }
      {
        int e = SSL_get_error(io->ssl, n);
        if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
          continue;
        }
      }
      return -1;
    }
#endif

    gettimeofday(&now, NULL);
    elapsed = (int)((now.tv_sec - start.tv_sec) * 1000 +
                    (now.tv_usec - start.tv_usec) / 1000);
    if (elapsed >= timeout_ms) {
      return 0;
    }

    FD_ZERO(&rf);
    FD_SET(io->fd, &rf);
    tv.tv_sec = (timeout_ms - elapsed) / 1000;
    tv.tv_usec = ((timeout_ms - elapsed) % 1000) * 1000;

    r = select(io->fd + 1, &rf, NULL, NULL, &tv);
    if (r < 0) {
      return -1;
    }
    if (r == 0) {
      return 0;
    }

#ifdef WS_SIM_TLS
    if (io->ssl) {
      int n = SSL_read(io->ssl, data, (int)len);
      if (n > 0) {
        return n;
      }
      {
        int e = SSL_get_error(io->ssl, n);
        if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
          continue;
        }
      }
      return -1;
    }
#endif
    {
      ssize_t n = recv(io->fd, data, len, 0);
      if (n <= 0) {
        return -1;
      }
      return (int)n;
    }
  }
}

void ws_io_close(void *p) {
  SimIo *io = (SimIo *)p;

  if (!io) {
    return;
  }
#ifdef WS_SIM_TLS
  if (io->ssl) {
    SSL_shutdown(io->ssl);
    SSL_free(io->ssl);
  }
  if (io->ctx) {
    SSL_CTX_free(io->ctx);
  }
#endif
  close(io->fd);
  free(io);
}

void ws_io_random(void *buf, size_t len) {
  FILE *f = fopen("/dev/urandom", "rb");

  if (f) {
    if (fread(buf, 1, len, f) == len) {
      fclose(f);
      return;
    }
    fclose(f);
  }
  {
    size_t i;
    for (i = 0; i < len; i++) {
      ((unsigned char *)buf)[i] = (unsigned char)rand();
    }
  }
}

void ws_io_delay(int ms) { usleep((useconds_t)ms * 1000); }

typedef struct {
  void (*fn)(void *);
  void *arg;
} TaskArg;

static void *task_tramp(void *p) {
  TaskArg *ta = (TaskArg *)p;
  ta->fn(ta->arg);
  return NULL;
}

void ws_task_create(void (*fn)(void *), void *arg) {
  TaskArg *ta = (TaskArg *)malloc(sizeof *ta);
  pthread_t t;

  ta->fn = fn;
  ta->arg = arg;
  if (pthread_create(&t, NULL, task_tramp, ta) == 0) {
    pthread_detach(t);
  }
}

void ws_io_log(const char *msg) { fprintf(stderr, "%s\n", msg); }
