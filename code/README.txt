
Day 21 - Loading Game Code Dynamically
======================================

- Split the game code into a DLL
- Invoke game code in handmade.dll from win32_handmade.exe
  - remove internal linkage modifiers - internal(static)
  - use the same trick to load XINPUT dll to load the dll
  - have stub functions, game won't crash?
  - keep track of the loaded HMODULE (dll)
  - IMPORTANT: need to fix the name mangling in C++ compiler, we only need old style C linkage
    - use extern "C"
  - export the function, 2 ways: https://learn.microsoft.com/en-us/cpp/build/exporting-from-a-dll?view=msvc-170
    1. compiler flag
    2. put __declspec(dllexport)
  - use dumpbin tool to view the DLL export table, to see the difference between C++ linkage and C linkage (with extern "C")
- Invoke platform code in win32_handmade.cpp in handmade.dll?
  - can we load the exe? we can, but we don't have to do that
  - Use game memory to pass the function pointer to the platform specific functions - like DEBUGWriteEntireFile

- TODOs:
  - Change function pointer type definition naming to be consistent with other defintions
  - Unload the game code
  - Get rid of local_persist variables, put them in Game_State

Day 20 - Debugging Audio sync
=============================

- Breaking down the time spent for each frame
  - gather input -> update, render prep -> rendering -> wait -> flip
  - impossible to output sound even before the gather input stage
  - overlapping the gather input of the current frame and the rendering of the last frame?
    - introduced input lag, probably not a good idea, as low input lag is more important

- Use the play to write cursor delta to adjust target audio latency
- Keeping code fluid and a bit messy at early stage, get it working and stable, before considering pull it out.
- audio clock might not be in sync with wall clock (cpu clock)

Achieving audio sync is complicated TODO: link the noteability diagram here.

We want our projected frame boundary byte to be close to the actual play_cursor when the frame flips.

Also the play/write cursor update granularity is about 480 samples:
- can be done by doing a while-true loop and printing out the cursor locations, see how much it changes.
- update granularity is 1920 bytes 
This needs also be taken into consideration in doing audio sync.

Day 19 - Improving Audio Sync
=============================

- Create a Debug Diagram for audio sync debug code
  - Draw both play and write cursor
  - Tuning audio sample latency
- Getting the same audio latency for play cursor and write cursor:

play_cursor: 86400, write_cursor: 92160
play_cursor: 92160, write_cursor: 97920
play_cursor: 99840, write_cursor: 105600
play_cursor: 105600, write_cursor: 111360

gap = 111360 - 105600 = 5760 bytes = 5760 / 4 (bytes per sample) = 1440 samples
1440 samples is roughly 30ms of audio data

but here: https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708925(v=vs.85), it says:

> The write cursor indicates the position at which it is safe to write new data to the buffer.
> The write cursor always leads the play cursor, typically by about **15 milliseconds' worth of audio data**.
> It is always safe to change data that is behind the position indicated by the lpdwCurrentPlayCursor parameter.

Not sure why MSDN says differently, right now we increase the audio latency when writing to 3 frames.

Day 18 - Enforcing a Video Frame Rate
=====================================

- Trying to run a fixed framerate, never want to do variable framerate, as the physics might be wrong. 
- Target milliseconds/frame.
- Audio: if we miss the next frame, we might have an audio lag. 
  - [preferred] always hit the fps
  - overwrite the next frame's audio
  - frame of lag
  - guard thread


Day 17 - Unified Keyboard and Gamepad input
===========================================

NOTE: try to increase the functionalness of the program more.

- Deadzone processing on controller
  - Deadzone: a region around zero that is considered 0
  - See: https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput
- [BUGFIX] Fixed a bug that incorrectly swap old_input and new_input
- How to unify the keyboard and gamepad
  - Making analogue stick act like dpad buttons
  - Use average x/y offset
- The button naming was a bit confusing, changed to 2 sets of buttons
  - move up/down/left/right: character movement
  - action up/down/left/right: the actual ABXY buttons, according to their location on the gamepad.
- Dpad can be unified to stick movement


Day 16 - Some MSVC compiler options
===================================

- /nologo: do not display the msvc and linker version information during compilation
- /MT:     link the c runtime library in the binary, instead of using the dll
- /GR-:    disable RTTI
- /EHa-:   disable C++ exception handling
- /Oi:     Generate intrinsic functions
- /Od:     Disables optimization, good for debugging
- /WX:     treat warnings as errors
- /W4:     set warning level, W4 is the highest
- /Z7:     Generates C 7.0-compatible debugging information.
- /FC:     Displays the full path of source code files passed to cl.exe in diagnostic text.
- /wd2101: disables C4201 warning
- /D:      macro definitions


We're not targeting Windows XP for now, so ignored the XP related flags to compile a 32-bit binary.


Day 15 - Platform-independent Debug file I/O
============================================

- We want to read the entire file, instead of using the streaming io.
- get file size -> reserve memory -> read file into reserved memory



Day 14 - Platform-independent Game memory
==========================================

Nothing special



Day 13 - Platform-independent Input
===================================

- As a game what do we save for user input? fighting game (sequences) or just a snapshot of a certain time?
- We don't want to miss a button click
- Joystick polling frequency?


