/*
 * fbutils.c
 *
 * Utility routines for framebuffer interaction
 *
 * Copyright 2002 Russell King and Doug Lowder
 *
 * This file is placed under the GPL.  Please see the
 * file COPYING for details.
 *
 */

//#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include <linux/mxcfb.h>

#include "font.h"
#include "fbutils.h"

union multiptr {
	unsigned char *p8;
	unsigned short *p16;
	unsigned long *p32;
};

static int con_fd, fb_fd, last_vt = -1;
static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo var;
static unsigned char *fbuffer;
static unsigned char **line_addr;
static int fb_fd=0;
static int bytes_per_pixel;
static unsigned colormap [256];
static const struct fbcon_font_desc * current_font = &font_vga_8x8;
__u32 xres, yres;

static char *defaultfbdevice = "/dev/fb0";
static char *defaultconsoledevice = "/dev/tty";
static char *fbdevice = NULL;
static char *consoledevice = NULL;

int open_framebuffer(void)
{
	struct vt_stat vts;
	char vtname[128];
	int fd, nr;
	unsigned y, addr;

	if ((fbdevice = getenv ("TSLIB_FBDEVICE")) == NULL)
		fbdevice = defaultfbdevice;

	if ((consoledevice = getenv ("TSLIB_CONSOLEDEVICE")) == NULL)
		consoledevice = defaultconsoledevice;

	if (strcmp (consoledevice, "none") != 0) {
		sprintf (vtname,"%s%d", consoledevice, 1);
        	fd = open (vtname, O_WRONLY);
        	if (fd < 0) {
        	        perror("open consoledevice");
        	        return -1;
        	}

		if (ioctl(fd, VT_OPENQRY, &nr) < 0) {
        	        perror("ioctl VT_OPENQRY");
        	        return -1;
        	}
        	close(fd);

        	sprintf(vtname, "%s%d", consoledevice, nr);

        	con_fd = open(vtname, O_RDWR | O_NDELAY);
        	if (con_fd < 0) {
        	        perror("open tty");
        	        return -1;
        	}

        	if (ioctl(con_fd, VT_GETSTATE, &vts) == 0)
        	        last_vt = vts.v_active;

        	if (ioctl(con_fd, VT_ACTIVATE, nr) < 0) {
        	        perror("VT_ACTIVATE");
        	        close(con_fd);
        	        return -1;
        	}

#ifndef TSLIB_NO_VT_WAITACTIVE
        	if (ioctl(con_fd, VT_WAITACTIVE, nr) < 0) {
        	        perror("VT_WAITACTIVE");
        	        close(con_fd);
        	        return -1;
        	}
#endif

        	if (ioctl(con_fd, KDSETMODE, KD_GRAPHICS) < 0) {
        	        perror("KDSETMODE");
        	        close(con_fd);
        	        return -1;
        	}

	}

	fb_fd = open(fbdevice, O_RDWR);
	if (fb_fd == -1) {
		perror("open fbdevice");
		return -1;
	}

	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		perror("ioctl FBIOGET_FSCREENINFO");
		close(fb_fd);
		return -1;
	}

	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		perror("ioctl FBIOGET_VSCREENINFO");
		close(fb_fd);
		return -1;
	}
	xres = var.xres;
	yres = var.yres;

	fbuffer = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fb_fd, 0);
	if (fbuffer == (unsigned char *)-1) {
		perror("mmap framebuffer");
		close(fb_fd);
		return -1;
	}
	memset(fbuffer,0,fix.smem_len);

	bytes_per_pixel = (var.bits_per_pixel + 7) / 8;
	line_addr = malloc (sizeof (__u32) * var.yres_virtual);
	addr = 0;
	for (y = 0; y < var.yres_virtual; y++, addr += fix.line_length)
		line_addr [y] = fbuffer + addr;

	return 0;
}

void close_framebuffer(void)
{
	munmap(fbuffer, fix.smem_len);
	close(fb_fd);


	if(strcmp(consoledevice,"none")!=0) {
	
        	if (ioctl(con_fd, KDSETMODE, KD_TEXT) < 0)
        	        perror("KDSETMODE");

        	if (last_vt >= 0)
        	        if (ioctl(con_fd, VT_ACTIVATE, last_vt))
        	                perror("VT_ACTIVATE");

        	close(con_fd);
	}

        free (line_addr);
}

