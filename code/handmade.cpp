#include <cstdint>
#include <math.h>

#include "handmade.h"

internal void
GameOutputSound(Game_Sound_Output_Buffer* buffer, int tone_hz) {
    local_persist float32_t t_sine;

    int16_t tone_volume = 3000;
    int     wave_period = buffer->samples_per_second / tone_hz;

    int16_t* sample_out = buffer->samples;
    for (int sample_idx = 0; sample_idx < buffer->sample_count; ++sample_idx) {
        float32_t sine_val   = sin(t_sine);
        int16_t   sample_val = (int16_t)(sine_val * tone_volume);

        // left
        *sample_out = sample_val;
        ++sample_out;
        // right
        *sample_out = sample_val;
        ++sample_out;

        t_sine += 2.0 * PI * 1.0 / (float32_t)wave_period;
    }
}

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
GameUpdateAndRender(
    Game_Input* input, Game_Offscreen_Buffer* offscreen_buffer, Game_Sound_Output_Buffer* sound_buffer) {
    // TODO: allow sample offset here for more robust platform options
    local_persist int x_offset = 0;
    local_persist int y_offset = 0;
    local_persist int tone_hz  = 256;

    Game_Controller_Input* controller_input0 = &input->controllers[0];
    if (controller_input0->is_analog) {
        tone_hz = 256 + (int)(128.0f * controller_input0->end_y);
        x_offset += (int)4.0f * controller_input0->end_x;
    } else {
        // TODO: digital movement
    }

    if (controller_input0->down.ended_down) {
        y_offset += 1;
    }

    GameOutputSound(sound_buffer, tone_hz);
    RenderBitmap(offscreen_buffer, x_offset, y_offset);
}
