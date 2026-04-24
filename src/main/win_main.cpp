#include <vector>
#include <windows.h>


// Global variables
static int g_width = 0;
static int g_height = 0;
static std::vector<RGBQUAD> g_pixels;
static BITMAPINFO g_bmi = {};

// Initialize DIB data
void InitDIBData(int width, int height)
{
    g_width = width;
    g_height = height;

    // Allocate and fill pixel data with red gradient
    g_pixels.resize(static_cast<size_t>(width) * height);
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            auto& p = g_pixels[static_cast<size_t>(i) * width + j];
            p.rgbRed = static_cast<BYTE>(j * 255 / width);
            p.rgbGreen = 0;
            p.rgbBlue = 0;
            p.rgbReserved = 255;
        }
    }

    // Setup BITMAPINFO
    g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = width;
    g_bmi.bmiHeader.biHeight = -height; // top-down
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;
}

// reference for window: https://learn.microsoft.com/en-us/windows/win32/winmsg/windows
// reference for this callback: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        // Set timer for ~60 FPS redraw
        SetTimer(hwnd, 1, 16, NULL);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        if (wParam == 1)
        {
            // Trigger repaint every frame
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            RECT rect{};
            GetClientRect(hwnd, &rect);

            // Initialize on first paint
            if (g_pixels.empty())
            {
                InitDIBData(rect.right - rect.left, rect.bottom - rect.top);
            }

            HDC hdc = BeginPaint(hwnd, &ps);
            SetStretchBltMode(hdc, COLORONCOLOR);
            // Draw using StretchDIBits
            // ref: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits
            StretchDIBits(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, 0, 0, g_width, g_height,
                          g_pixels.data(), &g_bmi, DIB_RGB_COLORS, SRCCOPY);

            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    const char CLASS_NAME[] = "MyWindowClass";

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, CLASS_NAME, "WinMain Window Example", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL)
    {
        MessageBoxA(NULL, "CreateWindowEx failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
