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

internal void GameUpdateAndRender(
    Game_Offscreen_Buffer* offscreen_buffer, int x_offset, int y_offset, Game_Sound_Output_Buffer* sound_buffer);

#define HANDMADE_H
#endif
