Day 4
============

- Penalty on un-aligned access, if a value is 4 byte long, it should fall on the address of 4 byte boundaries like 0, 4, 8...
- VirtualAlloc to alloc memory, 
- Forgot to set bmiHeader.biPlanes to 1, the window did not display anything.


Day 3
============

- Things are better acquired and released in aggregate, instead of doing individually. Thinking of resource aquisition and release in waves.
- `static` keyword in C
    - global variable
    - local persisting variable
    - "private" functions in a translation unit (.c files)
    - use macros to define better names for these 3 purposes
- Handle `WM_RESIZE`
    - `WM_RESIZE`: update back buffer
    - `WM_PAINT`: use back buffer to draw the window.

Some style preferences to be consistent onwards
- variable names: `some_variable` instead of `someVariable`
- global variables: use `g_` to indicate it's global, e.g. `g_app_running`
- method: UpdateEntry instead of `update_entry`
- function: return type in its own line


Day 2
============

- Win32 programming
    references: https://learn.microsoft.com/en-us/windows/win32/learnwin32/learn-to-program-for-windows 
