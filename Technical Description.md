# Multiple Bonus Vehicles modder's notes

## Putting cars on a map

Place a Car Start locator for each bonus vehicle. Give each locator a unique
name, set its heading and bonus vehicle model name, and leave the ordinary
parked-traffic flag clear. Leave room between cars and keep the spawn points
clear of mission triggers and entrances.

Pure3D stores these as Locator chunks (0x03000005), type 3. The first data
word is the Y rotation. The second is the parked-traffic flag. The remaining
words contain the padded, null-terminated vehicle name. The game resolves
that name to art/cars/<name>.p3d. Provide a working vehicle and its
associated configuration.

A launcher map mod can declare RequiredHack=MultipleBonusVehicles under
[Miscellaneous] to load the hack as a dependency.

## How many cars fit?

The hack supports 20 bonus cars per running level across all streamed chunks.
Pending loads count toward that limit. Removing a car frees its slot once
cleanup completes. Traffic, mission cars, and the player's car also count
toward the engine's shared 30-vehicle list.

Version 1.1 targets Lucas Mod Launcher 1.27.1 and the English retail PC game
with executable SHA-256
2928118887B910AE03CE714E287D9BC67FB95F29E1B4F1271B8AF389990C0795.
The hack checks the executable and expected machine instructions before
applying its in-memory hooks. A conflict at any of the checked sites leaves
the stock instructions in place.

## What was crashing?

The stock game has one place to store a pending bonus car. If two cars start
loading at once, the second can clobber the first car's locator and inventory
section. The loader then calls back with the wrong state.

Each car now has its own locator, callback and inventory section. The game
still does the actual work of spawning, removing and repairing cars. If a
zone unloads while a car is loading, its callback just cleans up.

The LMLH is a 32-bit DLL with the launcher metadata inside. Lucas calls
HackEntryPoint to install the hooks. The examples in
[WadSupport](https://github.com/EnAppelsin/WadSupport) and
[FPCamInCar](https://github.com/Krolik9933/FPCamInCar) helped with that part.

The demo supplies two locator records and a Custom Files handler that
appends them in memory to the expected stock Level 1 chunk. The records
use markers that activate when the hack has installed its hooks.
The demo package contains only the two locators and the handler.

## Getting a log

To record hook activation and each queued, spawned or removed car, set
SHAR_BONUS_LOG to a writable absolute file path before starting Lucas
from the same shell. If neither demo car appears, check the launcher
entries and test with the stock Level 1 map.
