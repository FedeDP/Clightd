#ifdef DPMS_PRESENT

int tty_get_dpms_state(void);
int tty_set_dpms_state(int dpms_level);
void tty_close(void);

#endif
