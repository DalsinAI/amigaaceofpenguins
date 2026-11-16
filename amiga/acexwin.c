/* Ace of Penguins' lib/xwin.c for the Amiga: the same calls (xwin.h,
 * cards.h's clear, text, flush and beep, imagelib.h's put_image, put_mask
 * and fill_image), drawn into an 8-bit frame instead of an X window, and
 * the handful of Xlib calls lib/help.c makes (amiga/include/X11/Xlib.h).
 *
 * The frame is the platform's (amiga/acexwin.h): ACGame's 8-bit screen on
 * the Amiga, a buffer in memory in the host tests. Its palette is fixed: a
 * 6x6x6 colour cube and 40 greys, so a picture's colours map once, when it
 * is first drawn, and every picture shares the one palette. A picture is
 * its pixels (palette indexes) and, when the PNG has transparency, a mask
 * (a byte a pixel, 0 = not drawn). Everything drawn on the frame marks the
 * rows it changed; flush() shows them.
 *
 * No X, no rotation (-rotate is gone: the Amiga's screens are landscape).
 * Upstream's lib/xwin.c is not built.
 *
 * Copyright (C) 1998, 2001 DJ Delorie (lib/xwin.c, which this follows).
 * Copyright (c) 2026 AmiCradle Team. GPL-2.0-or-later, like Ace of Penguins. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/time.h>

#include <png.h>

#include <X11/Xlib.h>

typedef struct {
	unsigned char *pix;      /* width x height palette indexes */
	unsigned char *mask;     /* width x height, 0 = transparent; 0 when opaque */
	unsigned char *inverted; /* pix with every colour inverted, made when asked */
	unsigned char *rot;      /* turned half round, made when asked */
	unsigned char *rotmask;
} pixels_type;
#define PIXELS_TYPE pixels_type *

#include "imagelib.h"
#include "cards.h"
#include "xwin.h"
#include "acexwin.h"

/* What the rest of Ace reads (lib/table.c has xwin_options: no -rotate,
 * no -visual here, so it stays 0) */
int font_width, font_height;
int table_background;
int help_background;
image *display_image;
/* lib/help.c's X handles: only their addresses matter */
struct _ace_display { int unused; };
struct _ace_gc { int foreground; };
static struct _ace_display the_display;
static struct _ace_gc the_gc;
Display *display = &the_display;
int screen = 0;
Window window = 1;
GC gc = &the_gc;

static image_list display_image_list;
static image display_image_data;
static pixels_type display_pixels;
static unsigned char *frame;
static int frame_w, frame_h, frame_pitch;
static int dirty_y0, dirty_y1; /* rows to show: y0 <= y < y1 */
static int have_clip, clip_x, clip_y, clip_w, clip_h;
static unsigned char palette[256][3];

extern int help_is_showing;

/* ---- the palette ---- */

static const unsigned char grey_levels[40] = {
	/* between the cube's 0, 51, 102, 153, 204, 255: eight in each gap */
	6, 11, 17, 23, 28, 34, 40, 45,
	57, 62, 68, 74, 79, 85, 91, 96,
	108, 113, 119, 125, 130, 136, 142, 147,
	159, 164, 170, 176, 181, 187, 193, 198,
	210, 215, 221, 227, 232, 238, 244, 249
};

void acx_palette(unsigned char rgb[256][3])
{
	int i;

	for (i = 0; i < ACX_CUBE; i++) {
		rgb[i][0] = (unsigned char)(i / 36 * 51);
		rgb[i][1] = (unsigned char)(i / 6 % 6 * 51);
		rgb[i][2] = (unsigned char)(i % 6 * 51);
	}
	for (i = 0; i < 40; i++) {
		rgb[ACX_CUBE + i][0] = rgb[ACX_CUBE + i][1] = rgb[ACX_CUBE + i][2] = grey_levels[i];
	}
}

static int cube_step(int v) { return (v + 25) / 51; }

int acx_nearest(int r, int g, int b)
{
	int ci = cube_step(r) * 36 + cube_step(g) * 6 + cube_step(b);
	int cr = cube_step(r) * 51, cg = cube_step(g) * 51, cb = cube_step(b) * 51;
	long best = (long)(r - cr) * (r - cr) + (long)(g - cg) * (g - cg) + (long)(b - cb) * (b - cb);
	int y = (r * 77 + g * 150 + b * 29) >> 8, i, gi = -1;

	/* a grey (or a colour close to one) may be nearer one of the 40 */
	for (i = 0; i < 40; i++) {
		int d = grey_levels[i] - y;
		long e = (long)(r - grey_levels[i]) * (r - grey_levels[i]) +
		         (long)(g - grey_levels[i]) * (g - grey_levels[i]) +
		         (long)(b - grey_levels[i]) * (b - grey_levels[i]);
		if (d > 40 || d < -40) continue;
		if (e < best) { best = e; gi = i; }
	}
	return gi >= 0 ? ACX_CUBE + gi : ci;
}

