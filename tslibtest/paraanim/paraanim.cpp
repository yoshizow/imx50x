/*
 *  paraanim.cpp
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
#include <unistd.h>
#include <list>

#include <tslib.h>
#include "fbutils.h"
#include "font.h"

#define NR_COLORS 16

struct ts_button {
    int x, y, w, h;
    char *text;
    int flags;
#define BUTTON_ACTIVE 0x00000001
};

/* [inactive] border fill text [active] border fill text */
static int button_palette[6] = {
    2, 12, 0, 2, 6, 15
};

#define BLACK 0
#define WHITE 15

enum {
    BUTTON_PLAY,
    BUTTON_PLAY_MONOCHROME,
    BUTTON_NEXT,
    BUTTON_CLEAR,
    BUTTON_QUIT,
    NR_BUTTONS
};
static struct ts_button buttons[NR_BUTTONS];

class Line {
public:
    Line(int x1, int y1, int x2, int y2) : x1(x1), y1(y1), x2(x2), y2(y2) {}
    int x1, y1, x2, y2;
};

typedef std::list<Line> Drawing;
typedef std::list<Drawing> Animation;

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

static void refresh_screen(bool full = true)
{
    int i;

    fillrect(0, 0, xres - 1, yres - 1, WHITE);

    for (i = 0; i < NR_BUTTONS; i++)
        button_draw(&buttons [i]);

    mxc_damage(0, 0, xres, yres, full ? MXC_DAMAGE_MODE_FULL : MXC_DAMAGE_MODE_MONOCHROME, true);
}

static void finalize_screen()
{
    int i;

    fillrect(0, 0, xres - 1, yres - 1, WHITE);

    mxc_damage(0, 0, xres, yres, MXC_DAMAGE_MODE_FULL, true);
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
    mxc_damage(x1, y1, x2 - x1 + 1, y2 - y1 + 1, mode, false);
}

static void draw_frame(const Drawing& drawing)
{
    fillrect(0, 0, xres - 1, yres - 1, WHITE);
    for (Drawing::const_iterator it = drawing.begin(); it != drawing.end(); it++) {
        const Line& e = *it;
        line(e.x1, e.y1, e.x2, e.y2, BLACK);
    }
}

static void play(const Animation& animation, bool mono)
{
    refresh_screen();
    for (Animation::const_iterator it = animation.begin(); it != animation.end(); it++) {
        draw_frame(*it);
        mxc_damage(0, 0, xres, yres, mono ? MXC_DAMAGE_MODE_MONOCHROME: 0, true);
    }
    sleep(1);
}

int main(void)
{
    struct tsdev *ts;
    int x, y;
    unsigned int i;
    bool mode_pressed = false;
    bool quit_pressed = false;

    Animation animation;
    Drawing current_drawing;

    char *tsdevice = NULL;

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

    setfont(&font_vga_8x16);

    x = xres/2;
    y = yres/2;

    for (i = 0; i < NR_COLORS; i++)
        setcolor(i, i * 0x111111);

    /* Initialize buttons */
    memset(&buttons, 0, sizeof (buttons));
    buttons[0].w = buttons[1].w = buttons[2].w = buttons[3].w = buttons[4].w = xres / 6;
    buttons[0].h = buttons[1].h = buttons[2].h = buttons[3].h = buttons[4].h = 40;
    buttons[0].x = 0;
    buttons[1].x = (5 * xres) / 24;
    buttons[2].x = (10 * xres) / 24;
    buttons[3].x = (15 * xres) / 24;
    buttons[4].x = (20 * xres) / 24;
    buttons[0].y = buttons[1].y = buttons[2].y = buttons[3].y = buttons[4].y = 10;
    buttons[0].text = (char*)"Play";
    buttons[1].text = (char*)"PlayMono";
    buttons[2].text = (char*)"Next";
    buttons[3].text = (char*)"Clear";
    buttons[4].text = (char*)"Quit";

    refresh_screen();

    while (1) {
    loop:
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

        bool dirty = true;

        for (i = 0; i < NR_BUTTONS; i++) {
            if (button_handle(&buttons[i], &samp)) {
                switch (i) {
                case BUTTON_PLAY:
                    if (!current_drawing.empty())
                        animation.push_back(current_drawing);
                    play(animation, false);
                    current_drawing.clear();
                    refresh_screen();
                    dirty = false;
                    break;
                case BUTTON_PLAY_MONOCHROME:
                    if (!current_drawing.empty())
                        animation.push_back(current_drawing);
                    play(animation, true);
                    current_drawing.clear();
                    refresh_screen();
                    dirty = false;
                    break;
                case BUTTON_NEXT:
                    animation.push_back(current_drawing);
                    current_drawing.clear();
                    refresh_screen(false);
                    dirty = false;
                    break;
                case BUTTON_CLEAR:
                    current_drawing.clear();
                    refresh_screen();
                    dirty = false;
                    break;
                case BUTTON_QUIT:
                    quit_pressed = true;
                }
            }
        }

        /*printf("%ld.%06ld: %6d %6d %6d\n", samp.tv.tv_sec, samp.tv.tv_usec,
                 samp.x, samp.y, samp.pressure);*/

        if (samp.pressure > 0 && samp.y > (buttons[0].y + buttons[0].h)) {
            if (mode_pressed) {
                line(x, y, samp.x, samp.y, BLACK);
                current_drawing.push_back(Line(x, y, samp.x, samp.y));
            }
            x = samp.x;
            y = samp.y;
            mode_pressed = true;
        } else
            mode_pressed = false;

        if (dirty)
            mxc_damage(0, 0, xres, yres, MXC_DAMAGE_MODE_MONOCHROME, false);
        if (quit_pressed)
            break;
    }
    finalize_screen();
    close_framebuffer();
    return 0;
}
