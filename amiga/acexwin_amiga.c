/* The Amiga under amiga/acexwin.c: ACGame's 8-bit screen (AGA, or RTG and
 * GFOpenGfx where there is one; no SDL, no GFOpenGPU needed), its pointer
 * and buttons, the keys through keymap.library (so the Amiga's own keymap
 * gives the letters), and topaz 8 for the text.
 *
 * The table: 800 x 600 unless ENV:AceOfPenguins/Display names another size
 * (for example 640x480); ACGame opens the nearest screen mode that holds it
 * and centres the table on it.
 *
 * The mouse: the left button is X's button 1, the right button X's 3; with
 * Shift held a click is one more (2 or 4), as upstream's xwin.c makes it.
 * The keys: what the keymap gives, the cursor keys, Del and Backspace,
 * F1-F10, Help as F1 (the help page), and Esc.
 *
 * Copyright (c) 2026 AmiCradle Team. GPL-2.0-or-later, like Ace of Penguins. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <devices/inputevent.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/keymap.h>

#include <acgame/platform.h>

#include "cards.h"
#include "acexwin.h"

struct Library *KeymapBase;

int acx_font_w = 8, acx_font_h = 8, acx_font_base = 6;
static unsigned short glyphs[256][16];
static acgame_indexed_surface *surface;
static acgame_input_state last;
static uint8_t keys_before[ACGAME_RAWKEY_COUNT];
static int last_x = -1, last_y = -1, button_down;

/* topaz 8 drawn once into a plane, one cell a character */
static void make_font(void)
{
	struct TextAttr ta = { (STRPTR)"topaz.font", 8, 0, 0 };
	struct TextFont *tf = OpenFont(&ta);
	struct BitMap bm;
	struct RastPort rp;
	PLANEPTR plane;
	int w, h;

	if (!tf) return;
	w = tf->tf_XSize > 16 ? 16 : tf->tf_XSize;
	h = tf->tf_YSize > 16 ? 16 : tf->tf_YSize;
	plane = AllocRaster(16, h);
	if (!plane) { CloseFont(tf); return; }
	InitBitMap(&bm, 1, 16, h);
	bm.Planes[0] = plane;
	InitRastPort(&rp);
	rp.BitMap = &bm;
	SetFont(&rp, tf);
	SetAPen(&rp, 1);
	SetDrMd(&rp, JAM1);
	for (int c = 32; c < 256; c++) {
		char ch = (char)c;
		if (c >= 127 && c < 160) continue;
		BltClear(plane, (ULONG)bm.BytesPerRow * h, 0);
		Move(&rp, 0, tf->tf_Baseline);
		Text(&rp, (STRPTR)&ch, 1);
		WaitBlit();
		for (int r = 0; r < h; r++) {
			const UBYTE *row = (const UBYTE *)plane + r * bm.BytesPerRow;
			glyphs[c][r] = (unsigned short)((row[0] << 8) | row[1]);
		}
	}
	acx_font_w = w;
	acx_font_h = h;
	acx_font_base = tf->tf_Baseline + 1;
	FreeRaster(plane, 16, h);
	CloseFont(tf);
}

const unsigned short *acx_plat_glyph(int c)
{
	return glyphs[c & 255];
}

void acx_plat_display_size(int *width, int *height)
{
	char buf[32];
	int w = 800, h = 600;

	if (GetVar((STRPTR)"AceOfPenguins/Display", (STRPTR)buf, sizeof buf, 0) > 0) {
		int a, b;
		if (sscanf(buf, "%dx%d", &a, &b) == 2 && a >= 320 && b >= 200) { w = a; h = b; }
	}
	*width = w;
	*height = h;
	make_font(); /* xwin_init asks once, before it reads the font's size */
}

int acx_plat_open(int width, int height, const unsigned char rgb[256][3],
                  unsigned char **pixels, int *pitch)
{
	acgame_video_config cfg;

	KeymapBase = OpenLibrary((CONST_STRPTR)"keymap.library", 37);
	memset(&cfg, 0, sizeof cfg);
	cfg.width = (unsigned)width;
	cfg.height = (unsigned)height;
	cfg.refresh_hz = 50;
	cfg.mode = ACGAME_VIDEO_INDEXED8;
	cfg.display = ACGAME_DISPLAY_AUTO;
	if (acgame_platform_init(&cfg)) return -1;
	acgame_platform_show_pointer(1);
	surface = acgame_platform_begin_indexed_frame();
	if (!surface || !surface->pixels) return -1;
	for (int i = 0; i < 256; i++) {
		surface->palette[i].r = rgb[i][0];
		surface->palette[i].g = rgb[i][1];
		surface->palette[i].b = rgb[i][2];
	}
	*pixels = surface->pixels;
	*pitch = (int)surface->pitch;
	acgame_platform_poll(&last);
	memcpy(keys_before, acgame_platform_rawkeys(), sizeof keys_before);
	return 0;
}