Day 12 - Platform-independent Sound
===================================

- Write the usage code first, then design the APIs to do exactly/closely to that
- Don't want to leak the Win32 DirectSound ring buffer to game layer.
- Where do we want to store the sound samples? 
  - probably not on the stack, VirtualAlloc to alloc on the heap

Day 11 - Platform API design
============================

2 Styles to write platform code
- Style 1: vitualizing the OS to the game.
- Style 2: We can keep the platform layer as is, and invoke game specific code.
  - e.g. call renderer in win32/macos/linux platform code, but renderer is a service that the game provides, and it's platform indenpendent.


References:
- Semantic compression: https://caseymuratori.com/blog_0015


Day 10 - Timing information of running game
===========================================

- RDTSC instruction gives access to the real-time stamp counter, which increment by 1 when a CPU cycle ends.
- Depending on the OS, you might get everything that the CPU runs, not just your code.
- A Couple of ideas of time:
  - Wall clock time - time as it passes in the real world. Measured in seconds.
  - Processor time - how many cycles? this is related to wall clock time by processor frequency, but for a long time now frequency varies a lot and quickly.
- QueryPerformanceCounter, measures wall time

What is an intrinsic?
- it looks like a function call, it's a hint to the compiler that it's a specific assembly instruction. 
- like: __rdtsc()

Day 9 - Sine Wave sound
=======================

- [Homework] Read about Floating point numbers
  - CSAPP
  - https://float.exposed/0x44bf9400
- Sine wave, pretty straight forward, the sample value is a function of running index, using sine.
- Currently it's not smooth, sounds like there's a blunt transition at period end.
- Fixed in a followup commit, the bug is that byte to write should be 0 when it's catching the play cursor.
  - initialize variable to 0
  - turn the warning level on in complier options.
- Use controller to control the Hz
  - what caused the glitch sound? it's not continous mathematically to make a Hz change and directly calculate the sine value from sample index
  - how to avoid the glitch? track the sine value incrementally, so the jump is not that dramatic.
- There's a delay in controller input and the actual Hz change, you can here it, how to resolve this?
  - write a head the play cursor for some samples, so that the play cursor can receive the latest writes, rather than loop around the buffer to get the latest freq change.
  - introduced in latency_sample_count


Day 8 - Writing a Square Wave to DirectSound
============================================

- Lock the secondary buffer
  - Needed for writing to the buffer
  - depending on where we want to lock and the size, we may get 1 or 2 regions


NOTE:
┌─┆─┐  ┌─┆─┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┆┌──┐  ┌──┐  ┌──┐
┘ ┆ └──┘ ┆ └──┘  └──┘  └──┘  └──┘  └──┆┘  └──┘  └──┘  └──
  <-    ->
   pattern

we flip every half pattern



Day 7 - DirectSound
=================================

- Create dsound, create primary/secondary buffers, set wave format.
- DSound API is terrible!!!
- Why do we need primary and secondary buffers?
    - secondary buffer is what we need to play sound
    - primary buffer seems a legacy thing, to set sound card in certain modes, to avoid resampling secondary buffer, 
    - the only thing we need for primary boffer is to set format on the sound card.
- Curious of how Core Audio API in macOS works


Day 6 - Keyboard/Controller Input
=================================

- Controller input - XInput API
- What happens if the xinput dll is not found?
  - do we crash?
  - we can use stub functions to not crash the program, try to load the function in the dll.

Day 5
============

- C++, constructor/destructor, called when something is created and out of scope.
- global variable clean up, clean up duplicated code.
- Reason to have drawing in WM_PAINT:
    https://hero.handmade.network/forums/game-discussion/t/8571-day4__why_is_stretchdibits_done_twice_and_what_triggers_wm_paint_#26968

    > The main reason to have drawing in WM_PAINT is because that is only code executed when you are moving window or when you are resizing it. During these operations your main loop does not run as message processing loop enters separate modal loop.
    > 
    > If you don't care about redrawing in such case, then there's no need to use WM_PAINT and just having drawing in main loop is enough.
    > 
    > But if you want to handle such drawing, then having draw code in WM_PAINT and main loop is fine to have. It does not mean it will be always called twice. Simply in normal game run the main loop will do the drawing. And only when window is moved/resized/obscured then WM_PAINT draw kicks-in. You can issue InvalidateRect to force WM_PAINT from main code. But meh.. it does not really matter.

- On StretchDIBits
  - Fix one bug that destination and source are in the wrong order
  - Making the back buffer fixed size, meaning the pixel count will be the same, but when we resize the window, StretchDIBits will possibly change the aspect ratio.

- Stack grows downwards in memory, high address -> low address
- Stack overflow when too much memory is used in stack space, e.g. 2MB local variable.



Day 4
============

- Penalty on un-aligned access, if a value is 4 byte long, it should fall on the address of 4 byte boundaries like 0, 4, 8...
- VirtualAlloc to alloc memory, 
- Forgot to set bmiHeader.biPlanes to 1, the window did not display anything.
- how to control the animation speed (fps)?
- VirtualAlloc, VirtualFree and HeapAlloc are windows system calls, different from the C runtime library malloc, malloc will end up calling these functions under the hood.



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