void put_cross(int x, int y, unsigned colidx)
{
	line (x - 10, y, x - 2, y, colidx);
	line (x + 2, y, x + 10, y, colidx);
	line (x, y - 10, x, y - 2, colidx);
	line (x, y + 2, x, y + 10, colidx);

#if 1
	line (x - 6, y - 9, x - 9, y - 9, colidx + 1);
	line (x - 9, y - 8, x - 9, y - 6, colidx + 1);
	line (x - 9, y + 6, x - 9, y + 9, colidx + 1);
	line (x - 8, y + 9, x - 6, y + 9, colidx + 1);
	line (x + 6, y + 9, x + 9, y + 9, colidx + 1);
	line (x + 9, y + 8, x + 9, y + 6, colidx + 1);
	line (x + 9, y - 6, x + 9, y - 9, colidx + 1);
	line (x + 8, y - 9, x + 6, y - 9, colidx + 1);
#else
	line (x - 7, y - 7, x - 4, y - 4, colidx + 1);
	line (x - 7, y + 7, x - 4, y + 4, colidx + 1);
	line (x + 4, y - 4, x + 7, y - 7, colidx + 1);
	line (x + 4, y + 4, x + 7, y + 7, colidx + 1);
#endif
}

void setfont(const struct fbcon_font_desc *font)
{
	if (font != NULL)
		current_font = font;
}

void put_char(int x, int y, int c, int colidx)
{
	int i,j,bits;

	for (i = 0; i < current_font->height; i++) {
		bits = current_font->data [current_font->height * c + i];
		for (j = 0; j < current_font->width; j++, bits <<= 1)
			if (bits & 0x80)
				pixel (x + j, y + i, colidx);
	}
}

void put_string(int x, int y, char *s, unsigned colidx)
{
	int i;
	for (i = 0; *s; i++, x += current_font->width, s++)
		put_char (x, y, *s, colidx);
}

void put_string_center(int x, int y, char *s, unsigned colidx)
{
	size_t sl = strlen (s);
        put_string (x - (sl / 2) * current_font->width,
                    y - current_font->height / 2, s, colidx);
}

void setcolor(unsigned colidx, unsigned value)
{
	unsigned res;
	unsigned short red, green, blue;
	struct fb_cmap cmap;

#ifdef DEBUG
	if (colidx > 255) {
		fprintf (stderr, "WARNING: color index = %u, must be <256\n",
			 colidx);
		return;
	}
#endif

	switch (bytes_per_pixel) {
	default:
	case 1:
		res = colidx;
		red = (value >> 8) & 0xff00;
		green = value & 0xff00;
		blue = (value << 8) & 0xff00;
		cmap.start = colidx;
		cmap.len = 1;
		cmap.red = &red;
		cmap.green = &green;
		cmap.blue = &blue;
		cmap.transp = NULL;

        	if (ioctl (fb_fd, FBIOPUTCMAP, &cmap) < 0)
        	        perror("ioctl FBIOPUTCMAP");
		break;
	case 2:
	case 4:
		red = (value >> 16) & 0xff;
		green = (value >> 8) & 0xff;
		blue = value & 0xff;
		res = ((red >> (8 - var.red.length)) << var.red.offset) |
                      ((green >> (8 - var.green.length)) << var.green.offset) |
                      ((blue >> (8 - var.blue.length)) << var.blue.offset);
	}
        colormap [colidx] = res;
}

static inline void __setpixel (union multiptr loc, unsigned xormode, unsigned color)
{
	switch(bytes_per_pixel) {
	case 1:
	default:
		if (xormode)
			*loc.p8 ^= color;
		else
			*loc.p8 = color;
		break;
	case 2:
		if (xormode)
			*loc.p16 ^= color;
		else
			*loc.p16 = color;
		break;
	case 4:
		if (xormode)
			*loc.p32 ^= color;
		else
			*loc.p32 = color;
		break;
	}
}

void pixel (int x, int y, unsigned colidx)
{
	unsigned xormode;
	union multiptr loc;

	if ((x < 0) || ((__u32)x >= var.xres_virtual) ||
	    (y < 0) || ((__u32)y >= var.yres_virtual))
		return;

	xormode = colidx & XORMODE;
	colidx &= ~XORMODE;

#ifdef DEBUG
	if (colidx > 255) {
		fprintf (stderr, "WARNING: color value = %u, must be <256\n",
			 colidx);
		return;
	}
#endif

	loc.p8 = line_addr [y] + x * bytes_per_pixel;
	__setpixel (loc, xormode, colormap [colidx]);
}

