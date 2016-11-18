# DualCtrl32
Turns LCONTROL into dual-role key LCONTROL/ESC. Compatible with AltDrag.

## Usage
After starting the program, LCONTROL becomes a dual-role key. 
- Press it down, and the LCTRL-DOWN event will be cached rather than sent directly.
- Press it down, and then release it without doing anything else on the keyboard/with the mouse, will translate LCOLTROL into ESC.
- Press it down, and perform other keyboard/mouse actions (like move mouse, press other keys), and the cached LCTRL-DOWN event will be flushed.
  - This is different from most other dual-role key programs in that DualCtrl32 is aware of mouse events (and also alt-drag events)
