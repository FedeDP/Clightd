#include <sensor.h>
#include <linux/videodev2.h>
#include <module/map.h>
#include <udev.h>

#ifndef NDEBUG
    #define INFO(fmt, ...)          printf(fmt, ##__VA_ARGS__);
#else
    #define INFO(fmt, ...)
#endif

#define CAMERA_ILL_MAX              255
#define HISTOGRAM_STEPS             40

typedef struct {
    int row_start;
    int row_end;
    int col_start;
    int col_end;
} rect_info_t;

struct histogram {
    double count;
    double sum;
};

static double get_frame_brightness(uint8_t *img_data, rect_info_t *crop, rect_info_t *full, bool is_yuv) {
    double brightness = 0.0;
    double min = CAMERA_ILL_MAX;
    double max = 0.0;
    
    /*
     * If greyscale (rgb is converted to grey) -> increment by 1. 
     * If YUYV -> increment by 2: we only want Y! 
     */
    const int inc = 1 + is_yuv;
    
    // If no crop is requested, just go with full image
    if (crop == NULL) {
        crop = full;
    }
    
    /* Find minimum and maximum brightness */
    int total = 0; // compute total used pixels
    for (int row = crop->row_start; row < crop->row_end; row++) {
        for (int col = crop->col_start; col < crop->col_end * inc; col += inc) {
            const int idx = (row * full->col_end * inc) + col;
            if (img_data[idx] < min) {
                min = img_data[idx];
            }
            if (img_data[idx] > max) {
                max = img_data[idx];
            }
            total++;
        }
    }
    INFO("Total computed pixels: %d\n", total);
    
    /* Ok, we should never get in here */
    if (max == 0.0) {
        return brightness;
    }
    
    /* Calculate histogram */
    struct histogram hist[HISTOGRAM_STEPS] = {0};
    const double step_size = (max - min) / HISTOGRAM_STEPS;
    for (int row = crop->row_start; row < crop->row_end; row++) {
        for (int col = crop->col_start; col < crop->col_end * inc; col += inc) {
            const int idx = (row * full->col_end * inc) + col;
            int bucket = (img_data[idx] - min) / step_size;
            if (bucket >= 0 && bucket < HISTOGRAM_STEPS) {
                hist[bucket].sum += img_data[idx];
                hist[bucket].count++;
            }
        }
    }
    
    /* Find (approximate) quartiles for histogram steps */
    const double quartile_size = (double)total / 4;
    double quartiles[3] = {0};
    int j = 0;
    for (int i = 0; i < HISTOGRAM_STEPS && j < 3; i++) {
        quartiles[j] += hist[i].count;
        if (quartiles[j] >= quartile_size) {
            quartiles[j] = (quartile_size / quartiles[j]) + i;
            j++;
        }
    }
    
    /*
     * Results may be clustered in a single estimated quartile, 
     * in which case consider full range.
     */
    int min_bucket = 0;
    int max_bucket = HISTOGRAM_STEPS - 1;
    if (quartiles[2] > quartiles[0]) {
        /* Trim outlier buckets via interquartile range */
        const double iqr = (quartiles[2] - quartiles[0]) * 1.5;
        min_bucket = quartiles[0] - iqr;
        max_bucket = quartiles[2] + iqr;
        if (min_bucket < 0) {
            min_bucket = 0;
        }
        if (max_bucket > HISTOGRAM_STEPS - 1) {
            max_bucket = HISTOGRAM_STEPS - 1;
        }
    }
    
    /*
     * Find the highest brightness bucket with
     * a significant sample count
     * and return the average brightness for it.
     */
    for (int i = max_bucket; i >= min_bucket; i--) {
        if (hist[i].count > step_size) {
            brightness = hist[i].sum / hist[i].count;
            break;
        }
    }
    return (double)brightness / CAMERA_ILL_MAX;
}

