## 5.5 (6.0)

### Camera
- [x] Add support for a cropping setting parameter: "x=0.4-0.6,y=0.2-0.8"
- [x] Support crop through crop and selection v4l2 api if available
- [x] fallback at manually skipping pixels
 
### Backlight
- [x] Create new object paths for each detected display
- [x] Drop "Set/Get" methods, and add a Set method on each object path
- [x] SetAll will call Set on each object path (and will be renamed to Set); same for GetAll
- [x] Follow Keyboard API
- [x] Create this new interface under org.clightd.clightd.Backlight2 (to avoid api break)?
- [x] Actually implement logic
- [ ] use more meaningful return types for Set,Raise,Lower? (boolean does not make much sense...)... TODO? For now, no return type
- [ ] Better error handling/code
- [ ] Drop old BACKLIGHT module? -> in case, drop {Lower,Raise,Set}All from clightd polkit policy
- [x] Drop bl_store_vpcode() and only load vpcode from CLIGHTD_BL_VCP env?
- [x] Add CLIGHTD_BL_VCP Environment variable to systemd script with a comment thus it is simple to update it if needed
- [x] Expose Max and Internal properties

## 5.x
- [ ] Keep it up to date with possible ddcutil api changes

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