void line (int x1, int y1, int x2, int y2, unsigned colidx)
{
	int tmp;
	int dx = x2 - x1;
	int dy = y2 - y1;

	if (abs (dx) < abs (dy)) {
		if (y1 > y2) {
			tmp = x1; x1 = x2; x2 = tmp;
			tmp = y1; y1 = y2; y2 = tmp;
			dx = -dx; dy = -dy;
		}
		x1 <<= 16;
		/* dy is apriori >0 */
		dx = (dx << 16) / dy;
		while (y1 <= y2) {
			pixel (x1 >> 16, y1, colidx);
			x1 += dx;
			y1++;
		}
	} else {
		if (x1 > x2) {
			tmp = x1; x1 = x2; x2 = tmp;
			tmp = y1; y1 = y2; y2 = tmp;
			dx = -dx; dy = -dy;
		}
		y1 <<= 16;
		dy = dx ? (dy << 16) / dx : 0;
		while (x1 <= x2) {
			pixel (x1, y1 >> 16, colidx);
			y1 += dy;
			x1++;
		}
	}
}

void rect (int x1, int y1, int x2, int y2, unsigned colidx)
{
	line (x1, y1, x2, y1, colidx);
	line (x2, y1, x2, y2, colidx);
	line (x2, y2, x1, y2, colidx);
	line (x1, y2, x1, y1, colidx);
}

void fillrect (int x1, int y1, int x2, int y2, unsigned colidx)
{
	int tmp;
	unsigned xormode;
	union multiptr loc;

	/* Clipping and sanity checking */
	if (x1 > x2) { tmp = x1; x1 = x2; x2 = tmp; }
	if (y1 > y2) { tmp = y1; y1 = y2; y2 = tmp; }
	if (x1 < 0) x1 = 0; if ((__u32)x1 >= xres) x1 = xres - 1;
	if (x2 < 0) x2 = 0; if ((__u32)x2 >= xres) x2 = xres - 1;
	if (y1 < 0) y1 = 0; if ((__u32)y1 >= yres) y1 = yres - 1;
	if (y2 < 0) y2 = 0; if ((__u32)y2 >= yres) y2 = yres - 1;

	if ((x1 > x2) || (y1 > y2))
		return;

	xormode = colidx & XORMODE;
	colidx &= ~XORMODE;

#ifdef DEBUG
	if (colidx > 255) {
		fprintf (stderr, "WARNING: color value = %u, must be <256\n",
			 colidx);
		return;
	}
#endif

	colidx = colormap [colidx];

	for (; y1 <= y2; y1++) {
		loc.p8 = line_addr [y1] + x1 * bytes_per_pixel;
		for (tmp = x1; tmp <= x2; tmp++) {
			__setpixel (loc, xormode, colidx);
			loc.p8 += bytes_per_pixel;
		}
	}
}

/*** EPD ***/

static int64_t current_msec(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void mxc_damage(int x, int y, int w, int h, int mode, int wait)
{
	struct mxcfb_update_data param;
        
	const int MARKER = 999;
	int r;

	if (fb_fd < 0) {
		return;
	}

	int64_t start_time = current_msec();

	param.update_region.left = x;
	param.update_region.top = y;
	param.update_region.width = w;
	param.update_region.height = h;
	if (mode & MXC_DAMAGE_MODE_FULL)
		param.update_mode = UPDATE_MODE_FULL;
	else
		param.update_mode = UPDATE_MODE_PARTIAL;
	param.update_marker = MARKER;
	param.temp = 0;
	if (mode & MXC_DAMAGE_MODE_MONOCHROME) {
		param.waveform_mode = 4;
		param.flags = EPDC_FLAG_FORCE_MONOCHROME;
	} else {
		param.waveform_mode = WAVEFORM_MODE_AUTO;
		param.flags = 0;
	}

	r = ioctl(fb_fd, MXCFB_SEND_UPDATE, &param);
	printf("send update = %d\n", r);
	if (wait) {
		r = ioctl(fb_fd, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &MARKER);
		printf("wait = %d\n", r);
	}
	int64_t end_time = current_msec();
	printf("update time = %lld msec\n", end_time - start_time);
}
