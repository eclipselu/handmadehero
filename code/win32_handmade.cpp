#include <cstdint>
#include <windows.h>

#define global        static
#define local_persist static
#define internal      static

/// Global variables
global bool       g_app_running;
global BITMAPINFO g_bitmap_info;
global void*      g_bitmap_memory;
global int        g_bitmap_width;
global int        g_bitmap_height;

internal void
RenderBitmap(int x_offset, int y_offset) {
    uint8_t* row = (uint8_t*)g_bitmap_memory;
    for (int y = 0; y < g_bitmap_height; ++y) {
        uint32_t* pixel = (uint32_t*)row;

        for (int x = 0; x < g_bitmap_width; ++x) {
            // byte order: BB GG RR 00

            uint8_t blue  = (uint8_t)(y + y_offset);
            uint8_t green = (uint8_t)(x + x_offset);
            uint8_t red   = (uint8_t)(x + y + x_offset + y_offset);
            *pixel        = (red << 16) | (green << 8) | blue;
            ++pixel;
        }

        row += 4 * g_bitmap_width;
    }
}

internal void
ResizeDIBSection(int width, int height) {
    int bitmap_memory_size = width * height * 4;

    if (g_bitmap_memory) {
        // maybe we can use MEM_DECOMMIT
        VirtualFree(g_bitmap_memory, 0, MEM_RELEASE);
        // VirtualProtect?
    }

    g_bitmap_width  = width;
    g_bitmap_height = height;

    g_bitmap_info.bmiHeader.biSize  = sizeof(g_bitmap_info.bmiHeader);
    g_bitmap_info.bmiHeader.biWidth = width;
    // If biHeight is negative, the bitmap is a top-down DIB with the origin at the upper left
    // corner.
    g_bitmap_info.bmiHeader.biHeight      = -height;
    g_bitmap_info.bmiHeader.biPlanes      = 1;  // must be 1
    g_bitmap_info.bmiHeader.biBitCount    = 32; // alignment?
    g_bitmap_info.bmiHeader.biCompression = BI_RGB;

    g_bitmap_memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);

    RenderBitmap(64, 64);
}

internal void
UpdateWindow(HDC device_ctx, RECT* window_rect, int x, int y, int width, int height) {
    int window_width  = window_rect->right - window_rect->left;
    int window_height = window_rect->bottom - window_rect->top;

    StretchDIBits(
        device_ctx,
        0,
        0,
        g_bitmap_width,
        g_bitmap_height,
        0,
        0,
        window_width,
        window_height,
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
        RECT client_rect = {};
        GetClientRect(window, &client_rect);
        UpdateWindow(device_ctx, &client_rect, x, y, width, height);

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

    windowClass.style         = CS_VREDRAW | CS_HREDRAW;
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

            int x_offset = 0;
            int y_offset = 0;
            MSG message  = {};
            while (g_app_running) {
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        g_app_running = false;
                        break;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                RenderBitmap(x_offset, y_offset);

                HDC  device_ctx = GetDC(window_handle);
                RECT window_rect;
                GetClientRect(window_handle, &window_rect);
                int window_width  = window_rect.right - window_rect.left;
                int window_height = window_rect.bottom - window_rect.top;
                UpdateWindow(device_ctx, &window_rect, 0, 0, window_width, window_height);
                ++x_offset;
                ++y_offset;
            }
        }
    } else {
        // handle error
    }
    return 0;
}
