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
global int64_t                g_perf_count_freq;

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

        case WM_CLOSE: {
            OutputDebugStringA("WM_CLOSE\n");
            DestroyWindow(window);
        } break;

        case WM_ACTIVATEAPP: {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_KEYDOWN: {
            Assert(!"Keyboard input should not be handled here!!");
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

internal void
Win32ProcessKeyboardMessage(Game_Button_State* new_state, bool is_down) {
    // BUG: this assertion fails when I keep pressing the same button and don't release
    Assert(new_state->ended_down != is_down);
    new_state->ended_down = is_down;
    ++new_state->half_transition_count;
}

internal void
Win32ProcessPendingMessages(Game_Controller_Input* keyboard_controller) {
    MSG message = {};
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
            case WM_QUIT: {
                g_app_running = false;
            } break;

            case WM_SYSKEYUP:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_KEYDOWN: {
                bool     was_down = (message.lParam & (1 << 30)) != 0;
                bool     is_down  = (message.lParam & (1 << 31)) == 0;
                uint32_t vk_code  = (uint32_t)message.wParam;

                // half transition
                if (was_down != is_down) {
                    switch (vk_code) {
                        case 'W': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->move_up, is_down);
                        } break;
                        case 'A': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->move_left, is_down);
                        } break;
                        case 'S': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->move_down, is_down);
                        } break;
                        case 'D': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->move_right, is_down);
                        } break;
                        case 'Q': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->left_shoulder, is_down);
                        } break;
                        case 'E': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->right_shoulder, is_down);
                        } break;
                        case VK_UP: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->action_up, is_down);
                        } break;
                        case VK_LEFT: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->action_left, is_down);
                        } break;
                        case VK_DOWN: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->action_down, is_down);
                        } break;
                        case VK_RIGHT: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->action_right, is_down);
                        } break;
                        case VK_ESCAPE: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->back, is_down);
                        } break;
                        case VK_SPACE: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->start, is_down);
                        } break;
                    }
                }

                bool alt_was_down = (message.lParam & (1 << 29)) != 0;
                if (vk_code == VK_F4 && alt_was_down) {
                    g_app_running = false;
                }
            } break;
            default: {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            } break;
        }
    }
}

internal void
Win32ProcessXInputDigitalButton(
    WORD xinput_button_state, DWORD button_bit, Game_Button_State* old_state, Game_Button_State* new_state) {

    new_state->ended_down            = (xinput_button_state & button_bit) == button_bit;
    new_state->half_transition_count = (new_state->ended_down != old_state->ended_down) ? 1 : 0;
}

// NOTE: See https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput for deadzone
inline float32_t
Win32NormalizeAnalogStickInput(int16_t stick, int16_t deadzone) {
    // [-32768, 32767] -> [-1, 1]
    float32_t normalized_input = 0.0f;
    if (stick < -deadzone || stick > deadzone) {
        normalized_input = ((float32_t)(stick) + 32768.0f) * 2 / 65536.0f - 1;
    }
    return normalized_input;
}

inline LARGE_INTEGER
Win32GetWallClock(void) {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

inline float32_t
Win32GetMilliSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
    float32_t ms_elapsed = 1000.0f * (float32_t)(end.QuadPart - start.QuadPart) / (float32_t)g_perf_count_freq;
    return ms_elapsed;
}

internal void
Win32DebugDrawVertical(Win32_Offscreen_Buffer* backbuffer, int x, int top, int bottom, uint32_t color) {
    int      pitch = backbuffer->width * backbuffer->bytes_per_pixel;
    uint8_t* pixel = (uint8_t*)backbuffer->memory + top * pitch + x * backbuffer->bytes_per_pixel;

    for (int y = top; y < bottom; ++y) {
        *(uint32_t*)pixel = color;
        pixel += pitch;
    }
}

