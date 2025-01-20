#include <stdint.h>
// #include <math.h>

#include "base.h"
#include "handmade.h"

internal void
GameOutputSound(Game_Sound_Output_Buffer* buffer, int tone_hz) {
    local_persist float32_t t_sine;

    int16_t tone_volume = 3000;
    int     wave_period = buffer->samples_per_second / tone_hz;

    int16_t* sample_out = buffer->samples;
    for (int sample_idx = 0; sample_idx < buffer->sample_count; ++sample_idx) {
        float32_t sine_val   = (float32_t)sin(t_sine); // use the intrinsic version of sin, not the stdlib
        int16_t   sample_val = (int16_t)(sine_val * tone_volume);

        // left
        *sample_out = sample_val;
        ++sample_out;
        // right
        *sample_out = sample_val;
        ++sample_out;

        t_sine += 2.0f * PI * 1.0f / (float32_t)wave_period;
    }
}

internal void
RenderBitmap(Game_Offscreen_Buffer* buffer, int x_offset, int y_offset) {
    uint8_t* row = (uint8_t*)buffer->memory;
    for (int y = 0; y < buffer->height; ++y) {
        uint32_t* pixel = (uint32_t*)row;

        for (int x = 0; x < buffer->width; ++x) {
            // (windows bitmap) byte order: BB GG RR 00

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
    Game_Memory*              memory,
    Game_Input*               input,
    Game_Offscreen_Buffer*    offscreen_buffer,
    Game_Sound_Output_Buffer* sound_buffer) {

    Assert(sizeof(Game_State) <= memory->permanent_storage_size);

    Game_State* state = (Game_State*)memory->permanent_storage;
    if (!memory->is_initialized) {
        const char*            file_name = __FILE__;
        Debug_Read_File_Result file      = DEBUGPlatformReadEntireFile(file_name);
        if (file.content) {
            DEBUGPlatformWriteEntireFile("test.out", file);
            DEBUGPlatformFreeFileMemory(file.content);
        }

        state->x_offset        = 0;
        state->y_offset        = 0;
        state->tone_hz         = 256;
        memory->is_initialized = true;
    }

    for (int controller_idx = 0; controller_idx < ArrayCount(input->controllers); ++controller_idx) {
        Game_Controller_Input* controller_input = &input->controllers[controller_idx];
        if (controller_input->is_analog) {
            state->tone_hz = 256 + (int)(128.0f * controller_input->stick_avg_y);
            state->x_offset += (int)(4.0f * controller_input->stick_avg_x);
        } else {
            if (controller_input->move_up.ended_down) {
                state->y_offset -= 1;
            }
            if (controller_input->move_down.ended_down) {
                state->y_offset += 1;
            }
            if (controller_input->move_left.ended_down) {
                state->x_offset -= 1;
            }
            if (controller_input->move_right.ended_down) {
                state->x_offset += 1;
            }
        }

        if (controller_input->action_down.ended_down) {
            state->y_offset += 1;
        }
    }

    GameOutputSound(sound_buffer, state->tone_hz);
    RenderBitmap(offscreen_buffer, state->x_offset, state->y_offset);
}
