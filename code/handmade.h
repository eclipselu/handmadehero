#ifndef HANDMADE_H
#define HANDMADE_H

#include <stdint.h>
#include "base.h"

struct Game_Offscreen_Buffer {
    void* memory;
    int   width;
    int   height;
    int   bytes_per_pixel;
};

struct Game_Sound_Output_Buffer {
    int      samples_per_second;
    int      sample_count;
    int16_t* samples;
};

// Input
struct Game_Button_State {
    int  half_transition_count;
    bool ended_down;
};

struct Game_Controller_Input {
    bool is_connected;
    bool is_analog;

    // sticks, use average offset
    float32_t stick_avg_x;
    float32_t stick_avg_y;

    union {
        Game_Button_State buttons[12];
        struct {
            // e.g. Dpad up/down/left/right
            Game_Button_State move_up;
            Game_Button_State move_down;
            Game_Button_State move_left;
            Game_Button_State move_right;

            // e.g. Xbox buttons ABXY
            Game_Button_State action_up;
            Game_Button_State action_down;
            Game_Button_State action_left;
            Game_Button_State action_right;

            Game_Button_State left_shoulder;
            Game_Button_State right_shoulder;
            Game_Button_State start;
            Game_Button_State back;
        };
    };
};

struct Game_Input {
    union {
        Game_Controller_Input controllers[5];
        struct {
            Game_Controller_Input keyboard_controller;
            Game_Controller_Input gamepad_controllers[4];
        };
    };
};

struct Game_State {
    int x_offset;
    int y_offset;
    int tone_hz;
};

struct Game_Memory {
    bool is_initialized;

    uint64_t permanent_storage_size;
    void*    permanent_storage;
    uint64_t transient_storage_size;
    void*    transient_storage;
};

internal void GameUpdateAndRender(Game_Memory* memory, Game_Input* input, Game_Offscreen_Buffer* offscreen_buffer);

// NOTE: this has to be a very fast function, it cannot be more than 1ms or so
// TODO: Reduce the pressure on this function's performance by measuring it.
internal void GameGetSoundSamples(Game_Memory* memory, Game_Sound_Output_Buffer* sound_buffer);

#if BUILD_DEBUG
struct Debug_Read_File_Result {
    uint32_t content_size;
    void*    content;
};

internal Debug_Read_File_Result DEBUGPlatformReadEntireFile(const char* file_name);
internal bool                   DEBUGPlatformWriteEntireFile(const char* file_name, Debug_Read_File_Result result);
internal void                   DEBUGPlatformFreeFileMemory(void* memory);
#endif

inline uint32_t
SafeTruncateUint64(uint64_t val) {
    Assert(val <= 0xFFFFFFFF);
    uint32_t result = (uint32_t)val;
    return result;
}

#endif
