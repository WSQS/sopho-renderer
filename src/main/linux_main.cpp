// src/main/linux_main.cpp

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/select.h>
#include <unistd.h>
#include <vector>

#include "pixel_buffer/pixel_buffer.h"

// Global state
static Display* g_display = nullptr;
static Window g_window = 0;
static GC g_gc = nullptr;
static XImage* g_image = nullptr;
static Pixmap g_pixmap = 0;
static int g_win_width = 800;
static int g_win_height = 600;
static int g_scaled_w = 0;
static int g_scaled_h = 0;

// PixelBuffers: source (800x600) and scaled output
// g_pixels: format "ARGB" (A=byte0, R=byte1, G=byte2, B=byte3)
// g_scaled: format "BGRA" (B=byte0, G=byte1, R=byte2, A=byte3) to match X11 native 32bpp
static sopho::PixelBuffer* g_pixels = nullptr; // 800x600 source
static sopho::PixelBuffer* g_scaled = nullptr; // CPU-scaled to window size

// X error handler to catch and report errors
static int handle_x_error(Display* dpy, XErrorEvent* ev)
{
    char msg[256];
    XGetErrorText(dpy, ev->error_code, msg, sizeof(msg));
    fprintf(stderr, "X Error: %s (opcode=%d, serial=%lu)\n", msg, ev->type, ev->serial);
    return 0;
}

// Nearest-neighbor scale from src PixelBuffer to dst PixelBuffer
// Reads raw bytes from src via get_pixels(); writes to dst via set_color()
static void scale_nn(const sopho::PixelBuffer* src, int sw, int sh, sopho::PixelBuffer* dst, int dw, int dh)
{
    for (int dy = 0; dy < dh; ++dy)
    {
        int sy = dy * sh / dh;
        for (int dx = 0; dx < dw; ++dx)
        {
            int sx = dx * sw / dw;
            // Read raw bytes from src (PixelBuffer doesn't expose get_color)
            const uint8_t* src_data = static_cast<const uint8_t*>(src->get_pixels());
            uint64_t src_bpp = 4;
            const uint8_t* src_px = src_data + (static_cast<uint64_t>(sy) * sw + sx) * src_bpp;
            uint8_t a = src_px[0];
            uint8_t r = src_px[1];
            uint8_t g = src_px[2];
            uint8_t b = src_px[3];
            // Write to dst using set_color
            dst->set_color(static_cast<uint64_t>(dx), static_cast<uint64_t>(dy), r, g, b, a);
        }
    }
}

// Initialize pixel buffer with red gradient (matches win_main.cpp InitDIBData)
void InitPixelData(int width, int height)
{
    g_pixels = new sopho::PixelBuffer(static_cast<uint64_t>(width), static_cast<uint64_t>(height), "ARGB", 4);

    for (uint64_t i = 0; i < g_pixels->get_height(); ++i)
    {
        for (uint64_t j = 0; j < g_pixels->get_width(); ++j)
        {
            uint8_t r = static_cast<uint8_t>(j * 255 / width);
            g_pixels->set_color(j, i, r, 0, 0, 255); // ARGB: A=255, R=r, G=0, B=0
        }
    }
}

