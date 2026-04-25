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
static std::vector<uint32_t> g_pixels; // 800x600 source
static std::vector<uint32_t> g_scaled; // CPU-scaled to window size

// X error handler to catch and report errors
static int handle_x_error(Display* dpy, XErrorEvent* ev)
{
    char msg[256];
    XGetErrorText(dpy, ev->error_code, msg, sizeof(msg));
    fprintf(stderr, "X Error: %s (opcode=%d, serial=%lu)\n", msg, ev->type, ev->serial);
    return 0;
}

// Nearest-neighbor scale
static void scale_nn(const uint32_t* src, int sw, int sh, uint32_t* dst, int dw, int dh)
{
    for (int dy = 0; dy < dh; ++dy)
    {
        int sy = dy * sh / dh;
        for (int dx = 0; dx < dw; ++dx)
        {
            int sx = dx * sw / dw;
            dst[dy * dw + dx] = src[sy * sw + sx];
        }
    }
}

// Initialize pixel buffer with red gradient (matches win_main.cpp InitDIBData)
// Pixel format: 0xAARRGGBB (ARGB), alpha=FF for full opacity
void InitPixelData(int width, int height)
{
    g_pixels.resize(static_cast<size_t>(width) * height);

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            uint8_t r = static_cast<uint8_t>(j * 255 / width);
            g_pixels[static_cast<size_t>(i) * width + j] = (0xFFu << 24) | (r << 16); // 0xAARRGGBB
        }
    }
}

// Blit: CPU-scale 800x600 source to window size, upload, copy
void StretchBlit(int win_width, int win_height)
{
    if (g_pixels.empty() || win_width <= 0 || win_height <= 0)
        return;

    // CPU scale if window size changed
    if (g_scaled_w != win_width || g_scaled_h != win_height)
    {
        g_scaled.resize(static_cast<size_t>(win_width) * win_height);
        scale_nn(g_pixels.data(), 800, 600, g_scaled.data(), win_width, win_height);
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

    // Fast bulk pixel upload: direct memcpy instead of slow XPutPixel
    if (g_image->bits_per_pixel == 32)
    {
        for (int y = 0; y < win_height; ++y)
        {
            uint32_t* row =
                reinterpret_cast<uint32_t*>(g_image->data + static_cast<size_t>(y) * g_image->bytes_per_line);
            const uint32_t* src = g_scaled.data() + static_cast<size_t>(y) * win_width;
            memcpy(row, src, static_cast<size_t>(win_width) * sizeof(uint32_t));
        }
    }
    else
    {
        // Fallback: slow XPutPixel for non-32bpp (unlikely on modern systems)
        for (int y = 0; y < win_height; ++y)
            for (int x = 0; x < win_width; ++x)
                XPutPixel(g_image, x, y, g_scaled[static_cast<size_t>(y) * win_width + x]);
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
                if (g_pixels.empty())
                    InitPixelData(800, 600);
            }
            else if (e.type == DestroyNotify)
            {
                if (g_image)
                    XDestroyImage(g_image);
                if (g_pixmap)
                    XFreePixmap(g_display, g_pixmap);
                XFreeGC(g_display, g_gc);
                XCloseDisplay(g_display);
                return 0;
            }
        }

        // Batch: only one StretchBlit per event-loop iteration
        if (!g_pixels.empty() && g_win_width > 0 && g_win_height > 0)
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

        if (!g_pixels.empty() && g_win_width > 0 && g_win_height > 0)
            StretchBlit(g_win_width, g_win_height);
    }

    return 0;
}
