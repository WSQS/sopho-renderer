#include <windows.h>

// reference for window: https://learn.microsoft.com/en-us/windows/win32/winmsg/windows
// reference for this callback: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            const char* text = "Hello from WinMain!";
            TextOutA(hdc, 20, 20, text, lstrlenA(text));
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
