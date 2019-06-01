#ifdef DPMS_PRESENT

int xorg_get_dpms_state(const char *display, const char *xauthority);
int xorg_set_dpms_state(const char *display, const char *xauthority, int dpms_level);
void xorg_close(void);

#endif
