/* The few Xlib names Ace of Penguins' lib/table.c and lib/help.c use, for
 * the Amiga port: no X server; amiga/acexwin.c draws them into the game's
 * 8-bit frame (ACGame) in the one font the port has (topaz 8).
 * Copyright (c) 2026 AmiCradle Team. GPL-2.0-or-later, like Ace of Penguins. */
#ifndef ACE_AMIGA_XLIB_H
#define ACE_AMIGA_XLIB_H

typedef struct _ace_display Display;
typedef unsigned long Window;
typedef unsigned long Pixmap;
typedef unsigned long Font;
typedef unsigned long KeySym;
typedef struct _ace_gc *GC;

typedef struct {
	short lbearing, rbearing, width, ascent, descent;
	unsigned short attributes;
} XCharStruct;

typedef struct {
	Font fid;
	int ascent, descent;
	XCharStruct max_bounds;
} XFontStruct;

XFontStruct *XLoadQueryFont(Display *d, const char *name);
int XTextWidth(XFontStruct *f, const char *s, int n);
int XTextExtents(XFontStruct *f, const char *s, int n, int *dir, int *ascent,
                 int *descent, XCharStruct *overall);
int XSetForeground(Display *d, GC gc, unsigned long pixel);
int XSetFont(Display *d, GC gc, Font font);
int XDrawString(Display *d, Window w, GC gc, int x, int y, const char *s, int n);
int XDrawLine(Display *d, Window w, GC gc, int x1, int y1, int x2, int y2);
int XDrawPoint(Display *d, Window w, GC gc, int x, int y);
int XFillRectangle(Display *d, Window w, GC gc, int x, int y,
                   unsigned int width, unsigned int height);

#endif