int pixel_for(int r, int g, int b)
{
	return acx_nearest(r, g, b);
}

static int invert_index(int p)
{
	return acx_nearest(255 - palette[p][0], 255 - palette[p][1], 255 - palette[p][2]);
}

/* ---- the frame ---- */

static void mark(int y, int h)
{
	if (h <= 0) return;
	if (dirty_y1 <= dirty_y0) { dirty_y0 = y; dirty_y1 = y + h; return; }
	if (y < dirty_y0) dirty_y0 = y;
	if (y + h > dirty_y1) dirty_y1 = y + h;
}

int xwin_init(int argc, char **argv)
{
	(void)argc; (void)argv;
	acx_palette(palette);
	acx_plat_display_size(&display_width, &display_height);
	table_type = TABLE_COLOR;
	table_background = pixel_for(0x00, 0x66, 0x00);
	font_width = acx_font_w;
	font_height = acx_font_h;
	return 0;
}

void xwin_create(int width, int height)
{
	width = (width + 7) & ~7; /* ACGame's frames are a multiple of 8 wide */
	if (acx_plat_open(width, height, (const unsigned char (*)[3])palette,
	                  &frame, &frame_pitch)) {
		fprintf(stderr, "Ace of Penguins: no screen of %dx%d\n", width, height);
		exit(20);
	}
	frame_w = width;
	frame_h = height;
	memset(frame, table_background, (size_t)frame_pitch * frame_h);
	mark(0, frame_h);

	display_pixels.pix = frame;
	display_image_data.width = width;
	display_image_data.height = height;
	display_image_data.list = &display_image_list;
	display_image_data.pixels = &display_pixels;
	display_image_list.name = "Amiga screen";
	display_image_list.across = 1;
	display_image_list.down = 1;
	display_image = &display_image_data;
	atexit(acx_plat_close);
}

void xwin_fixed_size(int width, int height)
{
	(void)width; (void)height; /* the screen is the size it is */
}

void flush(void)
{
	if (dirty_y1 > dirty_y0) {
		acx_plat_present(dirty_y0, dirty_y1);
		dirty_y0 = dirty_y1 = 0;
	}
}

void flushsync(void) { flush(); }

void beep(void) { acx_plat_beep(); }

void xwin_clip(int x, int y, int w, int h)
{
	have_clip = 1;
	clip_x = x; clip_y = y; clip_w = w; clip_h = h;
}

void xwin_noclip(void) { have_clip = 0; }

/* The part of (x, y, w, h) inside the frame (and the clip): 0 if none. */
static int frame_rect(int *x, int *y, int *w, int *h, int use_clip)
{
	int x0 = 0, y0 = 0, x1 = frame_w, y1 = frame_h;

	if (use_clip && have_clip) {
		if (clip_x > x0) x0 = clip_x;
		if (clip_y > y0) y0 = clip_y;
		if (clip_x + clip_w < x1) x1 = clip_x + clip_w;
		if (clip_y + clip_h < y1) y1 = clip_y + clip_h;
	}
	if (*x < x0) { *w -= x0 - *x; *x = x0; }
	if (*y < y0) { *h -= y0 - *y; *y = y0; }
	if (*x + *w > x1) *w = x1 - *x;
	if (*y + *h > y1) *h = y1 - *y;
	return *w > 0 && *h > 0;
}

static void frame_fill(int x, int y, int w, int h, int p)
{
	if (!frame || !frame_rect(&x, &y, &w, &h, 1)) return;
	for (int r = 0; r < h; r++)
		memset(frame + (size_t)(y + r) * frame_pitch + x, p, (size_t)w);
	mark(y, h);
}

void clear(int x, int y, int w, int h)
{
	frame_fill(x, y, w, h, help_is_showing ? help_background : table_background);
}

/* ---- text: the platform's font, foreground on no background ---- */

