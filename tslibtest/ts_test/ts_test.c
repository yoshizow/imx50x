/*
 *  tslib/src/ts_test.c
 *
 *  Copyright (C) 2001 Russell King.
 *
 * This file is placed under the GPL.  Please see the file
 * COPYING for more details.
 *
 *
 * Basic test program for touchscreen library.
 */
//#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <sys/types.h>
#include <linux/mxcfb.h>

#include "tslib.h"
#include "fbutils.h"

static int palette [] =
{
	0x000000, 0xffe080, 0xffffff, 0xe0c0a0, 0x304050, 0x80b8c0
};
#define NR_COLORS (sizeof (palette) / sizeof (palette [0]))

struct ts_button {
	int x, y, w, h;
	char *text;
	int flags;
#define BUTTON_ACTIVE 0x00000001
};

/* [inactive] border fill text [active] border fill text */
static int button_palette [6] =
{
	1, 4, 2,
	1, 5, 0
};

#define NR_BUTTONS 3
static struct ts_button buttons [NR_BUTTONS];

static void sig(int sig)
{
	close_framebuffer();
	fflush(stderr);
	printf("signal %d caught\n", sig);
	fflush(stdout);
	exit(1);
}

static void button_draw (struct ts_button *button)
{
	int s = (button->flags & BUTTON_ACTIVE) ? 3 : 0;
	rect (button->x, button->y, button->x + button->w - 1,
	      button->y + button->h - 1, button_palette [s]);
	fillrect (button->x + 1, button->y + 1,
		  button->x + button->w - 2,
		  button->y + button->h - 2, button_palette [s + 1]);
	put_string_center (button->x + button->w / 2,
			   button->y + button->h / 2,
			   button->text, button_palette [s + 2]);
}

static int button_handle (struct ts_button *button, struct ts_sample *samp)
{
	int inside = (samp->x >= button->x) && (samp->y >= button->y) &&
		(samp->x < button->x + button->w) &&
		(samp->y < button->y + button->h);

	if (samp->pressure > 0) {
		if (inside) {
			if (!(button->flags & BUTTON_ACTIVE)) {
				button->flags |= BUTTON_ACTIVE;
				button_draw (button);
			}
		} else if (button->flags & BUTTON_ACTIVE) {
			button->flags &= ~BUTTON_ACTIVE;
			button_draw (button);
		}
	} else if (button->flags & BUTTON_ACTIVE) {
		button->flags &= ~BUTTON_ACTIVE;
		button_draw (button);
                return 1;
	}

        return 0;
}

static void mxc_damage (int x, int y, int w, int h);

static void refresh_screen ()
{
	int i;

	fillrect (0, 0, xres - 1, yres - 1, 0);
	put_string_center (xres/2, yres/4,   "TSLIB test program", 1);
	put_string_center (xres/2, yres/4+20,"Touch screen to move crosshair", 2);

	for (i = 0; i < NR_BUTTONS; i++)
		button_draw (&buttons [i]);

	mxc_damage (0, 0, 800, 600);
}

int mxcfb = -1;
static void
mxc_damage(int x, int y, int w, int h){
    struct mxcfb_update_data param;
    const int MARKER = 999;
    int r;
    if(mxcfb<0){
        mxcfb = open("/dev/fb0", O_RDWR);
    }
    if(mxcfb<0){
        printf("mxcfb open error!\n");
    }

    param.update_region.left = x;
    param.update_region.top = y;
    param.update_region.width = w;
    param.update_region.height = h;
    param.waveform_mode = WAVEFORM_MODE_AUTO;
    param.update_mode = UPDATE_MODE_PARTIAL;
    param.update_marker = MARKER;
    param.temp = 0;
    param.flags = EPDC_FLAG_FORCE_MONOCHROME;
    /* alt_buffer_data */
    r = ioctl(mxcfb, MXCFB_SEND_UPDATE, &param);
    printf("send update = %d\n",r);
#if 0
    r = ioctl(mxcfb, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &MARKER);
    printf("wait = %d\n",r);
#endif

}

static void reflect_screen (int x1, int y1, int x2, int y2)
{
    int t;

    if (x1 > x2) {
        t = x1; x1 = x2; x2 = t;
    }
    if (y1 > y2) {
        t = y1; y1 = y2; y2 = t;
    }
    mxc_damage(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

int main()
{
	struct tsdev *ts;
	int x, y;
	unsigned int i;
	unsigned int mode = 0;
	int quit_pressed = 0;

	char *tsdevice=NULL;

	signal(SIGSEGV, sig);
	signal(SIGINT, sig);
	signal(SIGTERM, sig);

	if( (tsdevice = getenv("TSLIB_TSDEVICE")) != NULL ) {
		ts = ts_open(tsdevice,0);
	} else {
		if (!(ts = ts_open("/dev/input/event1", 0)))
			ts = ts_open("/dev/touchscreen/ucb1x00", 0);
	}

	if (!ts) {
		perror("ts_open");
		exit(1);
	}

	if (ts_config(ts)) {
		perror("ts_config");
		exit(1);
	}

	if (open_framebuffer()) {
		close_framebuffer();
		exit(1);
	}

	x = xres/2;
	y = yres/2;

	for (i = 0; i < NR_COLORS; i++)
		setcolor (i, palette [i]);

	/* Initialize buttons */
	memset (&buttons, 0, sizeof (buttons));
	buttons [0].w = buttons [1].w = buttons [2].w = xres / 4;
	buttons [0].h = buttons [1].h = buttons [2].h = 20;
	buttons [0].x = 0;
	buttons [1].x = (3 * xres) / 8;
	buttons [2].x = (3 * xres) / 4;
	buttons [0].y = buttons [1].y = buttons [2].y = 10;
	buttons [0].text = "Drag";
	buttons [1].text = "Draw";
	buttons [2].text = "Quit";

	refresh_screen ();

	while (1) {
		struct ts_sample samp;
		int ret;

		/* Show the cross */
		if ((mode & 15) != 1)
			put_cross(x, y, 2 | XORMODE);

		ret = ts_read(ts, &samp, 1);

		/* Hide it */
		if ((mode & 15) != 1)
			put_cross(x, y, 2 | XORMODE);

		if (ret < 0) {
			perror("ts_read");
			close_framebuffer();
			exit(1);
		}

		if (ret != 1)
			continue;

		for (i = 0; i < NR_BUTTONS; i++)
			if (button_handle (&buttons [i], &samp))
				switch (i) {
				case 0:
					mode = 0;
					refresh_screen ();
					break;
				case 1:
					mode = 1;
					refresh_screen ();
					break;
				case 2:
					quit_pressed = 1;
				}

		printf("%ld.%06ld: %6d %6d %6d\n", samp.tv.tv_sec, samp.tv.tv_usec,
			samp.x, samp.y, samp.pressure);

		if (samp.pressure > 0) {
			if (mode == 0x80000001) {
				line (x, y, samp.x, samp.y, 2);
                                reflect_screen (x, y, samp.x, samp.y);
                        }
			x = samp.x;
			y = samp.y;
			mode |= 0x80000000;
		} else
			mode &= ~0x80000000;
		if (quit_pressed)
			break;
	}
	close_framebuffer();
}
