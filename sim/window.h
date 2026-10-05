#ifndef HUB75_WINDOW_H
#define HUB75_WINDOW_H

struct Hub75Panel;

int sim_button_up(void);
int sim_button_down(void);

/* Open the LED window and run setup's draw loop at 30 frames per second
   until the window closes. setup() has already been called. */
void sim_window_run(const char *title, struct Hub75Panel *panel);

#endif
