# Mini OS — Notes + Flappy Bird

## Included apps
- Calculator
- Clock
- Stopwatch
- Notes (64 characters; saved to Arduino EEPROM)
- Flappy Bird (rotary encoder button to flap)

Sensors, Password Vault, and Settings have been removed from the menu.

## Controls
- Rotate: navigate the menu.
- Short press: open an app.
- Hold the button for ~0.5 seconds: return to menu (Notes saves automatically).
- Notes: rotate to change the selected character; click to move to the next character; choose a space to erase; hold to save and exit.
- Flappy Bird: click to start/flap; click after Game Over to restart; hold to exit.

## Wokwi
Use the included `sketch.ino` and `diagram.json` in your Wokwi project. Keep the existing U8g2 library. EEPROM is built into the Arduino core and does not need to be added separately.

## Multitasking note
The Uno shows one app at a time. Calculator and notes state are retained while switching; clock and stopwatch are based on `millis()` so time keeps progressing in the background. Notes persist across power cycles using EEPROM. This is cooperative multitasking, not true multi-core parallel execution.
