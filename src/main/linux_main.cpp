// src/main/linux_main.cpp

#include <X11/Xlib.h>
#include <cstdlib>

int main()
{
    // Open Display
    Display* display = XOpenDisplay(nullptr);
    if (!display)
        return 1;

    int screen = DefaultScreen(display);

    // Create simple window
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 800, 600, 0, 0, 0);

    XSelectInput(display, window, ExposureMask | StructureNotifyMask);
    XMapWindow(display, window);

    // Event loop
    while (true)
    {
        XEvent e;
        XNextEvent(display, &e);

        if (e.type == Expose)
        {
            // Minimal draw: flush on expose
            XFlush(display);
        }
        else if (e.type == DestroyNotify)
        {
            break;
        }
    }

    XCloseDisplay(display);
    return 0;
}
