#include <windows.h>

LRESULT MainWindowCallback(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    switch (message) {
    case WM_SIZE: {
        OutputDebugStringA("WM_SIZE\n");
    } break;

    case WM_DESTROY: {
        OutputDebugStringA("WM_DESTROY Quitting\n");
        PostQuitMessage(0);
    } break;

    case WM_CLOSE: {
        OutputDebugStringA("WM_CLOSE\n");
        DestroyWindow(window);
    } break;

    case WM_ACTIVATEAPP: {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC         device_ctx = BeginPaint(window, &paint);

        LONG x      = paint.rcPaint.left;
        LONG y      = paint.rcPaint.top;
        LONG width  = paint.rcPaint.right - paint.rcPaint.left;
        LONG height = paint.rcPaint.bottom - paint.rcPaint.top;

        static DWORD operation = WHITENESS;
        PatBlt(device_ctx, x, y, width, height, operation);
        if (operation == WHITENESS) {
            operation = BLACKNESS;
        } else {
            operation = WHITENESS;
        }

        EndPaint(window, &paint);
    } break;

    default: {
        // OutputDebugStringA("default\n");
        // default window callback
        // handles WM_CLOSE -> destroy the window
        result = DefWindowProc(window, message, wparam, lparam);
    } break;
    }
    return result;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    WNDCLASSA windowClass = {};

    windowClass.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    windowClass.lpfnWndProc   = MainWindowCallback;
    windowClass.hInstance     = instance;
    windowClass.lpszClassName = "Handmade Windowclass";

    if (RegisterClassA(&windowClass)) {
        HWND window_handle = CreateWindowExA(
            0,
            windowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            // size and position
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            instance,
            0);

        if (window_handle) {
            // not necessary if we have WS_VISIBLE set.
            // ShowWindow(window_handle, show_cmd);

            MSG message = {};
            while (GetMessage(&message, 0, 0, 0) > 0) {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
        }
    } else {
        // handle error
    }
    return 0;
}
