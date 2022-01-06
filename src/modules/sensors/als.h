#pragma once

#include <math.h>

/* For more on interpreting lux values: https://docs.microsoft.com/en-us/windows/win32/sensorsapi/understanding-and-interpreting-lux-values */
#ifndef ALS_INTERVAL
    #define ALS_INTERVAL        20      // ms, customizable. Yocto sensor has different default interval
#endif
#define ALS_ILL_MAX         100000  // Direct sunlight
#define ALS_ILL_MIN         1       // Pitch black

static inline double compute_value(double illuminance) {
    if (illuminance > ALS_ILL_MAX) {
        illuminance = ALS_ILL_MAX;
    } else if (illuminance < ALS_ILL_MIN) {
        illuminance = ALS_ILL_MIN;
    }
    return log10(illuminance) / log10(ALS_ILL_MAX);
}

static inline void parse_settings(char *settings, int *interval) {
    const char opts[] = { 'i' };
    int *vals[] = { interval };

    /* Default value */
    *interval = ALS_INTERVAL;

    if (settings && settings[0] != '\0') {
        char *token; 
        char *rest = settings; 

        while ((token = strtok_r(rest, ",", &rest))) {
            char opt;
            int val;

            if (sscanf(token, "%c=%d", &opt, &val) == 2) {
                bool found = false;
                for (int i = 0; i < SIZE(opts) && !found; i++) {
                    if (opts[i] == opt) {
                        *(vals[i]) = val;
                        found = true;
                    }
                }

                if (!found) {
                    fprintf(stderr, "Option %c not found.\n", opt);
                }
            } else {
                fprintf(stderr, "Expected a=b format.\n");
            }
        }
    }
    
    /* Sanity checks */
    if (*interval < 0 || *interval > 1000) {
        fprintf(stderr, "Wrong interval value. Resetting default.\n");
        *interval = ALS_INTERVAL;
    }
}
