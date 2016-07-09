# Dromaius, a C++ gameboy emulator
This is an emulator for the original Nintendo GameBoy (aka DMG).

Currently it is able to start most games (with compatible MBC, not all are implemented yet), and some of those games are actually playable. Other games don't make it past the nintendo logo, and some even segfault. Not (yet) implemented: RTC, battery, proper halting and timers, windows, serial, etc. In other words: if you just want to play games, use another emulator.
 
Usage:

    $ make
    $ ./gbemu tests/tetris.gb

(requires SDL2)

![Screenshot](https://raw.githubusercontent.com/ThomasRinsma/dromaius/master/screenshots/screenshot_games.png)
