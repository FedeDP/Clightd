Before v1.0:

- [ ] add polkit checkAuthorization support -> this way only active session can affect screen brightness/gamma
- [x] fix "Resource temporarily unavailable" error message for getgamma/setgamma (only when ran from bus systemd service unit...) in XopenDisplay call
- [x] fix getgamma/setgamma: drop xhost call and switch to Xauthority cookie 
