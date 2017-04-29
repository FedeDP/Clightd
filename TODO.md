Before v1.0:

- [x] add polkit checkAuthorization support -> this way only active session can affect screen brightness/gamma
- [ ] drop xlib and use libxcb (see redshift) for gamma
- [ ] once moved gamma to xcb, move dpms support in clightd (as xcb will already be a dep)
