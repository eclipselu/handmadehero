#include <cmath>
#include <cstdint>
#include <dsound.h>
#include <math.h>
#include <windows.h>
#include <xinput.h>

#define PI 3.14159265358979323846

#define global        static
#define local_persist static
#define internal      static

typedef float  float32_t;
typedef double float64_t;

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

struct win32_sound_output {
    int      samples_per_second;
    int      tone_hz;
    int16_t  tone_volume;
    uint32_t running_sample_idx;
    int      wave_period;
    int      bytes_per_sample;
    int      secondary_buffer_size;
};

internal void
Win32FillSoundBuffer(win32_sound_output* sound_output, DWORD byte_to_lock, DWORD bytes_to_write) {
    VOID *region1, *region2;
    DWORD region1_size, region2_size;
    // lock
    HRESULT hr = g_dsound_secondary_buffer->Lock(
        byte_to_lock, bytes_to_write, &region1, &region1_size, &region2, &region2_size, 0);
    if (SUCCEEDED(hr)) {
        // TODO: assert region sizes

        // trying to write to the locked regions
        DWORD    region1_sample_count = region1_size / sound_output->bytes_per_sample;
        int16_t* sample_out           = (int16_t*)region1;
        for (DWORD sample_idx = 0; sample_idx < region1_sample_count; ++sample_idx) {
            float32_t t          = 2.0 * PI * sound_output->running_sample_idx / (float32_t)sound_output->wave_period;
            int16_t   sample_val = (int16_t)(sin(t) * sound_output->tone_volume);

            ++sound_output->running_sample_idx;

            // left
            *sample_out = sample_val;
            ++sample_out;
            // right
            *sample_out = sample_val;
            ++sample_out;
        }
        DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
        sample_out                 = (int16_t*)region2;
        for (DWORD sample_idx = 0; sample_idx < region2_sample_count; ++sample_idx) {
            float32_t t          = 2.0 * PI * sound_output->running_sample_idx / (float32_t)sound_output->wave_period;
            int16_t   sample_val = (int16_t)(sin(t) * sound_output->tone_volume);

            ++sound_output->running_sample_idx;

            // left
            *sample_out = sample_val;
            ++sample_out;
            // right
            *sample_out = sample_val;
            ++sample_out;
        }

        g_dsound_secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
    }
}

// internal void
// TestDSoundWaveOutput(int samples_per_second, int bytes_per_sample, int secondary_buffer_size) {
//     }

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
            // Since we have 2 channels, and each bits per sample is 16, the buffer will look like:
            // [Left, Right] [Left, Right] ...
            // Left or Right has 16 bits, each left right tuple is called a sample

            win32_sound_output sound_output = {};
            sound_output.samples_per_second = 48000;
            // how many repeatable patterns, this is middle C in piano 261.6 hz
            sound_output.tone_hz            = 256;
            sound_output.tone_volume        = 6000;
            sound_output.running_sample_idx = 0;
            sound_output.wave_period        = sound_output.samples_per_second /
                                       sound_output.tone_hz; // how many samples do we need to fill in a pattern
            sound_output.bytes_per_sample      = sizeof(int16_t) * 2; // 2 channels, one channel is 16 bits
            sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;

            Win32InitDSound(window_handle, sound_output.samples_per_second, sound_output.secondary_buffer_size);
            Win32FillSoundBuffer(&sound_output, 0, sound_output.secondary_buffer_size);
            g_dsound_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

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

                        if (a_button) {
                            sound_output.tone_hz     = 512;
                            sound_output.wave_period = sound_output.samples_per_second / sound_output.tone_hz;
                        }
                        if (b_button) {
                            sound_output.tone_hz     = 256;
                            sound_output.wave_period = sound_output.samples_per_second / sound_output.tone_hz;
                        }

                        if (back) {
                            g_app_running = false;
                        }

                    } else {
                    }
                }

                // Test dsound output
                // both cursors are in bytes
                DWORD   play_cursor;
                DWORD   write_cursor;
                HRESULT hr = g_dsound_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor);
                if (SUCCEEDED(hr)) {
                    // where do we want start to lock
                    DWORD byte_to_lock = (sound_output.running_sample_idx * sound_output.bytes_per_sample) %
                                         sound_output.secondary_buffer_size;
                    DWORD bytes_to_write = 0;
                    if (byte_to_lock > play_cursor) {
                        // [xxx play ... lock xxx ]
                        bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock;
                        bytes_to_write += play_cursor;
                    } else {
                        // [... lock xxx play ... ]
                        bytes_to_write = play_cursor - byte_to_lock;
                    }
                    Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write);
                }

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
