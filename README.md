# Multiple Bonus Vehicles

Park more than one bonus car in a level, even in the same map chunk. This
is for the English retail PC game and Lucas Mod Launcher 1.27.1.

## Install

Grab the hack and the optional demo from [Releases](https://github.com/YoriRitsuko/MultipleBonusVehicles/releases/latest).

1. Drop the `.lmlh` into `Hacks` beside the Lucas launcher.
   Make the folder if it's missing.
2. Reload the mod list and tick **Multiple Bonus Vehicles** in **Settings**.

Upgrading from 1.0? Close the game and move the old bonus car LMLM and its
`d3d8.dll` out of the launcher and game folders first.

## Try the Level 1 demo

Drop the demo `.lmlm` into `Mods` and tick it in
**General**. Lucas loads the hack with it. Start Level 1 and look for the red
convertible and Bart's Honor Roller outside the Simpsons house. Untick the
demo when you're testing your own Level 1 map.

## Build from source

Run `./build.ps1` with x86 [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw)
on PATH, or pass `-Compiler` with the path to its `i686-w64-mingw32-clang++.exe`.
The script finds windres beside the compiler. Use `-ResourceCompiler` if yours
is somewhere else. The LMLH lands in `Release`.

To rebuild the demo, put the LMLH in `Hacks` and the demo folder from `package`
in `Mods`. Reload the list, right-click the demo and hit **Compile**.

The [modder's notes](Technical%20Description.md) cover locators, limits and logging.
