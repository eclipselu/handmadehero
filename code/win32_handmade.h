#ifndef WIN32_HANDMADE_H
#define WIN32_HANDMADE_H

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

struct Win32_Sound_Output {
    int      samples_per_second;
    uint32_t running_sample_idx;
    int      bytes_per_sample;
    int      secondary_buffer_size;

    int latency_sample_count;
};

#endif
