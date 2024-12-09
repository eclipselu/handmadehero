#include <cstdint>
#include <windows.h>

#define global        static
#define local_persist static
#define internal      static

struct Win32_Offscreen_Buffer {
    BITMAPINFO info;
    void*      memory;
    int        width;
    int        height;
    int        bytes_per_pixel;
};

struct Win32_Window_Dimension {
    int width;
    int height;
};

/// Global variables
global bool                   g_app_running;
global Win32_Offscreen_Buffer g_backbuffer;

internal Win32_Window_Dimension
Win32GetWindowDimension(HWND window) {
    Win32_Window_Dimension dimension;

    RECT client_rect;
    GetClientRect(window, &client_rect);

    dimension.width  = client_rect.right - client_rect.left;
    dimension.height = client_rect.bottom - client_rect.top;
    return dimension;
}

internal void
Win32RenderBitmap(Win32_Offscreen_Buffer buffer, int x_offset, int y_offset) {
    uint8_t* row = (uint8_t*)buffer.memory;
    for (int y = 0; y < buffer.height; ++y) {
        uint32_t* pixel = (uint32_t*)row;

        for (int x = 0; x < buffer.width; ++x) {
            // byte order: BB GG RR 00

            uint8_t blue  = (uint8_t)(y + y_offset);
            uint8_t green = (uint8_t)(x + x_offset);
            uint8_t red   = (uint8_t)(x + y + x_offset + y_offset);
            *pixel        = (red << 16) | (green << 8) | blue;
            ++pixel;
        }

        row += buffer.bytes_per_pixel * buffer.width;
    }
}

internal void
Win32ResizeDIBSection(Win32_Offscreen_Buffer* buffer, int width, int height) {

    if (buffer->memory) {
        // maybe we can use MEM_DECOMMIT
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
        // VirtualProtect?
    }

    buffer->width           = width;
    buffer->height          = height;
    buffer->bytes_per_pixel = 4;

    buffer->info.bmiHeader.biSize  = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = width;
    // If biHeight is negative, the bitmap is a top-down DIB with the origin at the upper left
    // corner.
    buffer->info.bmiHeader.biHeight      = -height;
    buffer->info.bmiHeader.biPlanes      = 1;  // must be 1
    buffer->info.bmiHeader.biBitCount    = 32; // alignment?
    buffer->info.bmiHeader.biCompression = BI_RGB;

    int bitmap_memory_size = width * height * buffer->bytes_per_pixel;
    buffer->memory         = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32DisplayBufferInWindow(
    HDC                    device_ctx,
    int                    window_width,
    int                    window_height,
    Win32_Offscreen_Buffer buffer,
    int                    x,
    int                    y,
    int                    width,
    int                    height) {

    // stretch buffer memory given the buffer size to window size.
    // TODO: aspect ratio correction during resize
    StretchDIBits(
        device_ctx,
        0,
        0,
        window_width,
        window_height,
        0,
        0,
        buffer.width,
        buffer.height,
        buffer.memory,
        &buffer.info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

LRESULT CALLBACK
MainWindowCallback(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    switch (message) {
    case WM_SIZE: {
        // Win32_Window_Dimension dimension = Win32GetWindowDimension(window);
        // Win32ResizeDIBSection(&g_backbuffer, dimension.width, dimension.height);

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
        Win32_Window_Dimension dimension = Win32GetWindowDimension(window);
        Win32DisplayBufferInWindow(
            device_ctx, dimension.width, dimension.height, g_backbuffer, x, y, width, height);

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

    Win32ResizeDIBSection(&g_backbuffer, 1280, 720);

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

                Win32RenderBitmap(g_backbuffer, x_offset, y_offset);

                HDC                    device_ctx = GetDC(window_handle);
                Win32_Window_Dimension dimension  = Win32GetWindowDimension(window_handle);
                Win32DisplayBufferInWindow(
                    device_ctx,
                    dimension.width,
                    dimension.height,
                    g_backbuffer,
                    0,
                    0,
                    dimension.width,
                    dimension.height);
                ReleaseDC(window_handle, device_ctx);

                ++x_offset;
                ++y_offset;
            }
        }
    } else {
        // handle error
    }
    return 0;
}
