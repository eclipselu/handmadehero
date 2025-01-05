#ifndef HANDMADE_H

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
    bool is_analog;

    // sticks
    float32_t start_x;
    float32_t start_y;
    float32_t end_x;
    float32_t end_y;
    float32_t min_x;
    float32_t min_y;
    float32_t max_x;
    float32_t max_y;

    union {
        Game_Button_State buttons[8];
        // NOTE: not the dpad, it's the actual ABXY buttons on an Xbox controller, e.g:
        //  Y
        // X B
        //  A
        // could be different for a DualShock controller
        struct {
            Game_Button_State up;    // Y
            Game_Button_State down;  // A
            Game_Button_State left;  // X
            Game_Button_State right; // B
            Game_Button_State left_shoulder;
            Game_Button_State right_shoulder;
            Game_Button_State start;
            Game_Button_State back;
        };
    };
};

struct Game_Input {
    Game_Controller_Input controllers[4];
};

internal void
GameUpdateAndRender(Game_Input* input, Game_Offscreen_Buffer* offscreen_buffer, Game_Sound_Output_Buffer* sound_buffer);

#define HANDMADE_H
#endif
