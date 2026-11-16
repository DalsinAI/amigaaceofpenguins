/* The platform under amiga/acexwin.c for the host tests: the frame is a
 * buffer in memory, the events come from ACE_EVENTS, the clock moves 20 ms
 * a wait, and the frame is written as a PPM (ACE_DUMP) when the game ends.
 *
 * The clock moves 1 ms each time it is read (lib/stack.c spins on it).
 *
 * ACE_EVENTS is a list of events, separated by spaces:
 *   c:X,Y      a click of button 1 (down and up) at X,Y
 *   d:X,Y,U,V  button 1 down at X,Y, moved to U,V, up there
 *   k:N        the key N (a number: 27 Esc, 113 q, 257 F1, ...)
 *   w          nothing for a while (animations finish)
 *   s          the frame written to ACE_DUMP now (else it is written at the end)
 * After the last one the game gets Esc, and then the window's close.
 * Copyright (c) 2026 AmiCradle Team. GPL-2.0-or-later, like Ace of Penguins. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acexwin.h"

int acx_font_w = 8, acx_font_h = 8, acx_font_base = 6;
static unsigned short block[16] = { 0, 0x7e00, 0x4200, 0x4200, 0x4200, 0x4200, 0x7e00, 0 };
static unsigned char *fb;
static int fw, fh;
static unsigned char pal[256][3];
static unsigned long now;
static XWin_Event queue[64];
static int qn, qi, ended;
static const char *script;
static int dumped;
static void dump(void);

void acx_plat_display_size(int *w, int *h) { *w = 800; *h = 600; }

const unsigned short *acx_plat_glyph(int c) { return c > ' ' ? block : 0; }

static void add(int type, int x, int y, int button, int key)
{
	if (qn >= 64) return;
	memset(&queue[qn], 0, sizeof queue[qn]);
	queue[qn].type = type; queue[qn].x = x; queue[qn].y = y;
	queue[qn].button = button; queue[qn].key = key;
	qn++;
}

/* the next item of ACE_EVENTS as queued events; 0 when there are none */
static int refill(void)
{
	int a, b, c, d, n = 0;

	qn = qi = 0;
	while (script && *script == ' ') script++;
	if (!script || !*script) return 0;
	if (sscanf(script, "c:%d,%d%n", &a, &b, &n) == 2) {
		add(ev_buttondown, a, b, 1, 0); add(ev_buttonup, a, b, 1, 0);
	} else if (sscanf(script, "d:%d,%d,%d,%d%n", &a, &b, &c, &d, &n) == 4) {
		add(ev_buttondown, a, b, 1, 0);
		add(ev_motion, (a + c) / 2, (b + d) / 2, 1, 0);
		add(ev_motion, c, d, 1, 0);
		add(ev_buttonup, c, d, 1, 0);
	} else if (sscanf(script, "k:%d%n", &a, &n) == 1) {
		add(ev_keypress, 0, 0, 0, a);
	} else if (*script == 'w') {
		n = 1;
		add(ev_none, 0, 0, 0, 0);
	} else if (*script == 's') {
		n = 1;
		add(ev_none, 0, 0, 0, 's');
	} else {
		fprintf(stderr, "ACE_EVENTS: cannot read \"%s\"\n", script);
		exit(2);
	}
	script += n;
	return 1;
}

static void dump(void)
{
	const char *path = getenv("ACE_DUMP");
	FILE *f;

	if (!path || !fb || dumped) return;
	f = fopen(path, "wb");
	if (!f) return;
	fprintf(f, "P6\n%d %d\n255\n", fw, fh);
	for (int i = 0; i < fw * fh; i++) fwrite(pal[fb[i]], 1, 3, f);
	fclose(f);
	dumped = 1;
}

int acx_plat_open(int w, int h, const unsigned char rgb[256][3], unsigned char **pixels, int *pitch)
{
	fb = (unsigned char *)calloc((size_t)w * h, 1);
	if (!fb) return -1;
	fw = w; fh = h;
	memcpy(pal, rgb, sizeof pal);
	*pixels = fb;
	*pitch = w;
	script = getenv("ACE_EVENTS");
	atexit(dump);
	return 0;
}

void acx_plat_present(int y0, int y1)
{
	if (y0 < 0 || y1 > fh || y0 >= y1) {
		fprintf(stderr, "present: rows %d-%d outside the frame (%d)\n", y0, y1, fh);
		exit(3);
	}
}

int acx_plat_event(XWin_Event *ev)
{
	while (qi >= qn) {
		if (!refill()) {
			/* the end: Esc, then (if the game is still there) the close */
			memset(ev, 0, sizeof *ev);
			if (ended++ == 0) { ev->type = ev_keypress; ev->key = 27; }
			else ev->type = ev_quit;
			ev->time = (int)now;
			return 1;
		}
	}
	*ev = queue[qi++];
	now += 300; /* far enough apart not to be double clicks */
	ev->time = (int)now;
	if (ev->type == ev_none) {
		if (ev->key == 's') { extern void flush(void); flush(); dump(); }
		return 0;
	}
	return 1;
}

void acx_plat_wait(void) { now += 20; }
/* lib/stack.c waits for the clock to tick over in a loop: every look at
 * it moves it on a millisecond, as a real clock would */
unsigned long acx_plat_ms(void) { return ++now; }
void acx_plat_beep(void) {}
void acx_plat_close(void) {}
