#include <stdint.h>
#include "handmade.h"

internal void
RenderBitmap(Game_Offscreen_Buffer* buffer, int x_offset, int y_offset) {
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
GameUpdateAndRender(Game_Offscreen_Buffer* buffer, int x_offset, int y_offset) {
    RenderBitmap(buffer, x_offset, y_offset);
}
