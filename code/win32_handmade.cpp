#include <cstdint>
#include <dsound.h>
#include <windows.h>
#include <xinput.h>

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
global LPDIRECTSOUNDBUFFER    g_dsound_secondary_buffer;

/// Dynamically loading XInput functions
// NOTE: define x_input_get_state as a function type, same as XInputGetState's signature
#define XINPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef XINPUT_GET_STATE(x_input_get_state);

// NOTE: define a stub function and make a global function pointer point to it
XINPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global x_input_get_state* XInputGetState_ = XInputGetStateStub;

// NOTE: Make XInputGetState point to the global function pointer that points to stub
#define XInputGetState XInputGetState_

#define XINPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef XINPUT_SET_STATE(x_input_set_state);
XINPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DSOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef DSOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput(void) {
    HMODULE XInputLib = LoadLibraryA("XInput1_3.dll");
    if (XInputLib) {
        XInputGetState_ = (x_input_get_state*)GetProcAddress(XInputLib, "XInputGetState");
        XInputSetState_ = (x_input_set_state*)GetProcAddress(XInputLib, "XInputSetState");
    }
}

internal void
Win32InitDSound(HWND window, int32_t samples_per_second, int32_t buffer_size) {
    // NOTE: Load the library
    HMODULE DSoundLib = LoadLibraryA("dsound.dll");
    if (DSoundLib) {
        // NOTE: Get a DirectSound object
        direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLib, "DirectSoundCreate");
        LPDIRECTSOUND        direct_sound;

        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &direct_sound, 0))) {
            WAVEFORMATEX wave_format    = {};
            wave_format.wFormatTag      = WAVE_FORMAT_PCM;
            wave_format.nChannels       = 2;
            wave_format.nSamplesPerSec  = samples_per_second;
            wave_format.wBitsPerSample  = 16;
            wave_format.nBlockAlign     = wave_format.nChannels * wave_format.wBitsPerSample / 8;
            wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;

            if (SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
                // NOTE: Create a primary buffer (old school way of doing sound stuff, still need to set the mode)
                //
                // When creating a primary buffer, applications must set the dwBufferBytes member to zero. DirectSound
                // will determine the best buffer size for the particular sound device in use
                DSBUFFERDESC buffer_desc = {};
                buffer_desc.dwFlags      = DSBCAPS_PRIMARYBUFFER;
                buffer_desc.dwSize       = sizeof(buffer_desc);

                LPDIRECTSOUNDBUFFER primary_buffer;
                if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_desc, &primary_buffer, 0))) {
                    if (SUCCEEDED(primary_buffer->SetFormat(&wave_format))) {
                        OutputDebugStringA("Primary buffer format was set\n");
                    } else {
                        OutputDebugStringA("Failed to set primary buffer format\n");
                    }

                } else {
                    // TODO: diagnostic
                }
            }

            // NOTE: Create a secondary buffer
            DSBUFFERDESC buffer_desc  = {};
            buffer_desc.dwFlags       = 0;
            buffer_desc.dwSize        = sizeof(buffer_desc);
            buffer_desc.dwBufferBytes = buffer_size;
            buffer_desc.lpwfxFormat   = &wave_format;

            if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_desc, &g_dsound_secondary_buffer, 0))) {
                OutputDebugStringA("Secondary buffer was created\n");
            } else {
                OutputDebugStringA("failed to create Secondary buffer\n");
            }
        } else {
            // TODO: diagnostics
        }
    }
}

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
Win32RenderBitmap(Win32_Offscreen_Buffer* buffer, int x_offset, int y_offset) {
    uint8_t* row = (uint8_t*)buffer->memory;
    for (int y = 0; y < buffer->height; ++y) {
        uint32_t* pixel = (uint32_t*)row;

        for (int x = 0; x < buffer->width; ++x) {
            // byte order: BB GG RR 00

            uint8_t blue  = (uint8_t)(y + y_offset);
            uint8_t green = (uint8_t)(x + x_offset);
            uint8_t red   = (uint8_t)(x + y + x_offset + y_offset);
            *pixel        = (red << 16) | (green << 8) | blue;
            ++pixel;
        }

        row += buffer->bytes_per_pixel * buffer->width;
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
    // need both reserve and commit
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32DisplayBufferInWindow(HDC device_ctx, int window_width, int window_height, Win32_Offscreen_Buffer buffer) {

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

internal LRESULT CALLBACK
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

    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_KEYDOWN: {
        uint32_t vk_code      = wparam;
        bool     alt_key_down = (lparam & (1 << 29)) != 0;
        if (vk_code == VK_F4 && alt_key_down) {
            g_app_running = false;
        }
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
        Win32DisplayBufferInWindow(device_ctx, dimension.width, dimension.height, g_backbuffer);

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

internal void
TestDSoundSquareWaveOutput(int samples_per_second, int bytes_per_sample, int secondary_buffer_size) {
    // NOTE: test DirectSound output
    // Since we have 2 channels, and each bits per sample is 16, the buffer will look like:
    // [Left, Right] [Left, Right] ...
    // Left or Right has 16 bits, each left right tuple is called a sample

    int      tone_hz                 = 256; // how many repeatable patterns, this is middle C in piano 261.6 hz
    int16_t  tone_volume             = 6000;
    uint32_t running_sample_idx      = 0;
    int      square_wave_period      = samples_per_second / tone_hz; // how many samples do we need to fill in a pattern
    int      half_square_wave_period = square_wave_period / 2;       // flip every half cycle

    g_dsound_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);
    // both cursors are in bytes
    DWORD   play_cursor;
    DWORD   write_cursor;
    HRESULT hr = g_dsound_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor);
    if (SUCCEEDED(hr)) {
        // where do we want start to lock
        DWORD lock_cursor = running_sample_idx * bytes_per_sample % secondary_buffer_size;
        DWORD bytes_to_write;
        if (lock_cursor < play_cursor) {
            // [... lock xxx play ... ]
            bytes_to_write = play_cursor - lock_cursor;
        } else if (lock_cursor > play_cursor) {
            // [xxx play ... lock xxx ]
            bytes_to_write = secondary_buffer_size - lock_cursor;
            bytes_to_write += play_cursor;
        } else {
            // lock_cursor == play_cursor
            bytes_to_write = play_cursor - lock_cursor;
        }

        VOID *region1, *region2;
        DWORD region1_size, region2_size;
        // lock
        HRESULT hr = g_dsound_secondary_buffer->Lock(
            lock_cursor, bytes_to_write, &region1, &region1_size, &region2, &region2_size, 0);
        if (SUCCEEDED(hr)) {
            // TODO: assert region sizes

            // trying to write to the locked regions
            DWORD    region1_sample_count = region1_size / bytes_per_sample;
            int16_t* sample_out           = (int16_t*)region1;
            for (DWORD sample_idx = 0; sample_idx < region1_sample_count; ++sample_idx) {
                int16_t sample_val = (running_sample_idx / half_square_wave_period) % 2 ? tone_volume : -tone_volume;
                ++running_sample_idx;

                // left
                *sample_out = sample_val;
                ++sample_out;
                // right
                *sample_out = sample_val;
                ++sample_out;
            }
            DWORD region2_sample_count = region2_size / bytes_per_sample;
            sample_out                 = (int16_t*)region2;
            for (DWORD sample_idx = 0; sample_idx < region2_sample_count; ++sample_idx) {
                int16_t sample_val = (running_sample_idx / half_square_wave_period) % 2 ? tone_volume : -tone_volume;
                ++running_sample_idx;

                // left
                *sample_out = sample_val;
                ++sample_out;
                // right
                *sample_out = sample_val;
                ++sample_out;
            }

            g_dsound_secondary_buffer->Unlock(&region1, region1_size, &region2, region2_size);
        }
    }
}

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    Win32LoadXInput();

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

            HDC device_ctx = GetDC(window_handle);
            int x_offset   = 0;
            int y_offset   = 0;
            MSG message    = {};

            // sound
            int samples_per_second    = 48000;
            int bytes_per_sample      = sizeof(int16_t) * 2; // 2 channels, one channel is 16 bits
            int secondary_buffer_size = samples_per_second * bytes_per_sample;
            int tone_hz               = 256; // piano middle C is 261.63Hz
            Win32InitDSound(window_handle, samples_per_second, secondary_buffer_size);

            while (g_app_running) {
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        g_app_running = false;
                        break;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                // deal with controller
                for (DWORD controller_idx = 0; controller_idx < XUSER_MAX_COUNT; ++controller_idx) {
                    XINPUT_STATE controller_state = {};
                    if (XInputGetState(controller_idx, &controller_state) == ERROR_SUCCESS) {
                        XINPUT_GAMEPAD* gamepad = &controller_state.Gamepad;

                        bool dpad_up        = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool dpad_down      = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool dpad_left      = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool dpad_right     = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool start          = gamepad->wButtons & XINPUT_GAMEPAD_START;
                        bool back           = gamepad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool left_shoulder  = gamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool right_shoulder = gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool a_button       = gamepad->wButtons & XINPUT_GAMEPAD_A;
                        bool b_button       = gamepad->wButtons & XINPUT_GAMEPAD_B;
                        bool x_button       = gamepad->wButtons & XINPUT_GAMEPAD_X;
                        bool y_button       = gamepad->wButtons & XINPUT_GAMEPAD_Y;

                        int16_t stick_lx = gamepad->sThumbLX;
                        int16_t stick_ly = gamepad->sThumbLY;
                        int16_t stick_rx = gamepad->sThumbRX;
                        int16_t stick_ry = gamepad->sThumbRY;

                        if (a_button) {
                            OutputDebugStringA("Button A\n");
                        }
                        if (b_button) {
                            OutputDebugStringA("Button B\n");
                        }
                        if (x_button) {
                            OutputDebugStringA("Button X\n");
                        }
                        if (y_button) {
                            OutputDebugStringA("Button Y\n");
                        }

                        /// NOTE: play around with input to manipulate the image displaying
                        if (dpad_up) {
                            --y_offset;
                        }
                        if (dpad_down) {
                            ++y_offset;
                        }
                        if (dpad_left) {
                            --x_offset;
                        }
                        if (dpad_right) {
                            ++x_offset;
                        }

                        if (back) {
                            g_app_running = false;
                        }

                    } else {
                    }
                }

                XINPUT_VIBRATION vibration = {};
                vibration.wLeftMotorSpeed  = 32768;
                vibration.wRightMotorSpeed = 32768;
                XInputSetState(0, &vibration);

                // Test dsound output
                TestDSoundSquareWaveOutput(samples_per_second, bytes_per_sample, secondary_buffer_size);

                Win32RenderBitmap(&g_backbuffer, x_offset, y_offset);

                Win32_Window_Dimension dimension = Win32GetWindowDimension(window_handle);
                Win32DisplayBufferInWindow(device_ctx, dimension.width, dimension.height, g_backbuffer);

                ++x_offset;
                ++y_offset;
            }
        }
    } else {
        // handle error
    }
    return 0;
}
