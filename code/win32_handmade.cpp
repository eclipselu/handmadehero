#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <dsound.h>
#include <intrin.h>
#include <math.h>
#include <windows.h>
#include <xinput.h>

#include "base.h"
#include "handmade.h"
#include "handmade.cpp"
#include "win32_handmade.h"

/*
TODO:
 - Save game locations
 - Getting a handle to our own executable file
 - Asset loading path
 - Threading
 - Raw input, support multiple keyboards
 - Sleep/timeBeginPeriod
 - Clipcursor for multimonitor support
 - fullscreen
 - WM_SETCURSOR
 - QueryCancelAutoplay
 - Not the active application: WM_ACTIVEAPP
 - Blt speed improvements
 - Hardware accleration (OpenGL/Direct3D)
 - Different Keyboard layout support
 */

#pragma intrinsic(__rdtsc)

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

internal Debug_Read_File_Result
DEBUGPlatformReadEntireFile(const char* file_name) {
    Debug_Read_File_Result result = {};

    HANDLE file_handle =
        CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER file_size;
        if (GetFileSizeEx(file_handle, &file_size)) {
            uint32_t file_size32 = SafeTruncateUint64(file_size.QuadPart);

            result.content = VirtualAlloc(0, file_size32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (result.content) {
                DWORD bytes_read = 0;
                if (ReadFile(file_handle, result.content, file_size32, &bytes_read, NULL) &&
                    (bytes_read == file_size32)) {
                    // NOTE: file read successfully
                    result.content_size = (uint32_t)bytes_read;
                } else {
                    // TODO: logging
                    DEBUGPlatformFreeFileMemory(result.content);
                    result.content = NULL;
                }
            } else {
                // TODO: logging
            }
        } else {
            // TODO: logging
        }

        CloseHandle(file_handle);
    } else {
        // TODO: logging
    }

    return result;
}

internal bool
DEBUGPlatformWriteEntireFile(const char* file_name, Debug_Read_File_Result read_result) {
    bool result = false;

    HANDLE file_handle = CreateFileA(file_name, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        DWORD bytes_written;

        if (WriteFile(file_handle, read_result.content, read_result.content_size, &bytes_written, NULL)) {
            result = (bytes_written == read_result.content_size);
        } else {
            // TODO: logging
        }
        CloseHandle(file_handle);
    } else {
        // TODO: logging
    }

    return result;
}

internal void
DEBUGPlatformFreeFileMemory(void* memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
}

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
                // When creating a primary buffer, applications must set the dwBufferBytes member to zero.
                // DirectSound will determine the best buffer size for the particular sound device in use
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
        result = DefWindowProcA(window, message, wparam, lparam);
    } break;
    }
    return result;
}

internal void
Win32FillSoundBuffer(
    Win32_Sound_Output*       sound_output,
    DWORD                     byte_to_lock,
    DWORD                     bytes_to_write,
    Game_Sound_Output_Buffer* source_buffer) {

    VOID *region1, *region2;
    DWORD region1_size, region2_size;
    // lock
    HRESULT hr = g_dsound_secondary_buffer->Lock(
        byte_to_lock, bytes_to_write, &region1, &region1_size, &region2, &region2_size, 0);
    if (SUCCEEDED(hr)) {
        // TODO: assert region sizes

        // trying to write to the locked regions
        DWORD    region1_sample_count = region1_size / sound_output->bytes_per_sample;
        int16_t* source_sample        = source_buffer->samples;
        int16_t* sample_out           = (int16_t*)region1;
        for (DWORD sample_idx = 0; sample_idx < region1_sample_count; ++sample_idx) {
            // left
            *sample_out = *source_sample;
            ++sample_out;
            ++source_sample;
            // right
            *sample_out = *source_sample;
            ++sample_out;
            ++source_sample;

            ++sound_output->running_sample_idx;
        }
        DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
        sample_out                 = (int16_t*)region2;
        for (DWORD sample_idx = 0; sample_idx < region2_sample_count; ++sample_idx) {
            // left
            *sample_out = *source_sample;
            ++sample_out;
            ++source_sample;
            // right
            *sample_out = *source_sample;
            ++sample_out;
            ++source_sample;

            ++sound_output->running_sample_idx;
        }

        g_dsound_secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
    }
}

