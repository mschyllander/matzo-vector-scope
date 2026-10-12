# Matzo Vector Scope

[![A complete green vector triangle on the Hitachi V-212 CRT](docs/images/oscilloscope-demo.jpg)](docs/videos/matzo-vector-scope-silent.mp4)

**Turn a classic CRT oscilloscope into a vector display with a NodeMCU ESP8266.**

Two filtered PWM outputs drive the X and Y inputs of a Hitachi V-212. The sketch draws rotating solids and smooth curves using integer arithmetic and a sine lookup table.

[Watch the silent demo](docs/videos/matzo-vector-scope-silent.mp4) · [Open the Arduino sketch](MatzoShapes.ino)

## The show

One continuous shape is drawn at a time:

- Rotating cube and octahedron.
- Breathing circle.
- Tetrahedron and square pyramid.
- Rotating flower curve.
- A spacecraft with swept wings and a raised canopy.
- A rotating 3D knot.

Each shape runs for seven seconds before the next begins. The sequence repeats automatically.

## Hardware

- NodeMCU ESP8266 board.
- Two **4.7 kΩ** resistors.
- Two **100 nF** capacitors, commonly marked `104`.
- Breadboard, jumper wires and a USB data cable.
- Two oscilloscope probes or BNC leads.
- An oscilloscope with X–Y mode.

## Wiring

| NodeMCU pin | GPIO used in the sketch | Oscilloscope input |
| --- | --- | --- |
| **D1** | **5** | **CH1 / X** |
| **D2** | **4** | **CH2 / Y** |
| **GND** | — | Both probe grounds and both capacitor ground connections |

Build a separate RC filter for each output. Connect the probe tip **after the resistor**, at the same junction as the capacitor's signal lead:

```text
D1 ──[4.7 kΩ]──┬──── CH1 / X
               │
             [100 nF]
               │
              GND

D2 ──[4.7 kΩ]──┬──── CH2 / Y
               │
             [100 nF]
               │
              GND
```

Power the board through USB. All circuit grounds must be connected together. Attach the oscilloscope ground clips to circuit GND.

The resistors and capacitors smooth the PWM pulses into changing analog voltages. Each filter has a time constant of approximately **470 µs** and a cutoff frequency of approximately **339 Hz**.

## Upload with Arduino IDE

1. Install the ESP8266 board package using the [official installation guide](https://arduino-esp8266.readthedocs.io/en/latest/installing.html).
2. Download [`MatzoShapes.ino`](MatzoShapes.ino) and open it in Arduino IDE. Accept the IDE's offer to place it in a folder named **MatzoShapes**, or create that folder yourself.
3. Select **NodeMCU 1.0 (ESP-12E Module)** and the board's USB port.
4. Start with **Upload Speed: 115200**, then upload.
5. Open Serial Monitor at **115200 baud** to see the scene numbers.

If uploading stalls at `Connecting...`, disconnect external wiring first. Hold **FLASH**, briefly press **RST**, and release FLASH once writing begins.

## Set up the Hitachi V-212

- Set **TIME/DIV to X–Y**.
- Set both input coupling switches to **DC**.
- Start at **0.5 V/div** on both channels.
- Set both **VAR** controls to **CAL**, with the gain controls pushed in.
- Use the same probe factor on both channels; **×1** is suitable for the first test.
- Center the image and keep the intensity moderate. Avoid leaving a bright stationary dot on the CRT.

## Adjust the animation

Edit the constants near the top of the sketch:

| Setting | Default | What it changes |
| --- | --- | --- |
| `MOTION_SPEED` | `2` | Animation speed. Try `1`, `2` or `3`. |
| `AMPLITUDE` | `100` | Image size. Reduce it for a smaller image. |
| `POINT_US` | `80` | Dwell time at each point. Reducing it can increase rounding. |
| `SCENE_MS` | `7000` | Duration of each shape, in milliseconds. |

PWM runs at **20 kHz**. ESP8266 Arduino supports settings up to **40 kHz**, but software PWM consumes more CPU time as frequency increases. Change one setting at a time. See the [PWM reference](https://arduino-esp8266.readthedocs.io/en/latest/reference.html#analog-output).

Increasing `MOTION_SPEED` makes objects move faster while preserving the time allowed for the filters to follow each traced line.

## Verification and limitations

The current sketch builds successfully for **NodeMCU 1.0 / ESP8266** using **ESP8266 Arduino Core 3.1.2** and board profile `esp8266:esp8266:nodemcuv2`.

Additional checks covered valid vertex references, closed drawing paths and output bounds at sampled rotations. A local simulation ran all eight scenes, checked PWM values and verified that the sequence returns to the cube.

The RC filters soften corners and leave some PWM ripple. No Z-axis blanking is implemented, so the show uses continuous paths rather than disconnected stars or text strokes. The final appearance of the current show still needs to be assessed on the physical oscilloscope.

## Demo and project media

The demonstration is approximately **13 seconds**, encoded as **1080p H.264 MP4** at the original frame rate. **All audio tracks have been removed.** HDR footage was converted to SDR for ordinary playback. The entire output video was decoded successfully and verified to contain no audio streams.

The video shows the hardware in action; it is not a recording of every shape in the current sketch.

- Images: [`docs/images/`](docs/images/)
- Video: [`docs/videos/`](docs/videos/)
