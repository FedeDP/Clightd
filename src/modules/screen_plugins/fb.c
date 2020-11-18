#include "screen.h"
#include <linux/fb.h> /* to handle framebuffer ioctls */
#include <sys/ioctl.h>

#define DEFAULT_FB "/dev/fb0"

static int get_framebufferdata(int fd, struct fb_var_screeninfo *fb_varinfo_p, struct fb_fix_screeninfo *fb_fixedinfo);
static int read_framebuffer(int fd, size_t bytes, unsigned char *buf_p, int skip_bytes);

SCREEN("Fb")

/* Many thanks to fbgrab utility: https://github.com/GunnarMonell/fbgrab/blob/master/fbgrab.c */
static int get_frame_brightness(const char *id, const char *env) {
     if (!id || !strlen(id)) {
         id = DEFAULT_FB;
    }
    
    int ret = WRONG_PLUGIN;
    unsigned char *buf_p = NULL;
    int fd = open(id, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: Couldn't open %s.\n", id);
        return ret;
    }
    
    ret = UNSUPPORTED;
    struct fb_var_screeninfo fb_varinfo = {0};
    struct fb_fix_screeninfo fb_fixedinfo = {0};
    if (get_framebufferdata(fd, &fb_varinfo, &fb_fixedinfo) != 0) {
        goto err;
    }
    
    const int bitdepth = fb_varinfo.bits_per_pixel;
    const int width = fb_varinfo.xres;
    const int height = fb_varinfo.yres;
    const int line_length = fb_fixedinfo.line_length / (bitdepth >> 3);
    const int skip_bytes =  (fb_varinfo.yoffset * fb_varinfo.xres) * (bitdepth >> 3);
    const int stride = fb_fixedinfo.line_length;
    
    fprintf(stderr, "Fb resolution: %ix%i depth %i.\n", width, height, bitdepth);
    
    const size_t buf_size = height * stride;

    if (line_length < width) {
        fprintf(stderr, "Line length cannot be smaller than width");
        ret = -EINVAL;
    } else {
        buf_p = calloc(buf_size, sizeof(unsigned char));
        if (buf_p == NULL) {
            ret = -ENOMEM;
            goto err;
        }
        if (read_framebuffer(fd, buf_size, buf_p, skip_bytes) != 0) {
            ret = -EAGAIN;
        } else {
            ret = rgb_frame_brightness(buf_p, line_length, height, stride);
        }
    }

err:
    free(buf_p);
    if (fd != -1) {
        close(fd);
    }
    return ret;
}

static int get_framebufferdata(int fd, struct fb_var_screeninfo *fb_varinfo_p, struct fb_fix_screeninfo *fb_fixedinfo) {
    if (ioctl(fd, FBIOGET_VSCREENINFO, fb_varinfo_p) != 0) {
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, fb_fixedinfo) != 0) {
        return -1;
    }
    return 0;
}

static int read_framebuffer(int fd, size_t bytes, unsigned char *buf_p, int skip_bytes) {
    if (skip_bytes) {
        lseek(fd, skip_bytes, SEEK_SET);
    }

    if (read(fd, buf_p, bytes) != (ssize_t) bytes) {
        fprintf(stderr, "Error: Not enough memory or data\n");
        return -1;
    }
	return 0;
}