internal void
Win32ClearSoundBuffer(Win32_Sound_Output* sound_output) {
    VOID *region1, *region2;
    DWORD region1_size, region2_size;
    // lock
    HRESULT hr = g_dsound_secondary_buffer->Lock(
        0, sound_output->secondary_buffer_size, &region1, &region1_size, &region2, &region2_size, 0);
    if (SUCCEEDED(hr)) {
        uint8_t* sample_out = (uint8_t*)region1;
        for (DWORD sample_idx = 0; sample_idx < region1_size; ++sample_idx) {
            *sample_out = 0;
            ++sample_out;
        }
        sample_out = (uint8_t*)region2;
        for (DWORD sample_idx = 0; sample_idx < region2_size; ++sample_idx) {
            *sample_out = 0;
            ++sample_out;
        }
        g_dsound_secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
    }
}
// internal void
// TestDSoundWaveOutput(int samples_per_second, int bytes_per_sample, int secondary_buffer_size) {
//     }

internal void
Win32ProcessXInputDigitalButton(
    WORD xinput_button_state, DWORD button_bit, Game_Button_State* old_state, Game_Button_State* new_state) {

    new_state->ended_down            = (xinput_button_state & button_bit) == button_bit;
    new_state->half_transition_count = (new_state->ended_down != old_state->ended_down) ? 1 : 0;
}

