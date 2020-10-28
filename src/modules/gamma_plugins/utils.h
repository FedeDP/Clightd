#pragma once

#include <stdint.h>

struct _gamma_cl;

typedef struct {
    int (*set)(struct _gamma_cl *cl, const int temp);
    int (*get)(struct _gamma_cl *cl);
    int (*dtor)(struct _gamma_cl *cl);
    void *priv;
} gamma_handler;

typedef struct _gamma_cl {
    unsigned int target_temp;
    unsigned int is_smooth;
    unsigned int smooth_step;
    unsigned int smooth_wait;
    unsigned int current_temp;
    char *display;
    int fd;
    gamma_handler handler;
} gamma_client;

double clamp(double x, double min, double max);
int get_temp(const unsigned short R, const unsigned short B);
void fill_gamma_table(uint16_t *r, uint16_t *g, uint16_t *b, uint32_t ramp_size, int temp);
