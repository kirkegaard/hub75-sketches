#ifndef HUB75_NETDEBUG_H
#define HUB75_NETDEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/* A small status overlay for the panel: reset reason, free heap, and the
   net/ws state and counters. It is drawn on top of the sketch every
   frame by the firmware and the simulator when they are built with
   NETDEBUG=1, so no sketch has to carry its own debug screen. */

void net_debug_draw(void);

#ifdef __cplusplus
}
#endif

#endif