internal inline float32_t
Win32NormalizeAnalogStickInput(int16_t stick) {
    // [-32768, 32767] -> [-1, 1]
    return ((float32_t)(stick) + 32768.0f) * 2 / 65536.0f - 1;
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

    LARGE_INTEGER perf_count_freq_result;
    QueryPerformanceFrequency(&perf_count_freq_result);
    int64_t perf_count_freq = perf_count_freq_result.QuadPart;

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

            Win32_Sound_Output sound_output    = {};
            sound_output.samples_per_second    = 48000;
            sound_output.running_sample_idx    = 0;
            sound_output.bytes_per_sample      = sizeof(int16_t) * 2; // 2 channels, one channel is 16 bits
            sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
            sound_output.latency_sample_count  = sound_output.samples_per_second / 15;

            Win32InitDSound(window_handle, sound_output.samples_per_second, sound_output.secondary_buffer_size);
            Win32ClearSoundBuffer(&sound_output);
            g_dsound_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

            // Game memory
            Game_Memory game_memory            = {};
            game_memory.permanent_storage_size = MegaBytes(64);
            game_memory.permanent_storage =
                VirtualAlloc(0, game_memory.permanent_storage_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            game_memory.transient_storage_size = GigaBytes((uint64_t)4);
            game_memory.transient_storage =
                VirtualAlloc(0, game_memory.transient_storage_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            // Game input
            Game_Input  game_inputs[2] = {};
            Game_Input* old_input      = &game_inputs[0];
            Game_Input* new_input      = &game_inputs[1];

            // Performance
            LARGE_INTEGER last_counter;
            QueryPerformanceCounter(&last_counter);
            uint64_t last_cycle_count = __rdtsc();

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
                int max_controller_count = XUSER_MAX_COUNT;
                if (max_controller_count > ArrayCount(new_input->controllers)) {
                    max_controller_count = ArrayCount(new_input->controllers);
                }
                for (DWORD controller_idx = 0; controller_idx < XUSER_MAX_COUNT; ++controller_idx) {
                    XINPUT_STATE           controller_state = {};
                    Game_Controller_Input* old_controller   = &old_input->controllers[controller_idx];
                    Game_Controller_Input* new_controller   = &new_input->controllers[controller_idx];

                    if (XInputGetState(controller_idx, &controller_state) == ERROR_SUCCESS) {
                        XINPUT_GAMEPAD* gamepad = &controller_state.Gamepad;

                        bool dpad_up    = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool dpad_down  = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool dpad_left  = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool dpad_right = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                        // XInput -> Game_Input
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons, XINPUT_GAMEPAD_START, &old_controller->start, &new_controller->start);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons, XINPUT_GAMEPAD_BACK, &old_controller->back, &new_controller->back);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons,
                            XINPUT_GAMEPAD_LEFT_SHOULDER,
                            &old_controller->left_shoulder,
                            &new_controller->left_shoulder);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons,
                            XINPUT_GAMEPAD_LEFT_SHOULDER,
                            &old_controller->right_shoulder,
                            &new_controller->right_shoulder);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons, XINPUT_GAMEPAD_X, &old_controller->left, &new_controller->left);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons, XINPUT_GAMEPAD_Y, &old_controller->up, &new_controller->up);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons, XINPUT_GAMEPAD_A, &old_controller->down, &new_controller->down);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons, XINPUT_GAMEPAD_B, &old_controller->right, &new_controller->right);

                        // sticks
                        new_controller->is_analog = true;
                        new_controller->start_x   = old_controller->end_x;
                        new_controller->start_y   = old_controller->end_y;
                        float32_t stick_lx        = Win32NormalizeAnalogStickInput(gamepad->sThumbLX);
                        float32_t stick_ly        = Win32NormalizeAnalogStickInput(gamepad->sThumbLY);
                        new_controller->min_x = new_controller->max_x = new_controller->end_x = stick_lx;
                        new_controller->min_y = new_controller->max_y = new_controller->end_y = stick_ly;

                        if (new_controller->back.ended_down) {
                            g_app_running = false;
                        }

                    } else {
                    }
                }

                Game_Offscreen_Buffer game_buffer = {};

                game_buffer.bytes_per_pixel = g_backbuffer.bytes_per_pixel;
                game_buffer.width           = g_backbuffer.width;
                game_buffer.height          = g_backbuffer.height;
                game_buffer.memory          = g_backbuffer.memory;

                // Test dsound output
                // both cursors are in bytes
                DWORD play_cursor;
                DWORD write_cursor;
                DWORD bytes_to_write = 0;
                DWORD byte_to_lock   = 0;

                HRESULT hr = g_dsound_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor);

                bool sound_is_valid = SUCCEEDED(hr);
                if (sound_is_valid) {
                    // where do we want start to lock
                    byte_to_lock = (sound_output.running_sample_idx * sound_output.bytes_per_sample) %
                                   sound_output.secondary_buffer_size;
                    DWORD target_cursor =
                        (play_cursor + sound_output.latency_sample_count * sound_output.bytes_per_sample) %
                        sound_output.secondary_buffer_size;
                    if (byte_to_lock > target_cursor) {
                        // [xxx play ... lock xxx ]
                        bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock;
                        bytes_to_write += target_cursor;
                    } else {
                        // [... lock xxx play ... ]
                        bytes_to_write = target_cursor - byte_to_lock;
                    }
                }

                Game_Sound_Output_Buffer sound_buffer = {};

                // NOTE: samples per second: 48000, we only need 1/30 of a second, 2 channels
                int16_t* samples =
                    (int16_t*)VirtualAlloc(0, 48000 * 2 * sizeof(int16_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                sound_buffer.samples_per_second = sound_output.samples_per_second;
                sound_buffer.sample_count       = bytes_to_write / sound_output.bytes_per_sample;
                sound_buffer.samples            = samples;
                GameUpdateAndRender(&game_memory, new_input, &game_buffer, &sound_buffer);

                if (sound_is_valid) {
                    Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
                }

                VirtualFree(samples, 0, MEM_RELEASE);

                Win32_Window_Dimension dimension = Win32GetWindowDimension(window_handle);
                Win32DisplayBufferInWindow(device_ctx, dimension.width, dimension.height, g_backbuffer);

                // swap old and new inputs
                Game_Input* temp = new_input;
                new_input        = old_input;
                old_input        = new_input;

                // timing
                LARGE_INTEGER end_counter;
                QueryPerformanceCounter(&end_counter);
                uint64_t end_cycle_count = __rdtsc();

                uint64_t  cycle_elapsed   = end_cycle_count - last_cycle_count;
                int64_t   counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
                float32_t ms_per_frame    = 1000.0f * counter_elapsed / perf_count_freq;
                float32_t fps             = 1.0f * perf_count_freq / counter_elapsed;
                float32_t mc_per_frame    = cycle_elapsed / 1000.0f / 1000.0f;

                char buffer[256];
                sprintf(buffer, "%.2f ms/f, %.2f fps, %.2f mc/f\n", ms_per_frame, fps, mc_per_frame);
                OutputDebugStringA(buffer);

                last_counter     = end_counter;
                last_cycle_count = end_cycle_count;
            }
        }
    } else {
        // handle error
    }
    return 0;
}
