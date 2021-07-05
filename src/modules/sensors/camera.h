#include <sensor.h>
#include <linux/videodev2.h>
#include <module/map.h>
#include <udev.h>

#ifndef NDEBUG
    #define INFO(fmt, ...)          printf(fmt, ##__VA_ARGS__);
#else
    #define INFO(fmt, ...)
#endif

double get_frame_brightness(uint8_t *img_data, size_t size, bool is_yuv);