static void draw_glyphs(const char *s, int n, int x, int baseline, int fg, int bg)
{
	int top = baseline - acx_font_base;

	for (int i = 0; i < n; i++, x += acx_font_w) {
		const unsigned short *rows = acx_plat_glyph((unsigned char)s[i]);
		for (int r = 0; r < acx_font_h; r++) {
			int yy = top + r;
			for (int c = 0; c < acx_font_w; c++) {
				int xx = x + c, on = rows && (rows[r] & (0x8000u >> c));
				int px = xx, py = yy, pw = 1, ph = 1;
				if (!on && bg < 0) continue;
				if (!frame_rect(&px, &py, &pw, &ph, 1)) continue;
				frame[(size_t)py * frame_pitch + px] = (unsigned char)(on ? fg : bg);
			}
		}
	}
	mark(top, acx_font_h);
}

/* lower left corner, white on the table, as upstream's XDrawImageString */
void text(char *t, int x, int y)
{
	draw_glyphs(t, (int)strlen(t), x, y - (acx_font_h - acx_font_base), pixel_for(255, 255, 255),
	            table_background);
}

/* ---- the Xlib calls lib/help.c makes ---- */

static XFontStruct the_font;

XFontStruct *XLoadQueryFont(Display *d, const char *name)
{
	(void)d; (void)name;
	the_font.fid = 1;
	the_font.ascent = acx_font_base;
	the_font.descent = acx_font_h - acx_font_base;
	the_font.max_bounds.width = (short)acx_font_w;
	the_font.max_bounds.ascent = (short)the_font.ascent;
	the_font.max_bounds.descent = (short)the_font.descent;
	the_font.max_bounds.lbearing = 0;
	the_font.max_bounds.rbearing = (short)acx_font_w;
	return &the_font;
}

int XTextWidth(XFontStruct *f, const char *s, int n)
{
	(void)f; (void)s;
	return n * acx_font_w;
}

int XTextExtents(XFontStruct *f, const char *s, int n, int *dir, int *ascent,
                 int *descent, XCharStruct *overall)
{
	*dir = 0;
	*ascent = f->ascent;
	*descent = f->descent;
	memset(overall, 0, sizeof *overall);
	overall->width = (short)XTextWidth(f, s, n);
	overall->rbearing = overall->width;
	overall->ascent = (short)f->ascent;
	overall->descent = (short)f->descent;
	return 0;
}

int XSetForeground(Display *d, GC g, unsigned long pixel)
{
	(void)d;
	g->foreground = (int)pixel;
	return 0;
}

int XSetFont(Display *d, GC g, Font font) { (void)d; (void)g; (void)font; return 0; }

int XDrawString(Display *d, Window w, GC g, int x, int y, const char *s, int n)
{
	(void)d; (void)w;
	draw_glyphs(s, n, x, y, g->foreground, -1);
	return 0;
}

int XFillRectangle(Display *d, Window w, GC g, int x, int y,
                   unsigned int width, unsigned int height)
{
	(void)d; (void)w;
	frame_fill(x, y, (int)width, (int)height, g->foreground);
	return 0;
}

int XDrawPoint(Display *d, Window w, GC g, int x, int y)
{
	(void)d; (void)w;
	frame_fill(x, y, 1, 1, g->foreground);
	return 0;
}

int XDrawLine(Display *d, Window w, GC g, int x1, int y1, int x2, int y2)
{
	int dx = abs(x2 - x1), dy = -abs(y2 - y1);
	int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, err = dx + dy;

	(void)d; (void)w;
	for (;;) { /* Bresenham, both ends drawn as X draws them */
		frame_fill(x1, y1, 1, 1, g->foreground);
		if (x1 == x2 && y1 == y2) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x1 += sx; }
		if (e2 <= dx) { err += dx; y1 += sy; }
	}
	return 0;
}

/* ---- pictures ---- */

typedef struct { const unsigned char *p; } png_src;

static void png_reader(png_structp png, png_bytep data, png_size_t length)
{
	png_src *src = (png_src *)png_get_io_ptr(png);
	memcpy(data, src->p, length);
	src->p += length;
}

/* The picture's PNG (make-imglib put its bytes in file_data) as palette
 * indexes and a mask. 0 when it decoded. */
