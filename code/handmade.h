#ifndef HANDMADE_H

#include "base.h"

struct Game_Offscreen_Buffer {
    void* memory;
    int   width;
    int   height;
    int   bytes_per_pixel;
};

internal void GameUpdateAndRender(Game_Offscreen_Buffer* buffer, int x_offset, int y_offset);

#define HANDMADE_H
#endif
