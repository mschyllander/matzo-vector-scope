# Matzo Corridor — experimental vector walkthrough

A non-playable, Wolfenstein-inspired corridor tour for the Hitachi V-212. This is an original four-room map, with no original game code, artwork or textures.

The camera walks a rectangular route through four rooms and their connecting corridors, pauses to turn at each corner, then repeats. One tour takes **38 seconds**. The renderer casts **33 rays** and draws the visible ceiling and floor boundaries plus up to three vertical corner details. All calculations use integers and a sine lookup table.

## Try it

1. Open [`MatzoCorridor.ino`](MatzoCorridor.ino) in Arduino IDE. Keep it in a folder named **MatzoCorridor**.
2. Select **NodeMCU 1.0 (ESP-12E Module)** and upload.
3. Keep the same wiring as the main show: **D1 / GPIO5 → filtered X / CH1**, **D2 / GPIO4 → filtered Y / CH2**, common GND.
4. Set the Hitachi to **X–Y**, both inputs to **DC**, and start at **0.5 V/div** on both axes.

Each output uses a **4.7 kΩ series resistor** and **100 nF capacitor to GND**. Serial Monitor uses **115200 baud**.

This example replaces the running show on the board. Upload the main [`MatzoShapes.ino`](../../MatzoShapes.ino) to restore it.

## What to expect

Walls appear as green perspective outlines, without textures, sprites or controls. The RC filters round corners. The continuous contour avoids jumping between separate objects, but retraced corner lines can look brighter and wall-depth discontinuities can produce slanted connections.

This is a hardware experiment: compilation and route checks do not establish how clean or stable the picture will look on the CRT.

## Verified before publishing

- Firmware builds successfully with **ESP8266 Arduino Core 3.1.2**, using `esp8266:esp8266:nodemcuv2`.
- Camera positions sampled every 10 ms over the entire tour remain in open map cells, without position jumps.
- All 33 rays hit walls at each sampled camera position and their projected heights stay within the screen bounds.
- The tour returns to its starting position and heading. Render checks also verified the output pins and PWM range.

If the picture flickers, try reducing `MAX_CORNER_LINES` from `3` to `1`. Keep `POINT_US` at `80` for the first test. The original shapes show is separate and unchanged.
