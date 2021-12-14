#include <sensor.h>
#include <sys/inotify.h>
#include <glob.h>
#include <limits.h>

#define CUSTOM_NAME         "Custom"
#define CUSTOM_ILL_MAX      4096
#define CUSTOM_ILL_MIN      0
#define CUSTOM_INTERVAL     20 // ms
#define CUSTOM_FLD          "/etc/clightd/sensors.d/"

#define BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

SENSOR(CUSTOM_NAME);

static int inot_fd, inot_wd;

static bool validate_dev(void *dev) {
    return true;
}

static void fetch_dev(const char *interface, void **dev) {
    *dev = NULL;

    if (interface && interface[0] != '\0') {
        char fullpath[PATH_MAX + 1] = {0};

        /* User gave us a relative path! prepend CUSTOM_FLD as prefix! */
        if (interface[0] != '/') {
            snprintf(fullpath, PATH_MAX, CUSTOM_FLD"%s", interface);
            interface = fullpath;
        }

        /* If requested script exists */
        if (access(interface, F_OK) == 0) {
            /* Use user provided script */
            *dev = strdup(interface);
        }
    } else {
        /* Cycle files inside sensors.d folder */
        glob_t gl = {0};
        if (glob(CUSTOM_FLD"*", GLOB_ERR, NULL, &gl) == 0) {
            for (int i = 0; i < gl.gl_pathc; i++) {
                if (access(gl.gl_pathv[i], F_OK) == 0) {
                    *dev = strdup(gl.gl_pathv[i]); // first file it finds
                    break;
                }
            }
            globfree(&gl);
        }
    }
}

static void fetch_props_dev(void *dev, const char **node, const char **action) {
    if (node) {
        *node = dev;
    }

    if (action) {
        static const char *actions[] = { UDEV_ACTION_RM, UDEV_ACTION_ADD };

        int idx = access(dev, F_OK) == 0;
        *action = actions[idx];
    }
}

static void destroy_dev(void *dev) {
    free(dev);
}

static int init_monitor(void) {
    inot_fd = inotify_init();
    inot_wd = inotify_add_watch(inot_fd, CUSTOM_FLD, IN_CREATE | IN_DELETE | IN_MOVE);
    return inot_fd;
}

static void recv_monitor(void **dev) {
    *dev = NULL;
    char buffer[BUF_LEN];

    size_t len = read(inot_fd, buffer, BUF_LEN);    
    if (len > 0) {
        struct inotify_event *event = (struct inotify_event *)buffer;
        if (event->len) {
            /* prepend /etc/clightd/sensors.d/ */
            char fullpath[PATH_MAX + 1] = {0};
            snprintf(fullpath, PATH_MAX, CUSTOM_FLD"%s", event->name);
            *dev = strdup(fullpath);
        }
    }
}

static void destroy_monitor(void) {
    inotify_rm_watch(inot_fd, inot_wd);
    close(inot_fd);
}

static void parse_settings(char *settings, int *min, int *max, int *interval) {
    const char opts[] = { 'i', 'm', 'M' };
    int *vals[] = { interval, min, max };

    /* Default values */
    *min = CUSTOM_ILL_MIN;
    *max = CUSTOM_ILL_MAX;
    *interval = CUSTOM_INTERVAL;

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
        *interval = CUSTOM_INTERVAL;
    }
    if (*min < 0) {
        fprintf(stderr, "Wrong min value. Resetting default.\n");
        *min = CUSTOM_ILL_MAX;
    }
    if (*max < 0) {
        fprintf(stderr, "Wrong max value. Resetting default.\n");
        *max = CUSTOM_ILL_MAX;
    }
    if (*min > *max) {
        fprintf(stderr, "Wrong min/max values. Resetting defaults.\n");
        *min = CUSTOM_ILL_MAX;
        *max = CUSTOM_ILL_MAX;
    }
}

static int capture(void *dev, double *pct, const int num_captures, char *settings) {
    int min, max, interval;
    parse_settings(settings, &min, &max, &interval);

    int ctr = 0;
    FILE *fdev = fopen(dev, "r");
    if (fdev) {
        for (int i = 0; i < num_captures; i++) {
            int ill = -1;
            if (fscanf(fdev, "%d", &ill) == 1) {
                if (ill > max) {
                    ill = max;
                } else if (ill < min) {
                    ill = min;
                }
                pct[ctr++] = (double)ill / max;
                rewind(fdev);
            }
            usleep(interval * 1000);
        }
    }
    fclose(fdev);
    return ctr;
}
