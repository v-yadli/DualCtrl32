# DualCtrl32
Turns LCONTROL into dual-role key LCONTROL/ESC. Compatible with AltDrag.

## Usage
After starting the program, LCONTROL becomes a dual-role key. 
- Press it down, and the LCTRL-DOWN event will be cached rather than sent directly.
- Press it down, and then release it without doing anything else on the keyboard/with the mouse, will translate LCOLTROL into ESC.
- Press it down, and perform other keyboard/mouse actions (like move mouse, press other keys), and the cached LCTRL-DOWN event will be flushed.
  - This is different from most other dual-role key programs in that DualCtrl32 is aware of mouse events (and also alt-drag events)

## Configuration
Place DualCtrl32.ini beside the executable.
- `KeyTrigger=FALSE` disables keyboard event flush trigger.
- `MouseTrigger=FALSE` disables mouse event flush trigger.
- `TimerTrigger=100` sets a timeout flush trigger after LCTRL-DOWN, recommend use with disabled key/mouse trigger.
- `AltDragFix=FALSE` disables AltDrag fix.
- `Blacklist=WNDCLASS_1,WNDCLASS_2,...` specifies a blacklist of window class names. If the foreground window class name is a substring of a blacklist entry, DualCtrl32 will be bypassed.
  - Useful to bypass windows that register low level keyboard/mouse hooks and thus would interfere with DC32. For example:
  - `Blacklist=LyncConversationWindowClass,CommunicatorMainWindowClass,TscShellContainerClass` would bypass Skype for Business and mstsc remote desktop.
