#include <windows.h>

#define global        static
#define local_persist static
#define internal      static

/// Global variables
global bool       g_app_running;
global BITMAPINFO g_bitmap_info;
global void*      g_bitmap_memory;
global HBITMAP    g_bitmap_handle;
global HDC        g_device_ctx;

internal void
ResizeDIBSection(int width, int height) {
    if (g_bitmap_handle) {
        DeleteObject(g_bitmap_handle);
    }

    if (!g_device_ctx) {
        // TODO: should we recreate this?
        g_device_ctx = CreateCompatibleDC(0);
    }

    g_bitmap_info.bmiHeader.biSize        = sizeof(g_bitmap_info.bmiHeader);
    g_bitmap_info.bmiHeader.biWidth       = width;
    g_bitmap_info.bmiHeader.biHeight      = height;
    g_bitmap_info.bmiHeader.biBitCount    = 32; // alignment?
    g_bitmap_info.bmiHeader.biCompression = BI_RGB;

    g_bitmap_handle =
        CreateDIBSection(g_device_ctx, &g_bitmap_info, DIB_RGB_COLORS, &g_bitmap_memory, NULL, 0);
}

internal void
UpdateWindow(HDC device_ctx, int x, int y, int width, int height) {
    // TODO
    StretchDIBits(
        device_ctx,
        x,
        y,
        width,
        height,
        x,
        y,
        width,
        height,
        g_bitmap_memory,
        &g_bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

LRESULT CALLBACK
MainWindowCallback(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    switch (message) {
    case WM_SIZE: {
        OutputDebugStringA("WM_SIZE\n");

        // Create the buffer when window is resized, then paint the buffer under WM_PAINT
        RECT client_rect = {};
        GetClientRect(window, &client_rect);
        int width  = client_rect.right - client_rect.left;
        int height = client_rect.bottom - client_rect.top;
        ResizeDIBSection(width, height);

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

        // redraw the window using the back buffer.
        UpdateWindow(device_ctx, x, y, width, height);

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

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
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
            g_app_running = true;
            // not necessary if we have WS_VISIBLE set.
            // ShowWindow(window_handle, show_cmd);

            MSG message = {};
            while (g_app_running) {
                if (GetMessage(&message, 0, 0, 0) > 0) {
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                } else {
                    break;
                }
            }
        }
    } else {
        // handle error
    }
    return 0;
}