internal void
Win32DebugSyncDisplay(
    Win32_Offscreen_Buffer*  backbuffer,
    int                      marker_count,
    Win32_Debug_Time_Marker* debug_time_markers,
    Win32_Sound_Output*      sound_output,
    float32_t                target_ms_per_frame) {

    int padx = 16;
    int pady = 16;

    int top    = pady;
    int bottom = backbuffer->height - pady;

    // Draw
    float32_t c = (float32_t)backbuffer->width / (float32_t)sound_output->secondary_buffer_size;
    for (int marker_idx = 0; marker_idx < marker_count; ++marker_idx) {
        Win32_Debug_Time_Marker marker = debug_time_markers[marker_idx];

        int play_cursor_x  = padx + (int)(c * marker.play_cursor);
        int write_cursor_x = padx + (int)(c * marker.write_cursor);
        Win32DebugDrawVertical(backbuffer, play_cursor_x, top, bottom, 0xFFFFFFFF);
        Win32DebugDrawVertical(backbuffer, write_cursor_x, top, bottom, 0x00FF00FF);
    }
}

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    LARGE_INTEGER perf_count_freq_result;
    QueryPerformanceFrequency(&perf_count_freq_result);
    g_perf_count_freq = perf_count_freq_result.QuadPart;

    bool sleep_is_granular = (bool)timeBeginPeriod(1);

    Win32LoadXInput();
    WNDCLASSA windowClass = {};

    Win32ResizeDIBSection(&g_backbuffer, 1280, 720);

    windowClass.style         = CS_VREDRAW | CS_HREDRAW;
    windowClass.lpfnWndProc   = MainWindowCallback;
    windowClass.hInstance     = instance;
    windowClass.lpszClassName = "Handmade Windowclass";

