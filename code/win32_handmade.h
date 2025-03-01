#ifndef WIN32_HANDMADE_H
#define WIN32_HANDMADE_H

#include <stdint.h>
#include <Windows.h>
#include "handmade.h"

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

// TODO: can add a bytes per second to make the math simpler
struct Win32_Sound_Output {
    int samples_per_second;
    // TODO: should running sample index be in bytes?
    uint32_t running_sample_idx;
    int      bytes_per_sample;
    uint32_t secondary_buffer_size;

    int latency_sample_count;
    int safety_bytes;
};

struct Win32_Debug_Time_Marker {
    // when outputing sound
    DWORD output_play_cursor;
    DWORD output_write_cursor;
    DWORD output_location;
    DWORD output_bytes;

    // when doing frame flip, at the end of each frame
    DWORD expected_flip_play_cursor;
    DWORD flip_play_cursor;
    DWORD flip_write_cursor;
};

struct Win32_Game_Code {
    HMODULE  game_code_dll;
    FILETIME dll_last_write_time;

    game_get_sound_samples* GameGetSoundSamples;
    game_update_and_render* GameUpdateAndRender;

    bool is_valid;
};

#endif
