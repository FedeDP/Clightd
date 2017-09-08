## Later
It seems my driver does not send proper udev events for this to be implemented soon...  
Code is already in place but commented out.  

- [ ] Add an udev monitor on each backlight interface "enabled" value and send a signal as soon as it changes (this way clight can do a capture as soon as an interface became enabled by hooking on this signal)
