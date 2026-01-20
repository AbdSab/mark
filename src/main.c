#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

enum Mode { DRAW, RECT };

// Helper to set pixel in BGRA (X11 Native)
void set_pixel_bgra(unsigned char* data, int w, int h, int x, int y) {
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int idx = (y * w + x) * 4;
    data[idx]     = 0;   // Blue
    data[idx + 1] = 0;   // Green
    data[idx + 2] = 255; // Red
    data[idx + 3] = 255; // Alpha
}

// Thick line algorithm (3x3 brush)
void draw_line_buffer(unsigned char* data, int w, int h, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        for(int i=-1; i<=1; i++)
            for(int j=-1; j<=1; j++)
                set_pixel_bgra(data, w, h, x0+i, y0+j);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <image>\n", argv[0]);
        return 1;
    }

    int w, h, n;
    unsigned char *data = stbi_load(argv[1], &w, &h, &n, 4);
    if (!data) return 1;

    // Convert RGBA to BGRA for X11 compatibility if needed
    for (int i = 0; i < w * h * 4; i += 4) {
        unsigned char r = data[i];
        data[i] = data[i+2]; // B
        data[i+2] = r;       // R
    }

    Display *dpy = XOpenDisplay(NULL);
    int scr = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, w, h, 0, 0, 0);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
    XMapWindow(dpy, win);

    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetLineAttributes(dpy, gc, 4, LineSolid, CapRound, JoinRound);

    // Set X11 drawing color to Red
    XColor red;
    XParseColor(dpy, DefaultColormap(dpy, scr), "red", &red);
    XAllocColor(dpy, DefaultColormap(dpy, scr), &red);
    XSetForeground(dpy, gc, red.pixel);

    XImage *ximg = XCreateImage(dpy, DefaultVisual(dpy, scr), DefaultDepth(dpy, scr), ZPixmap, 0, (char*)data, w, h, 32, 0);

    int mode = DRAW, drawing = 0, last_x, last_y, start_x, start_y;
    XEvent ev;

    while (1) {
        XNextEvent(dpy, &ev);

        if (ev.type == Expose || (ev.type == MotionNotify && drawing)) {
            XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, w, h);
            if (mode == RECT && drawing) {
                int rx = (ev.xmotion.x < start_x) ? ev.xmotion.x : start_x;
                int ry = (ev.xmotion.y < start_y) ? ev.xmotion.y : start_y;
                XDrawRectangle(dpy, win, gc, rx, ry, abs(ev.xmotion.x - start_x), abs(ev.xmotion.y - start_y));
            }
        }

        if (ev.type == KeyPress) {
            KeySym k = XLookupKeysym(&ev.xkey, 0);
            if (k == XK_1) mode = DRAW;
            if (k == XK_2) mode = RECT;
            if (k == XK_Return) {
                // Swap B and R back to RGBA for PNG saving
                for (int i = 0; i < w * h * 4; i += 4) {
                    unsigned char b = data[i];
                    data[i] = data[i+2];
                    data[i+2] = b;
                }
                stbi_write_png("clip_tmp.png", w, h, 4, data, w * 4);
                system("xclip -selection clipboard -t image/png -i clip_tmp.png && rm clip_tmp.png");
                break;
            }
            if (k == XK_Escape) break;
        }

        if (ev.type == ButtonPress) {
            drawing = 1;
            last_x = start_x = ev.xbutton.x;
            last_y = start_y = ev.xbutton.y;
        }

        if (ev.type == MotionNotify && drawing && mode == DRAW) {
            draw_line_buffer(data, w, h, last_x, last_y, ev.xmotion.x, ev.xmotion.y);
            last_x = ev.xmotion.x;
            last_y = ev.xmotion.y;
        }

        if (ev.type == ButtonRelease && drawing) {
            if (mode == RECT) {
                int xmin = (ev.xbutton.x < start_x) ? ev.xbutton.x : start_x;
                int xmax = (ev.xbutton.x > start_x) ? ev.xbutton.x : start_x;
                int ymin = (ev.xbutton.y < start_y) ? ev.xbutton.y : start_y;
                int ymax = (ev.xbutton.y > start_y) ? ev.xbutton.y : start_y;
                for (int x = xmin; x <= xmax; x++) {
                    for(int t=-1; t<=1; t++) { set_pixel_bgra(data, w, h, x, ymin+t); set_pixel_bgra(data, w, h, x, ymax+t); }
                }
                for (int y = ymin; y <= ymax; y++) {
                    for(int t=-1; t<=1; t++) { set_pixel_bgra(data, w, h, xmin+t, y); set_pixel_bgra(data, w, h, xmax+t, y); }
                }
            }
            drawing = 0;
            XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, w, h);
        }
    }

    XCloseDisplay(dpy);
    return 0;
}