static int decode_png(image *img)
{
	png_structp png;
	png_infop info;
	png_src src;
	png_bytep *rows = 0;
	unsigned char *rgba = 0;
	png_uint_32 w, h;
	int has_alpha = 0, ok = -1;

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	info = png ? png_create_info_struct(png) : 0;
	if (!info) goto out;
	if (setjmp(png_jmpbuf(png))) {
		fprintf(stderr, "Invalid PNG image %s\n", img->list ? img->list->name : "");
		ok = -1;
		goto out;
	}
	src.p = img->file_data;
	png_set_read_fn(png, &src, png_reader);
	png_read_info(png, info);
	/* everything as 8-bit RGBA */
	png_set_expand(png);
	png_set_strip_16(png);
	png_set_gray_to_rgb(png);
	png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	png_set_interlace_handling(png);
	png_read_update_info(png, info);
	w = png_get_image_width(png, info);
	h = png_get_image_height(png, info);
	rgba = (unsigned char *)malloc((size_t)w * h * 4);
	rows = (png_bytep *)malloc(h * sizeof *rows);
	if (!rgba || !rows) goto out;
	for (png_uint_32 y = 0; y < h; y++) rows[y] = rgba + (size_t)y * w * 4;
	png_read_image(png, rows);
	png_read_end(png, 0);

	for (size_t i = 0; i < (size_t)w * h; i++)
		if (rgba[i * 4 + 3] <= 128) { has_alpha = 1; break; }
	/* the picture keeps the size make-imglib wrote, which is the PNG's */
	img->pixels->pix = (unsigned char *)malloc((size_t)w * h);
	if (!img->pixels->pix) goto out;
	if (has_alpha) img->pixels->mask = (unsigned char *)malloc((size_t)w * h);
	for (size_t i = 0; i < (size_t)w * h; i++) {
		const unsigned char *q = rgba + i * 4;
		img->pixels->pix[i] = (unsigned char)acx_nearest(q[0], q[1], q[2]);
		if (img->pixels->mask) img->pixels->mask[i] = q[3] > 128;
	}
	ok = 0;
out:
	if (png) png_destroy_read_struct(&png, info ? &info : 0, 0);
	free(rows);
	free(rgba);
	return ok;
}

static void build_image(image *img)
{
	img->pixels = (pixels_type *)calloc(1, sizeof(pixels_type));
	if (!img->pixels) return;
	if (img->synth_func || !img->file_data) {
		/* a picture the game draws itself (synth), or an empty one */
		img->pixels->pix = (unsigned char *)calloc((size_t)img->width * img->height, 1);
		if (img->synth_func) img->synth_func(img);
		return;
	}
	if (decode_png(img)) {
		free(img->pixels->pix);
		img->pixels->pix = 0;
	}
}

static unsigned char *rotated(const unsigned char *p, int w, int h)
{
	unsigned char *r;

	if (!p) return 0;
	r = (unsigned char *)malloc((size_t)w * h);
	if (!r) return 0;
	for (int i = 0; i < w * h; i++) r[i] = p[w * h - 1 - i];
	return r;
}

void put_image(image *src, int x, int y, int w, int h,
               image *dest, int dx, int dy, int flags)
{
	const unsigned char *pix, *mask;
	unsigned char *to;
	int sw = src->width, dw = dest->width, dh = dest->height, onframe = dest == display_image;
	int tx, ty, tw, th;

	if (!src->pixels) build_image(src);
	if (!dest->pixels) build_image(dest);
	if (!src->pixels || !src->pixels->pix || !dest->pixels || !dest->pixels->pix) return;
	pix = src->pixels->pix;
	mask = src->pixels->mask;

	if (flags & PUT_ROTATED) {
		/* upstream: the picture turned half round, the same part of it
		 * where the turned one's would be */
		int sh = src->height, nx = sw - x - w, ny = sh - y - h;
		if (!src->pixels->rot) {
			src->pixels->rot = rotated(src->pixels->pix, sw, sh);
			src->pixels->rotmask = rotated(src->pixels->mask, sw, sh);
		}
		if (!src->pixels->rot) return;
		pix = src->pixels->rot;
		mask = src->pixels->rotmask;
		dx += x - nx;
		dy += y - ny;
		x = nx;
		y = ny;
	}
	if (flags & PUT_INVERTED) {
		if (!src->pixels->inverted) {
			int n = src->width * src->height;
			src->pixels->inverted = (unsigned char *)malloc((size_t)n);
			if (!src->pixels->inverted) return;
			for (int i = 0; i < n; i++)
				src->pixels->inverted[i] = (unsigned char)invert_index(src->pixels->pix[i]);
		}
		pix = src->pixels->inverted;
	}

	/* where it lands: (dx+x, dy+y), w x h, inside dest (and the clip) */
	tx = dx + x; ty = dy + y; tw = w; th = h;
	if (onframe) {
		if (!frame_rect(&tx, &ty, &tw, &th, 1)) return;
	} else {
		if (tx < 0) { tw += tx; tx = 0; }
		if (ty < 0) { th += ty; ty = 0; }
		if (tx + tw > dw) tw = dw - tx;
		if (ty + th > dh) th = dh - ty;
		if (tw <= 0 || th <= 0) return;
	}
	x = tx - dx; y = ty - dy; /* the matching part of the picture */
	if (x < 0 || y < 0 || x + tw > src->width || y + th > src->height) {
		/* asked for more than the picture has: only what it has */
		int x0 = x < 0 ? -x : 0, y0 = y < 0 ? -y : 0;
		tx += x0; ty += y0; tw -= x0; th -= y0; x += x0; y += y0;
		if (x + tw > src->width) tw = src->width - x;
		if (y + th > src->height) th = src->height - y;
		if (tw <= 0 || th <= 0) return;
	}
	for (int r = 0; r < th; r++) {
		const unsigned char *s = pix + (size_t)(y + r) * sw + x;
		const unsigned char *m = mask ? mask + (size_t)(y + r) * sw + x : 0;
		to = onframe ? frame + (size_t)(ty + r) * frame_pitch + tx
		             : dest->pixels->pix + (size_t)(ty + r) * dw + tx;
		if (!m) {
			memcpy(to, s, (size_t)tw);
		} else {
			for (int c = 0; c < tw; c++)
				if (m[c]) to[c] = s[c];
		}
	}
	if (onframe) mark(ty, th);
}

