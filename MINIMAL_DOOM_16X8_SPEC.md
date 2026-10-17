# Minimal 16x8 Doom-Style Game Specification

## Purpose

Build a new, self-contained RP2040 game inspired by the movement and first-person view of Doom, but implemented from scratch for a 16x8 WS2812B-2020 LED matrix.

This is not a port of the Doom engine. Do not copy Doom source code, WAD files, maps, sprites, textures, music, or other game assets. Use an original small level and simple original geometric art. The project must not depend on external game data stored in flash.

The first version must prove the core loop:

- First-person movement in one small level.
- Turn left and right.
- Move forward and backward.
- Fire with `SW5`.
- Draw directly at 16x8; do not render at 320x200 and downscale.
- No menu, sound, save/load, networking, demos, inventory, settings UI, or multiple levels.

## Verified Hardware

| Item | Value |
| --- | --- |
| MCU | RP2040, generic Raspberry Pi Pico-compatible board |
| Flash | 2 MB W25Q080 |
| LED matrix | 16x8 WS2812B-2020, 128 LEDs |
| Matrix data input | GPIO13 |
| LED byte order | RGB, packed as `0xRRGGBB00` |
| LED wire layout | Straight rows; D17 is directly below D1 |
| Orientation | Both logical axes must be flipped to match the physical board |
| PIO peripheral | PIO0, state machine 0 |
| WS2812 bit rate | 800 kHz |

The panel is not a HUB75 display and is not a Cosmic Unicorn. It is a WS2812 serial chain.

### Buttons

All buttons are active-low and must use the RP2040 internal pull-ups.

| Physical control | GPIO | First-version action |
| --- | ---: | --- |
| Left | GPIO2 | Turn left |
| Right | GPIO1 | Turn right |
| Up | GPIO3 | Move forward |
| Down | GPIO0 | Move backward |
| SW5 / action | GPIO4 | Fire |

Do not enable UART stdio on GPIO0 or GPIO1; those pins are buttons. For the Pico SDK target, call:

```cmake
pico_enable_stdio_uart(tiny_doom_16x8 0)
```

## Matrix Output

Maintain a `uint32_t pixels[16 * 8]` framebuffer and transmit all 128 values over PIO0/SM0 after each game update.

Logical coordinate `(x, y)` maps to the physical LED index as follows:

```c
int physical_y = 7 - y;
int physical_x = 15 - x;
int led_index = physical_y * 16 + physical_x;
```

The PIO program must use the standard four-instruction WS2812 serializer, 24-bit left-shift output, joined TX FIFO, and a 300 microsecond reset/latch interval after a frame.

Do not calculate the PIO clock divider with `float`: the target should remain compatible with a float-free Pico build. Use a divider in 1/256 increments:

```c
uint32_t bit_clock_hz = 800000 * 10;
uint64_t divider_256 = ((uint64_t)clock_get_hz(clk_sys) * 256
    + bit_clock_hz / 2) / bit_clock_hz;
sm_config_set_clkdiv_int_frac(&config, divider_256 >> 8, divider_256 & 0xff);
```

`10` is the number of PIO cycles per WS2812 bit for timing values `T1=2`, `T2=5`, and `T3=3`.

## Minimal Game Design

### Current Game Design (supersedes the World section below)

- One hand-embedded `64x64` map in `src/world_map_data.h` (maze corridors, loops and rooms; every floor tile is reachable from the hub). The hub room is `7x7` tiles (`5x5` interior) in the top-left corner with one doorway at `x=6, y=3`.
- Monsters never enter the hub, and the player's health refills while inside it. Death respawns the player in the hub.
- Hub pillars (one tile each, north row): white = rifle, yellow = shotgun, blue = SMG. Shooting a pillar selects that weapon. There are no menus or levels.
- 25 monsters spawn at random open tiles outside the hub; killed ones respawn after about 20 s. Each spawn rolls a type: 50% red (5 HP), 35% orange (10 HP), 15% purple (10 HP, 50% chance to block each hit).
- Goal: shoot all 5 colored wall buttons (red, green, magenta, cyan, orange) to win; a pressed button turns dark gray. A dot on the bottom row, in the target button's color, shows its bearing (clamped to the edge when off-screen or behind). The target is the nearest unpressed button, re-picked after each press. After the last one the screen flashes green and the game restarts.

### World
Use one compile-time `20x20` tile map. `#` is a solid wall and `.` is empty space. The outer map border must always be walls.

```text
####################
#..................#
#.######.####.####.#
#.#....#....#....#.#
#.#.##.####.#.##.#.#
#...##...#....#....#
###.######.####.##.#
#...#....#.#....##.#
#.###.##.#.#.##....#
#.....##...#..####.#
#.##########.##....#
#.#........#....##.#
#.#.######.######..#
#...#....#......#..#
#####.##.######.#..#
#.....##........#..#
#.##############.#.#
#..................#
#..................#
####################
```

Start the player at `(1.5, 18.5)` in map coordinates (`Q16.16`: `1.5 * FIXED_ONE`, `18.5 * FIXED_ONE`), facing into the maze. Spawn one enemy at `(1.5, 15.5)` in the same starting corridor.

### Renderer

