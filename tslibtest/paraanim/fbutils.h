/*
 * fbutils.h
 *
 * Headers for utility routines for framebuffer interaction
 *
 * Copyright 2002 Russell King and Doug Lowder
 *
 * This file is placed under the GPL.  Please see the
 * file COPYING for details.
 *
 */

#ifndef _FBUTILS_H
#define _FBUTILS_H

#include <asm/types.h>

#include "font.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This constant, being ORed with the color index tells the library
 * to draw in exclusive-or mode (that is, drawing the same second time
 * in the same place will remove the element leaving the background intact).
 */
#define XORMODE	0x80000000

extern __u32 xres, yres;

int open_framebuffer(void);
void close_framebuffer(void);
void setcolor(unsigned colidx, unsigned value);
void setfont(const struct fbcon_font_desc *font);
void put_cross(int x, int y, unsigned colidx);
void put_string(int x, int y, char *s, unsigned colidx);
void put_string_center(int x, int y, char *s, unsigned colidx);
void pixel (int x, int y, unsigned colidx);
void line (int x1, int y1, int x2, int y2, unsigned colidx);
void rect (int x1, int y1, int x2, int y2, unsigned colidx);
void fillrect (int x1, int y1, int x2, int y2, unsigned colidx);

/*** EPD ***/

#define MXC_DAMAGE_MODE_FULL       0x01
#define MXC_DAMAGE_MODE_MONOCHROME 0x02

void mxc_damage(int x, int y, int w, int h, int mode, int wait);

#ifdef __cplusplus
}
#endif

#endif /* _FBUTILS_H */