void put_mask(image *src, int x, int y, int w, int h,
              image *dest, int dx, int dy, int flags)
{
	(void)flags;
	if (!src->pixels) build_image(src);
	if (!dest->pixels) build_image(dest);
	if (!src->pixels || !src->pixels->mask || !dest->pixels || dest == display_image) return;
	if (!dest->pixels->mask) {
		dest->pixels->mask = (unsigned char *)malloc((size_t)dest->width * dest->height);
		if (!dest->pixels->mask) return;
		memset(dest->pixels->mask, 1, (size_t)dest->width * dest->height);
	}
	for (int r = 0; r < h; r++) {
		int sy = y + r, ty = dy + y + r;
		if (sy < 0 || sy >= src->height || ty < 0 || ty >= dest->height) continue;
		for (int c = 0; c < w; c++) {
			int sx = x + c, tx = dx + x + c;
			if (sx < 0 || sx >= src->width || tx < 0 || tx >= dest->width) continue;
			dest->pixels->mask[(size_t)ty * dest->width + tx] =
				src->pixels->mask[(size_t)sy * src->width + sx];
		}
	}
}

void fill_image(image *dest, int x, int y, int w, int h, int r, int g, int b)
{
	int p = pixel_for(r, g, b);

	if (dest == display_image) {
		frame_fill(x, y, w, h, p);
		return;
	}
	if (!dest->pixels) build_image(dest);
	if (!dest->pixels || !dest->pixels->pix) return;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > dest->width) w = dest->width - x;
	if (y + h > dest->height) h = dest->height - y;
	for (int row = 0; row < h; row++)
		memset(dest->pixels->pix + (size_t)(y + row) * dest->width + x, p, (size_t)(w > 0 ? w : 0));
}

/* ---- events ---- */

int xwin_nextevent(XWin_Event *ev)
{
	static int stage = 0;

	/* first the size, then the whole table to draw, as X sends them */
	if (stage == 0) {
		stage = 1;
		memset(ev, 0, sizeof *ev);
		ev->type = ev_resize;
		ev->w = table_width;
		ev->h = table_height;
		return ev_resize;
	}
	if (stage == 1) {
		stage = 2;
		memset(ev, 0, sizeof *ev);
		ev->type = ev_expose;
		ev->w = table_width;
		ev->h = table_height;
		return ev_expose;
	}
	for (;;) {
		flush();
		if (acx_plat_event(ev)) return ev->type;
		acx_plat_wait();
	}
}

/* lib/stack.c's clock and its sleep (select with no files) */
int ace_gettimeofday(struct timeval *tv, void *tz)
{
	unsigned long ms = acx_plat_ms();

	(void)tz;
	tv->tv_sec = (long)(ms / 1000);
	tv->tv_usec = (long)(ms % 1000) * 1000;
	return 0;
}

int ace_select(int n, void *r, void *w, void *e, struct timeval *tv)
{
	unsigned long until;

	(void)n; (void)r; (void)w; (void)e;
	flush();
	if (!tv) return 0;
	until = acx_plat_ms() + (unsigned long)tv->tv_sec * 1000 + (unsigned long)tv->tv_usec / 1000;
	while ((long)(until - acx_plat_ms()) > 0) acx_plat_wait();
	return 0;
}
