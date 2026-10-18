# Arduino Parkour

A dodge-the-walls game for an Arduino with a 16×2 character LCD and an analog joystick.

Walls scroll toward you one line at a time, each with a gap; steer through the gap with the joystick before it hits you. The game ramps up as your score climbs: walls speed up (1.6 s → 0.5 s per step), gaps get narrower (5 columns → 1), and the gap can jump farther between lines. The joystick calibrates its center on boot, so it works with any stick.

## Hardware

- Arduino Uno (or compatible)
- 16×2 HD44780 LCD wired in 4-bit mode: RS→12, E→11, D4–D7→5, 4, 3, 2
- Analog joystick: X→A1, Y→A2, button→D7

## Files

- `parkour_game/parkour_game.ino`: the game
- `sketch_jul10a/sketch_jul10a.ino`: a joystick + LCD test sketch

Open the `.ino` in the Arduino IDE (needs the built-in `LiquidCrystal` library) and upload.
