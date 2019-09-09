# Dromaius, a C++ gameboy emulator / debugger
This is an emulator but mostly a debugger for the original Nintendo GameBoy (aka DMG).

What started as an attempt at emulating the most basic Z80/GB code is now a graphical GameBoy debugger that also kind of plays games :)
It's still far from complete but it kind of plays Pokemon Red. Current functionality includes:

- memory viewer
- disassembler
- live callstack view with support for imported symbols
- stepping by cycle / frame
- VRAM viewer: BG tileset, sprite data, PPU registers
- audio buffer viewer, with pretty waveform plots
- savestate support in the most hacky way possible

Usage:

    $ make
    $ ./dromaius tests/tetris.gb

(requires SDL2)

![Screenshot](/screenshots/gui.png?raw=true)