void acx_plat_present(int y0, int y1)
{
	(void)y0; (void)y1;
	if (surface) acgame_platform_present_indexed(surface);
}

void acx_plat_close(void)
{
	if (surface) {
		surface = 0;
		acgame_platform_shutdown();
	}
	if (KeymapBase) {
		CloseLibrary(KeymapBase);
		KeymapBase = 0;
	}
}

unsigned long acx_plat_ms(void) { return acgame_platform_ticks_ms(); }

void acx_plat_wait(void) { Delay(1); }

void acx_plat_beep(void) { DisplayBeep(NULL); }

static int shifted(const uint8_t *k) { return k[0x60] || k[0x61]; }

/* a raw key going down as Ace's key: 0 for none */
static int key_for(int code, const uint8_t *k)
{
	struct InputEvent ie;
	char buf[8];

	switch (code) {
	case 0x4c: return KEY_UP;
	case 0x4d: return KEY_DOWN;
	case 0x4e: return KEY_RIGHT;
	case 0x4f: return KEY_LEFT;
	case 0x46: case 0x41: return KEY_DELETE;
	case 0x5f: return KEY_F(1); /* Help */
	case 0x45: return 27;
	case 0x44: case 0x43: return '\r';
	}
	if (code >= 0x50 && code <= 0x59) return KEY_F(code - 0x50 + 1);
	if (!KeymapBase || code >= 0x60) return 0;
	memset(&ie, 0, sizeof ie);
	ie.ie_Class = IECLASS_RAWKEY;
	ie.ie_Code = (UWORD)code;
	ie.ie_Qualifier = (UWORD)((shifted(k) ? IEQUALIFIER_LSHIFT : 0) |
	                          (k[0x63] ? IEQUALIFIER_CONTROL : 0) |
	                          (k[0x62] ? IEQUALIFIER_CAPSLOCK : 0));
	if (MapRawKey(&ie, (STRPTR)buf, sizeof buf, NULL) == 1) return (unsigned char)buf[0];
	return 0;
}

int acx_plat_event(XWin_Event *ev)
{
	acgame_input_state now;
	const uint8_t *k;
	int x = 0, y = 0;

	acgame_platform_poll(&now);
	acgame_platform_pointer(&x, &y);
	k = acgame_platform_rawkeys();
	memset(ev, 0, sizeof *ev);
	ev->x = x;
	ev->y = y;
	ev->time = (int)acgame_platform_ticks_ms();

	/* the buttons, one change an event */
	if ((now.mouse_buttons & 1) != (last.mouse_buttons & 1) ||
	    (now.mouse_buttons & 2) != (last.mouse_buttons & 2)) {
		int changed = (now.mouse_buttons ^ last.mouse_buttons) & 1 ? 1 : 2;
		int down = now.mouse_buttons & changed;
		last.mouse_buttons = (uint8_t)((last.mouse_buttons & ~changed) | down);
		if (down) {
			button_down = (changed == 1 ? 1 : 3) + (shifted(k) ? 1 : 0);
			ev->type = ev_buttondown;
		} else {
			ev->type = ev_buttonup;
		}
		ev->button = button_down;
		last_x = x;
		last_y = y;
		return 1;
	}
	if (last.mouse_buttons && (x != last_x || y != last_y)) {
		last_x = x;
		last_y = y;
		ev->type = ev_motion;
		ev->button = button_down;
		return 1;
	}
	for (int c = 0; c < ACGAME_RAWKEY_COUNT; c++) {
		if (k[c] && !keys_before[c]) {
			int key;
			keys_before[c] = 1;
			key = key_for(c, k);
			if (key) {
				ev->type = ev_keypress;
				ev->key = key;
				return 1;
			}
		} else if (!k[c]) {
			keys_before[c] = 0;
		}
	}
	return 0;
}