#define MONITOR_REFRESH_HZ      60
#define GAME_REFRESH_HZ         (MONITOR_REFRESH_HZ / 2)
#define AUDIO_LATENCY_IN_FRAMES 3

    float32_t target_ms_elapsed_per_frame = 1000.0f / (float32_t)GAME_REFRESH_HZ;
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

            // sound
            // Since we have 2 channels, and each bits per sample is 16, the buffer will look like:
            // [Left, Right] [Left, Right] ...
            // Left or Right has 16 bits, each left right tuple is called a sample

            Win32_Sound_Output sound_output    = {};
            sound_output.samples_per_second    = 48000;
            sound_output.running_sample_idx    = 0;
            sound_output.bytes_per_sample      = sizeof(int16_t) * 2; // 2 channels, one channel is 16 bits
            sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
            // 2 frames of delay
            sound_output.latency_sample_count =
                AUDIO_LATENCY_IN_FRAMES * sound_output.samples_per_second / GAME_REFRESH_HZ;

            Win32InitDSound(window_handle, sound_output.samples_per_second, sound_output.secondary_buffer_size);
            Win32ClearSoundBuffer(&sound_output);
            g_dsound_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

            // Game memory
            Game_Memory game_memory            = {};
            game_memory.permanent_storage_size = MegaBytes(64);
            game_memory.permanent_storage =
                VirtualAlloc(0, game_memory.permanent_storage_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            game_memory.transient_storage_size = GigaBytes(4);
            game_memory.transient_storage =
                VirtualAlloc(0, game_memory.transient_storage_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            // Game input
            Game_Input  game_inputs[2] = {};
            Game_Input* old_input      = &game_inputs[0];
            Game_Input* new_input      = &game_inputs[1];

            // Performance
            LARGE_INTEGER last_counter = Win32GetWallClock();
            QueryPerformanceCounter(&last_counter);
            uint64_t last_cycle_count = __rdtsc();

            int                     debug_time_marker_idx  = 0;
            Win32_Debug_Time_Marker debug_time_markers[15] = {0}; // game_refresh_hz / 2

            // Direct sound
            DWORD last_play_cursor = 0;
            bool  is_sound_valid   = false;

            while (g_app_running) {
                // keyboard controller
                Game_Controller_Input* old_keyboard_controller = &old_input->keyboard_controller;
                Game_Controller_Input* new_keyboard_controller = &new_input->keyboard_controller;

                // TODO: cannot zero out everying because the up/down key will be wrong.
                *new_keyboard_controller              = {};
                new_keyboard_controller->is_connected = true;
                for (int button_idx = 0; button_idx < ArrayCount(new_keyboard_controller->buttons); ++button_idx) {
                    new_keyboard_controller->buttons[button_idx].ended_down =
                        old_keyboard_controller->buttons[button_idx].ended_down;
                }
                Win32ProcessPendingMessages(new_keyboard_controller);

                // deal with keypad controllers
                int max_gamepad_controller_count = XUSER_MAX_COUNT;
                if (max_gamepad_controller_count > ArrayCount(new_input->gamepad_controllers)) {
                    max_gamepad_controller_count = ArrayCount(new_input->gamepad_controllers);
                }
                for (DWORD controller_idx = 0; controller_idx < XUSER_MAX_COUNT; ++controller_idx) {
                    XINPUT_STATE           controller_state = {};
                    Game_Controller_Input* old_controller   = &old_input->gamepad_controllers[controller_idx];
                    Game_Controller_Input* new_controller   = &new_input->gamepad_controllers[controller_idx];

                    if (XInputGetState(controller_idx, &controller_state) == ERROR_SUCCESS) {
                        new_controller->is_connected = true;
                        XINPUT_GAMEPAD* gamepad      = &controller_state.Gamepad;

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
                            gamepad->wButtons,
                            XINPUT_GAMEPAD_X,
                            &old_controller->action_left,
                            &new_controller->action_left);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons,
                            XINPUT_GAMEPAD_Y,
                            &old_controller->action_up,
                            &new_controller->action_up);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons,
                            XINPUT_GAMEPAD_A,
                            &old_controller->action_down,
                            &new_controller->action_down);
                        Win32ProcessXInputDigitalButton(
                            gamepad->wButtons,
                            XINPUT_GAMEPAD_B,
                            &old_controller->action_right,
                            &new_controller->action_right);

                        // sticks
                        new_controller->is_analog = true;
                        new_controller->stick_avg_x =
                            Win32NormalizeAnalogStickInput(gamepad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                        new_controller->stick_avg_y =
                            Win32NormalizeAnalogStickInput(gamepad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                        bool dpad_up    = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool dpad_down  = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool dpad_left  = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool dpad_right = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        if (dpad_up) {
                            new_controller->stick_avg_y = -1.0;
                            new_controller->is_analog   = false;
                        }
                        if (dpad_down) {
                            new_controller->stick_avg_y = 1.0;
                            new_controller->is_analog   = false;
                        }
                        if (dpad_left) {
                            new_controller->stick_avg_x = -1.0;
                            new_controller->is_analog   = false;
                        }
                        if (dpad_right) {
                            new_controller->stick_avg_x = 1.0;
                            new_controller->is_analog   = false;
                        }
                        float32_t threshold = 0.5f;
                        Win32ProcessXInputDigitalButton(
                            new_controller->stick_avg_x < -threshold ? 1 : 0,
                            1,
                            &old_controller->move_left,
                            &new_controller->move_left);
                        Win32ProcessXInputDigitalButton(
                            new_controller->stick_avg_x > threshold ? 1 : 0,
                            1,
                            &old_controller->move_right,
                            &new_controller->move_right);
                        Win32ProcessXInputDigitalButton(
                            new_controller->stick_avg_y < -threshold ? 1 : 0,
                            1,
                            &old_controller->move_up,
                            &new_controller->move_up);
                        Win32ProcessXInputDigitalButton(
                            new_controller->stick_avg_y > threshold ? 1 : 0,
                            1,
                            &old_controller->move_down,
                            &new_controller->move_down);

                        if (new_controller->back.ended_down) {
                            g_app_running = false;
                        }

                    } else {
                        new_controller->is_connected = false;
                    }
                }

                Game_Offscreen_Buffer game_buffer = {};

                game_buffer.bytes_per_pixel = g_backbuffer.bytes_per_pixel;
                game_buffer.width           = g_backbuffer.width;
                game_buffer.height          = g_backbuffer.height;
                game_buffer.memory          = g_backbuffer.memory;

                // Test dsound output
                // both cursors are in bytes
                DWORD bytes_to_write = 0;
                DWORD byte_to_lock   = 0;

                if (is_sound_valid) {
                    // where do we want start to lock
                    byte_to_lock = (sound_output.running_sample_idx * sound_output.bytes_per_sample) %
                                   sound_output.secondary_buffer_size;
                    DWORD target_cursor =
                        (last_play_cursor + sound_output.latency_sample_count * sound_output.bytes_per_sample) %
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

                if (is_sound_valid) {
                    DWORD debug_play_cursor, debug_write_cursor;
                    g_dsound_secondary_buffer->GetCurrentPosition(&debug_play_cursor, &debug_write_cursor);

#if HANDMADE_INTERNAL
                    char text_buffer[256];
                    sprintf_s(
                        text_buffer, "play_cursor: %u, write_cursor: %u\n", debug_play_cursor, debug_write_cursor);
                    OutputDebugStringA(text_buffer);
#endif
                    Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
                }

                VirtualFree(samples, 0, MEM_RELEASE);

                // TODO: enforcing the framerate
                LARGE_INTEGER work_counter         = Win32GetWallClock();
                float32_t     ms_elapsed_for_work  = Win32GetMilliSecondsElapsed(last_counter, work_counter);
                float32_t     ms_elapsed_for_frame = ms_elapsed_for_work;
                if (ms_elapsed_for_work < target_ms_elapsed_per_frame) {
                    while (ms_elapsed_for_frame < target_ms_elapsed_per_frame) {
                        if (sleep_is_granular) {
                            DWORD sleep_ms = (DWORD)(target_ms_elapsed_per_frame - ms_elapsed_for_frame);
                            if (sleep_ms > 0) {
                                Sleep(sleep_ms);
                            }
                        }
                        ms_elapsed_for_frame = Win32GetMilliSecondsElapsed(last_counter, Win32GetWallClock());
                    }
                    // Sleep for the rest of the frame time.
                } else {
                    // TODO: logging
                }

                LARGE_INTEGER end_counter     = Win32GetWallClock();
                uint64_t      end_cycle_count = __rdtsc();

                float32_t ms_per_frame  = Win32GetMilliSecondsElapsed(last_counter, end_counter);
                uint64_t  cycle_elapsed = end_cycle_count - last_cycle_count;
                float32_t mc_per_frame  = cycle_elapsed / 1000.0f / 1000.0f;
                char      buffer[256];
                sprintf_s(buffer, "%.2f ms/f,  %.2f mc/f\n", ms_per_frame, mc_per_frame);
                OutputDebugStringA(buffer);

                // timing
                last_counter     = end_counter;
                last_cycle_count = end_cycle_count;

                Win32_Window_Dimension dimension = Win32GetWindowDimension(window_handle);
#if HANDMADE_INTERNAL
                Win32DebugSyncDisplay(
                    &g_backbuffer,
                    ArrayCount(debug_time_markers),
                    debug_time_markers,
                    &sound_output,
                    target_ms_elapsed_per_frame);
#endif
                Win32DisplayBufferInWindow(device_ctx, dimension.width, dimension.height, g_backbuffer);

                DWORD   play_cursor;
                DWORD   write_cursor;
                HRESULT hr = g_dsound_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor);
                if (SUCCEEDED(hr)) {
                    last_play_cursor = play_cursor;
                    if (!is_sound_valid) {
                        sound_output.running_sample_idx = write_cursor / sound_output.bytes_per_sample;
                        is_sound_valid                  = true;
                    }
                } else {
                    is_sound_valid = false;
                }

#if HANDMADE_INTERNAL
                {
                    Win32_Debug_Time_Marker* marker = &debug_time_markers[debug_time_marker_idx++];

                    marker->play_cursor  = play_cursor;
                    marker->write_cursor = write_cursor;
                    if (debug_time_marker_idx >= ArrayCount(debug_time_markers)) {
                        debug_time_marker_idx = 0;
                    }
                }
#endif

                // swap old and new inputs
                // TODO: make a swap macro?
                Game_Input* temp = new_input;
                new_input        = old_input;
                old_input        = temp;
            }
        }
    } else {
        // handle error
    }
    return 0;
}
