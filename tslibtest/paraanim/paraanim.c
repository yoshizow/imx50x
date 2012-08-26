/*
 *  paraanim.c
 *
 *  Copyright (C) 2001 Russell King.
 *  Copyright (C) 2012 yoshizow
 *
 * This file is placed under the GPL.  Please see the file
 * COPYING for more details.
 *
 *
 * Para Para Animation.
 */
//#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
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

#define NR_COLORS 16

struct ts_button {
    int x, y, w, h;
    char *text;
    int flags;
#define BUTTON_ACTIVE 0x00000001
};

/* [inactive] border fill text [active] border fill text */
static int button_palette [6] = {
    2, 12, 0, 2, 6, 15
};

#define BLACK 0
#define WHITE 15

enum {
    BUTTON_PLAY,
    BUTTON_CLEAR,
    BUTTON_QUIT,
    NR_BUTTONS
};
static struct ts_button buttons [NR_BUTTONS];

#define MXC_DAMAGE_MODE_FULL       0x01
#define MXC_DAMAGE_MODE_MONOCHROME 0x02

int mxcfb = -1;

static int64_t current_msec(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void mxc_damage(int x, int y, int w, int h, int mode)
{
    struct mxcfb_update_data param;
    const int MARKER = 999;
    int r;
    if(mxcfb < 0) {
	mxcfb = open("/dev/fb0", O_RDWR);
    }
    if(mxcfb < 0){
	printf("mxcfb open error!\n");
        return;
    }

    int64_t start_time = current_msec();

    param.update_region.left = x;
    param.update_region.top = y;
    param.update_region.width = w;
    param.update_region.height = h;
    param.waveform_mode = 4;//WAVEFORM_MODE_AUTO;
    if (mode & MXC_DAMAGE_MODE_FULL) {
        param.update_mode = UPDATE_MODE_FULL;
    } else {
        param.update_mode = UPDATE_MODE_PARTIAL;
    }
    param.update_marker = MARKER;
    param.temp = 0;
    if (mode & MXC_DAMAGE_MODE_MONOCHROME)
        param.flags = EPDC_FLAG_FORCE_MONOCHROME;
    else
        param.flags = 0;
    /* alt_buffer_data */
    r = ioctl(mxcfb, MXCFB_SEND_UPDATE, &param);
    printf("send update = %d\n",r);
#if 0
    r = ioctl(mxcfb, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &MARKER);
    printf("wait = %d\n",r);
#endif
    int64_t end_time = current_msec();
    printf("update time = %lld msec\n", end_time - start_time);
}

static void sig(int sig)
{
    close_framebuffer();
    fflush(stderr);
    printf("signal %d caught\n", sig);
    fflush(stdout);
    exit(1);
}

static void button_draw(struct ts_button *button)
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

static int
button_handle(struct ts_button *button, struct ts_sample *samp)
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

static void refresh_screen()
{
    int i;

    fillrect(0, 0, xres - 1, yres - 1, WHITE);
    //put_string_center(xres/2, yres/4,   "TSLIB test program", 2);
    //put_string_center(xres/2, yres/4+20,"Touch screen to move crosshair", 0);

    for (i = 0; i < NR_BUTTONS; i++)
        button_draw(&buttons [i]);

    mxc_damage(0, 0, 800, 600, MXC_DAMAGE_MODE_FULL | MXC_DAMAGE_MODE_MONOCHROME);
}

static void reflect_screen(int x1, int y1, int x2, int y2, int mode)
{
    int t;

    if (x1 > x2) {
	t = x1; x1 = x2; x2 = t;
    }
    if (y1 > y2) {
	t = y1; y1 = y2; y2 = t;
    }
    mxc_damage(x1, y1, x2 - x1 + 1, y2 - y1 + 1, mode);
}

int main(void)
{
    struct tsdev *ts;
    int x, y;
    unsigned int i;
    bool mode_pressed = false;
    bool quit_pressed = false;

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
        setcolor(i, i * 0x111111);

    /* Initialize buttons */
    memset(&buttons, 0, sizeof (buttons));
    buttons[0].w = buttons[1].w = buttons[2].w = xres / 4;
    buttons[0].h = buttons[1].h = buttons[2].h = 20;
    buttons[0].x = 0;
    buttons[1].x = (3 * xres) / 8;
    buttons[2].x = (3 * xres) / 4;
    buttons[0].y = buttons[1].y = buttons[2].y = 10;
    buttons[0].text = "Play";
    buttons[1].text = "Clear";
    buttons[2].text = "Quit";

    refresh_screen();

    while (1) {
        struct ts_sample samp;
        int ret;

        ret = ts_read(ts, &samp, 1);

        if (ret < 0) {
            perror("ts_read");
            close_framebuffer();
            exit(1);
        }

        if (ret != 1)
            continue;

        for (i = 0; i < NR_BUTTONS; i++)
            if (button_handle(&buttons [i], &samp))
                switch (i) {
                case BUTTON_PLAY:
                    refresh_screen();
                    break;
                case BUTTON_CLEAR:
                    refresh_screen();
                    break;
                case BUTTON_QUIT:
                    quit_pressed = true;
                }

        printf("%ld.%06ld: %6d %6d %6d\n", samp.tv.tv_sec, samp.tv.tv_usec,
               samp.x, samp.y, samp.pressure);

        if (samp.pressure > 0) {
            if (mode_pressed) {
                line (x, y, samp.x, samp.y, BLACK);
                reflect_screen(x, y, samp.x, samp.y, MXC_DAMAGE_MODE_MONOCHROME);
            }
            x = samp.x;
            y = samp.y;
            mode_pressed = true;
        } else
            mode_pressed = false;
        if (quit_pressed)
            break;
        //mxc_damage(0, 31, xres, yres - 31, MXC_DAMAGE_MODE_MONOCHROME);
    }
    close_framebuffer();
    return 0;
}