// Blit: CPU-scale 800x600 source to window size, upload, copy
void StretchBlit(int win_width, int win_height)
{
    if (!g_pixels || win_width <= 0 || win_height <= 0)
        return;

    // CPU scale if window size changed
    if (g_scaled_w != win_width || g_scaled_h != win_height)
    {
        delete g_scaled;
        g_scaled = new sopho::PixelBuffer(static_cast<uint64_t>(win_width), static_cast<uint64_t>(win_height), "BGRA", 4);
        scale_nn(g_pixels, 800, 600, g_scaled, win_width, win_height);
        g_scaled_w = win_width;
        g_scaled_h = win_height;
    }

    int screen = DefaultScreen(g_display);
    int depth = DefaultDepth(g_display, screen);
    Visual* vis = DefaultVisual(g_display, screen);

    static int s_prev_w = 0, s_prev_h = 0;

    // Recreate pixmap/image on resize
    if (!g_pixmap || s_prev_w != win_width || s_prev_h != win_height)
    {
        Pixmap new_pixmap = XCreatePixmap(g_display, g_window, static_cast<unsigned int>(win_width),
                                          static_cast<unsigned int>(win_height), static_cast<unsigned int>(depth));

        XImage* new_image = XCreateImage(g_display, vis, depth, ZPixmap, 0, nullptr, win_width, win_height, 32, 0);
        if (!new_image)
        {
            fprintf(stderr, "XCreateImage failed\n");
            XFreePixmap(g_display, new_pixmap);
            return;
        }

        size_t image_bytes = static_cast<size_t>(new_image->bytes_per_line) * static_cast<size_t>(win_height);
        new_image->data = static_cast<char*>(malloc(image_bytes));
        if (!new_image->data)
        {
            fprintf(stderr, "malloc failed: %zu bytes\n", image_bytes);
            XDestroyImage(new_image);
            XFreePixmap(g_display, new_pixmap);
            return;
        }

        // If we have an old pixmap, copy its content to window immediately
        // to avoid black frame during rebuild
        if (g_pixmap && s_prev_w > 0 && s_prev_h > 0)
        {
            int cw = (s_prev_w < win_width) ? s_prev_w : win_width;
            int ch = (s_prev_h < win_height) ? s_prev_h : win_height;
            XCopyArea(g_display, g_pixmap, g_window, g_gc, 0, 0, static_cast<unsigned int>(cw),
                      static_cast<unsigned int>(ch), 0, 0);
            XFlush(g_display);
        }

        // Free old pixmap
        if (g_pixmap)
            XFreePixmap(g_display, g_pixmap);

        g_pixmap = new_pixmap;

        if (g_image)
            XDestroyImage(g_image);
        g_image = new_image;

        s_prev_w = win_width;
        s_prev_h = win_height;
    }

    // Pixel upload: g_scaled is "BGRA" format — read as raw uint32_t and pass to XPutPixel
    const uint8_t* scaled_data = static_cast<const uint8_t*>(g_scaled->get_pixels());
    for (int y = 0; y < win_height; ++y)
    {
        for (int x = 0; x < win_width; ++x)
        {
            const uint8_t* px = scaled_data + (static_cast<size_t>(y) * win_width + x) * 4;
            uint32_t pixel = *reinterpret_cast<const uint32_t*>(px);
            XPutPixel(g_image, x, y, pixel);
        }
    }

    XPutImage(g_display, g_pixmap, g_gc, g_image, 0, 0, 0, 0, static_cast<unsigned int>(win_width),
              static_cast<unsigned int>(win_height));

    XCopyArea(g_display, g_pixmap, g_window, g_gc, 0, 0, static_cast<unsigned int>(win_width),
              static_cast<unsigned int>(win_height), 0, 0);
    XFlush(g_display);
}

int main()
{
    // Open Display
    g_display = XOpenDisplay(nullptr);
    if (!g_display)
        return 1;

    XSetErrorHandler(handle_x_error);

    int screen = DefaultScreen(g_display);

    // Create simple window with default visual
    // bit_gravity=NorthWestGravity: try to preserve old content when window is resized
    // background=None: avoid X server clearing on resize
    g_window = XCreateSimpleWindow(g_display, RootWindow(g_display, screen), 0, 0, 800, 600, 0, 0, 0);
    {
        XSetWindowAttributes attrs;
        attrs.background_pixmap = None;
        attrs.bit_gravity = NorthWestGravity;
        XChangeWindowAttributes(g_display, g_window, CWBackPixmap | CWBitGravity, &attrs);
    }

    XSelectInput(g_display, g_window, ExposureMask | StructureNotifyMask);
    XMapWindow(g_display, g_window);

    // Graphics context for XPutImage into pixmap
    g_gc = XCreateGC(g_display, g_window, 0, nullptr);

    // Event loop
    int xfd = ConnectionNumber(g_display);

    while (true)
    {
        while (XPending(g_display))
        {
            XEvent e;
            XNextEvent(g_display, &e);

            if (e.type == ConfigureNotify)
            {
                g_win_width = e.xconfigure.width;
                g_win_height = e.xconfigure.height;
            }
            else if (e.type == Expose)
            {
                if (!g_pixels)
                    InitPixelData(800, 600);
            }
            else if (e.type == DestroyNotify)
            {
                if (g_image)
                    XDestroyImage(g_image);
                if (g_pixmap)
                    XFreePixmap(g_display, g_pixmap);
                delete g_scaled;
                delete g_pixels;
                XFreeGC(g_display, g_gc);
                XCloseDisplay(g_display);
                return 0;
            }
        }

        // Batch: only one StretchBlit per event-loop iteration
        if (g_pixels && g_win_width > 0 && g_win_height > 0)
        {
            StretchBlit(g_win_width, g_win_height);
        }

        // Wait for X events with ~60Hz timeout (no busy sleep)
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 16000;
        select(xfd + 1, &fds, nullptr, nullptr, &tv);

        if (g_pixels && g_win_width > 0 && g_win_height > 0)
            StretchBlit(g_win_width, g_win_height);
    }

    return 0;
}
