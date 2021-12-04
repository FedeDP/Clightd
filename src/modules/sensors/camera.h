#pragma once

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

#define SET_V4L2(op, val) \
do { \
    struct v4l2_control *data = set_camera_setting(priv, op, val, #op, true); \
    if (data) { \
        map_put(stored_values, #op, data); \
    } \
} while(false)

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

typedef enum { X_AXIS, Y_AXIS, MAX_AXIS } crop_axis;
typedef enum { DISABLED, CROP_API, SELECTION_API, MANUAL} crop_type_t;

typedef struct {
    bool enabled;
    double area_pct[2]; // start - end
} crop_info_t;

static struct v4l2_control *set_camera_setting(void *priv, uint32_t op, float val, const char *op_name, bool store);
static int try_set_crop(void *priv, crop_info_t *crop, crop_type_t *crop_type);

static map_t *stored_values;
static crop_info_t crop[MAX_AXIS];
static crop_type_t crop_type;
static bool camera_set;

static double get_frame_brightness(uint8_t *img_data, rect_info_t *full, bool is_yuv) {
    double brightness = 0.0;
    double min = CAMERA_ILL_MAX;
    double max = 0.0;
    
    /*
     * If greyscale (rgb is converted to grey) -> increment by 1. 
     * If YUYV -> increment by 2: we only want Y! 
     */
    const int inc = 1 + is_yuv;
    
    rect_info_t crop_rect = *full;
    switch (crop_type) {
    case MANUAL:
        if (crop[X_AXIS].enabled) {
            crop_rect.col_start = crop[X_AXIS].area_pct[0] * full->col_end;
            crop_rect.col_end = crop[X_AXIS].area_pct[1] * full->col_end;
        }
        if (crop[Y_AXIS].enabled) {
            crop_rect.row_start = crop[Y_AXIS].area_pct[0] * full->row_end;
            crop_rect.row_end = crop[Y_AXIS].area_pct[1] * full->row_end;
        }
        INFO("Manual crop: rows[%d-%d], cols[%d-%d]\n", crop_rect.row_start, crop_rect.row_end, 
                                                        crop_rect.col_start, crop_rect.col_end);
        break;
    case SELECTION_API:
    case CROP_API:
        // Update crop size
        crop_rect.col_end *= crop[X_AXIS].area_pct[1] - crop[X_AXIS].area_pct[0];
        crop_rect.row_end *= crop[Y_AXIS].area_pct[1] - crop[Y_AXIS].area_pct[0];
        break;
    default:
        break;
    }
    
    /* Find minimum and maximum brightness */
    int total = 0; // compute total used pixels
    for (int row = crop_rect.row_start; row < crop_rect.row_end; row++) {
        for (int col = crop_rect.col_start; col < crop_rect.col_end * inc; col += inc) {
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
    for (int row = crop_rect.row_start; row < crop_rect.row_end; row++) {
        for (int col = crop_rect.col_start; col < crop_rect.col_end * inc; col += inc) {
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

static void set_camera_settings_def(void *priv) {
    SET_V4L2(V4L2_CID_SCENE_MODE, -1);
    SET_V4L2(V4L2_CID_AUTO_WHITE_BALANCE, -1);
    SET_V4L2(V4L2_CID_EXPOSURE_AUTO, -1);
    SET_V4L2(V4L2_CID_AUTOGAIN, -1);
    SET_V4L2(V4L2_CID_ISO_SENSITIVITY_AUTO, -1);
    SET_V4L2(V4L2_CID_BACKLIGHT_COMPENSATION, -1);
    SET_V4L2(V4L2_CID_AUTOBRIGHTNESS, -1);
    
    SET_V4L2(V4L2_CID_WHITE_BALANCE_TEMPERATURE, -1);
    SET_V4L2(V4L2_CID_EXPOSURE_ABSOLUTE, -1);
    SET_V4L2(V4L2_CID_IRIS_ABSOLUTE, -1);
    SET_V4L2(V4L2_CID_GAIN, -1);
    SET_V4L2(V4L2_CID_ISO_SENSITIVITY, -1);
    SET_V4L2(V4L2_CID_BRIGHTNESS, -1);
}

static void set_camera_settings(void *priv, char *settings) {
    // Already called; expect a restore_camera_settings() call
    if (camera_set) {
        return;
    }
    
    /* Default values: until end of axis */
    crop[X_AXIS].area_pct[1] = 1.0;
    crop[Y_AXIS].area_pct[1] = 1.0;
    
    stored_values = map_new(true, free);
    
    /* Set default values */
    set_camera_settings_def(priv);
    if (settings && strlen(settings)) {
        char *token; 
        char *rest = settings;
        
        while ((token = strtok_r(rest, ",", &rest))) {
            uint32_t v4l2_op;
            int32_t v4l2_val;
            char axis;
            double area_pct[2];
            
            if (sscanf(token, "%u=%d", &v4l2_op, &v4l2_val) == 2) {
                SET_V4L2(v4l2_op, v4l2_val);
            } else if (sscanf(token, "%c=%lf-%lf", &axis, &area_pct[0], &area_pct[1]) == 3) {
                int8_t crop_idx = -1;
                if (area_pct[0] >= area_pct[1]) {
                    fprintf(stderr, "Start should be lesser than end: %lf-%lf\n", area_pct[0], area_pct[1]);
                } else {
                    switch (axis) {
                        case 'x':
                            crop_idx = X_AXIS;
                            break;
                        case 'y':
                            crop_idx = Y_AXIS;
                            break;
                        default:
                            fprintf(stderr, "wrong axis specified: %c; 'x' or 'y' supported.\n", axis);
                            break;
                    }
                }
                if (crop_idx != -1 && !crop[crop_idx].enabled) {
                    crop[crop_idx].enabled = true;
                    crop[crop_idx].area_pct[0] = area_pct[0];
                    crop[crop_idx].area_pct[1] = area_pct[1];
                }
            } else {
                fprintf(stderr, "Expected a=b format in '%s' token.\n", token);
            }
        }
        if (crop[X_AXIS].enabled || crop[Y_AXIS].enabled) {
            if (try_set_crop(priv, crop, &crop_type) != 0) {
                INFO("Unsupported crop/selection v4l2 API; fallback at manually skipping pixels.\n")
                crop_type = MANUAL;
            }
        } else {
            crop_type = DISABLED;
        }
    }
    camera_set = true;
}

static void restore_camera_settings(void *priv) {
    for (map_itr_t *itr = map_itr_new(stored_values); itr; itr = map_itr_next(itr)) {
        struct v4l2_control *old_ctrl = map_itr_get_data(itr);
        const char *ctrl_name = map_itr_get_key(itr); 
        INFO("Restoring setting for '%s'\n", ctrl_name)
        set_camera_setting(priv, old_ctrl->id, old_ctrl->value, ctrl_name, false);
    }
    
    // Restore crop if needed
    if (crop_type != DISABLED && crop_type != MANUAL) {
        try_set_crop(priv, NULL, &crop_type);
    }
    
    map_free(stored_values);
    stored_values = NULL;
    
    memset(crop, 0, sizeof(crop));
    crop_type = -1;
    camera_set = false;
}