Implement a fixed-point raycaster directly for 16 screen columns:

1. Cast one ray per horizontal LED column across a field of view near 60 degrees.
2. Use DDA grid stepping to find the first wall tile.
3. Convert wall distance into a centered vertical wall slice. A distant wall must occupy fewer than all 8 rows; clamp the slice to 1-7 pixels while its ray distance is at least one full map tile. As the player approaches, increase the slice height monotonically, allowing it to reach all 8 rows only at close range.
4. Paint the ceiling dark blue at minimum visible intensity and the floor black. Render the first and third wall directions (north and south) in muted gray; render the second and fourth directions (east and west) in muted green. Opposite faces must therefore use the same base color. Scale RGB intensity with an inverse-square distance falloff, clamped to a minimum of `2/255`: at 5, 4, 3, 2, and 1 tile, target intensities are approximately `2`, `4`, `7`, `16`, and `64` out of `255`; cap wall intensity at `160/255`.
5. Render the enemy as a small muted red-brown sprite only when it is in front of the player and closer than the wall in that screen column. Make it brighter briefly while attacking.

Use integer or fixed-point arithmetic. A `Q16.16` position and direction representation is sufficient. Trigonometry may use a 256-entry sine lookup table stored in flash.

### Input and Simulation

Run a fixed update at 30 Hz or 60 Hz.

- Holding Up/Down continuously moves the player with collision against solid map tiles.
- Holding Left/Right continuously changes the view angle.
- Pressing SW5 starts a short bright yellow/orange firing state. When the living enemy is inside the center aim cone and has a clear line of sight, the shot removes it.
- The enemy moves directly toward the player while a clear corridor exists. At close range it periodically attacks, using a brief brighter enemy sprite and red edge pixels as hit feedback.

Poll GPIOs every update. Store previous button states so a press edge can trigger firing once, while movement and turning use the held state.

## Required Project Layout

```text
tiny-doom-16x8/
  CMakeLists.txt
  pico_sdk_import.cmake
  src/
    main.c
    ws2812.pio
    display_ws2812.c
    display_ws2812.h
    input.c
    input.h
    world.c
    world.h
    raycaster.c
    raycaster.h
```

Keep hardware output, buttons, world state, and renderer in separate modules. `main.c` should only initialize modules and run the update-render-present loop.

## Build Requirements

- CMake 3.13 or newer
- Ninja
- Arm GNU Toolchain (`arm-none-eabi-gcc`)
- Raspberry Pi Pico SDK 1.5.1 or newer
- `picotool`

Known local SDK paths on the development machine:

```text
C:/Program Files/Raspberry Pi/Pico SDK v1.5.1/pico-sdk
C:/Users/Ondra/Documents/Pico-v1.5.1/pico-extras
C:/Program Files/Raspberry Pi/Pico SDK v1.5.1/picotool/picotool.exe
```

Configure and build from the new project's root directory:

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=MinSizeRel `
  -DPICO_BOARD=pico `
  -DPICO_SDK_PATH="C:/Program Files/Raspberry Pi/Pico SDK v1.5.1/pico-sdk"

cmake --build build --target tiny_doom_16x8 -j 4
```

`PICO_EXTRAS_PATH` is not needed for this minimal project. Add it only if the new project later uses Pico Extras.

The build must produce:

```text
build/tiny_doom_16x8.uf2
```

## Flashing

1. Hold BOOTSEL while connecting or resetting the board so it enters USB bootloader mode.
2. Run:

```powershell
& "C:\Program Files\Raspberry Pi\Pico SDK v1.5.1\picotool\picotool.exe" load -u -v ".\build\tiny_doom_16x8.uf2"
& "C:\Program Files\Raspberry Pi\Pico SDK v1.5.1\picotool\picotool.exe" reboot
```

3. If the firmware does not begin immediately after `picotool reboot`, press the board's physical `RESET` button once.

Successful flashing ends with `Verifying Flash: 100%` and `OK`.

## Acceptance Tests

1. On boot, show a brief static test frame proving all 128 LEDs can be addressed, then begin the game.
2. A stable 16x8 first-person maze view is visible; it must not use a 320x200 framebuffer or a WAD.
3. Holding Up and Down moves the view forward and backward without crossing walls.
4. Holding Left and Right rotates the view.
5. Pressing SW5 visibly fires.
6. The `20x20` map starts in the lower-left corridor and prevents crossing any `#` wall.
7. Opposite wall faces have the same muted color, while adjacent perpendicular faces differ only by shade. Moving closer makes a wall slice taller and brighter; moving away makes it shorter and dimmer, without showing a distant object across all 8 rows.
8. The visible enemy approaches through the starting corridor, flashes while attacking, and disappears after a centered SW5 shot with clear line of sight.
9. The firmware builds for `PICO_BOARD=pico`, flashes by `picotool`, and runs after reset.

## Explicitly Deferred

- Multiple levels, level loading, WAD support, save/load, passwords.
- Audio and music.
- Menus, HUD, inventory, keys, doors, lifts, secrets, and intermissions.
- Textures, animated sprites, additional enemies, path finding, projectiles, and persistent player damage.
- USB keyboard, UART controls, networking, and overclocking.
- Any direct reuse of Doom source code or assets.
