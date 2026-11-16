/* Between amiga/acexwin.c (Ace of Penguins' xwin.c for the Amiga: the
 * frame, the pictures, the drawing calls, all on 8-bit pixels) and the
 * platform under it: amiga/acexwin_amiga.c (ACGame's screen, its mouse and
 * keys, topaz 8) or tests/host/acexwin_host.c (a frame in memory, events
 * from the test).
 * Copyright (c) 2026 AmiCradle Team. GPL-2.0-or-later, like Ace of Penguins. */
#ifndef ACEXWIN_H
#define ACEXWIN_H

#include "xwin.h"

/* The palette: 0-215 the 6x6x6 colour cube (index r*36 + g*6 + b, each
 * 0-5 for 0, 51, ... 255), 216-255 greys between the cube's. */
#define ACX_CUBE 216
void acx_palette(unsigned char rgb[256][3]);
int acx_nearest(int r, int g, int b);

/* The platform's frame: width x height pixels, one byte each, pitch bytes
 * a row. 0 when it opened. */
int acx_plat_open(int width, int height, const unsigned char rgb[256][3],
                  unsigned char **pixels, int *pitch);
/* Show the frame (the rows y0 to y1-1 changed). */
void acx_plat_present(int y0, int y1);
/* The next event when there is one (1), else 0. Never blocks. Sets
 * ev->type to ev_buttondown, ev_buttonup, ev_motion, ev_keypress or
 * ev_quit, with x, y, button, key and time as xwin.h has them. */
int acx_plat_event(XWin_Event *ev);
/* Wait a little (a frame) for input. */
void acx_plat_wait(void);
unsigned long acx_plat_ms(void);
void acx_plat_beep(void);
void acx_plat_close(void);
/* The largest table the screen holds. */
void acx_plat_display_size(int *width, int *height);

/* The font: a fixed cell of font_w x font_h, the baseline font_base rows
 * from its top; acx_plat_glyph gives the cell's rows, bit 15 the left
 * pixel. */
extern int acx_font_w, acx_font_h, acx_font_base;
const unsigned short *acx_plat_glyph(int c);

#endif
