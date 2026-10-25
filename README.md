# Teensy LDR Speedometer

This project uses a Teensy 2.0, two light-dependent resistors (LDRs), and a
multiplexed four-digit, seven-segment display to time an object passing two
light sensors. It calculates speed from the elapsed time and a configurable
distance, then displays the result in miles per hour (mph).

> **Important:** Treat this as a classroom experiment, not a safety-rated
> speedometer or a device for measuring the speed of road vehicles. The
> distance, sensor thresholds, display code, and timing behavior need testing
> and calibration before measurements can be trusted.

## How it works

The sketch is [`TeensyLDRSpeedo7Seg.ino`](TeensyLDRSpeedo7Seg.ino).

1. The two LDR voltage-divider signals are read on Teensy pins 21 (start) and
   20 (stop).
2. When the start reading is below 500 while the stop reading is at least 500,
   the sketch records `millis()` and starts timing.
3. When the stop reading falls below 500, it records the elapsed time.
4. It calculates speed using
   `speed (mph) = distance (cm) / elapsed time (seconds) × 0.0223694`.
   In this sketch, the distance used is `LDRdistance × scale`.
5. While timing, the display shows elapsed seconds. Otherwise it shows the
   most recently calculated speed. In setup mode, it shows the scale value.

The setup switch selects setup mode. The down and up buttons change `scale` by
0.5 per loop iteration while held. The sketch starts with `LDRdistance = 10 cm`
and `scale = 76`, so its initial effective distance is **760 cm (7.6 m)**.
That product is what the speed calculation uses; the displayed scale is not
itself the distance between the sensors.

## Parts and tools

- Teensy 2.0 and a USB cable
- Four-digit, common-cathode, seven-segment display with decimal point
- Two LDRs
- Two fixed resistors for LDR voltage dividers (10 kΩ is a reasonable starting
  point; adjust for the lighting and LDRs)
- Current-limiting resistors for the seven segment lines and decimal-point line
- One setup switch and two momentary push buttons
- Breadboard or other suitable prototyping board, jumper wires, and a common
  ground connection
- Arduino IDE with Teensy support (Teensyduino)

Check your display's datasheet before wiring it. Pin numbering and whether the
display is common-cathode or common-anode vary by part. This sketch drives
segment lines HIGH and digit cathodes LOW, so it expects a **common-cathode**
display.

## Connections

Pin numbers below are the Teensy/Arduino digital pin numbers used in the
sketch, not physical package-pin numbers. Connect all grounds together.

### Seven-segment display

Each digit has a shared cathode controlled independently; corresponding segment
anodes are shared across the four digits. Connect:

| Display connection | Teensy pin |
| --- | ---: |
| Digit 1 common cathode (leftmost) | 1 |
| Digit 2 common cathode | 2 |
| Digit 3 common cathode | 3 |
| Digit 4 common cathode (rightmost) | 4 |
| Segment A anodes | 12 |
| Segment B anodes | 13 |
| Segment C anodes | 14 |
| Segment D anodes | 15 |
| Segment E anodes | 16 |
| Segment F anodes | 17 |
| Segment G anodes | 18 |
| Decimal-point anodes | 19 |

Put a **separate current-limiting resistor in series with each segment line**,
including the decimal point. Do not rely on one resistor in a digit common
connection: segment current and brightness can vary as the displayed pattern
changes. Select resistor values from the display's forward-voltage/current
specifications and the Teensy's I/O limits. If the display needs more current
than the microcontroller pins can safely supply, use suitable driver
transistors or a display-driver circuit instead of connecting it directly.

The digit common cathodes connect to pins 1–4. When a digit is selected, its
pin is driven LOW; an illuminated segment is driven HIGH on pins 12–19. The
sketch briefly scans the digits to create the appearance of a steady display.

### LDR sensors

Build one voltage divider per LDR. A starting arrangement that makes the
reading go low in darkness is:

```text
Teensy +5 V ── LDR ──┬── analog input
                     |
                   10 kΩ
                     |
Teensy GND ──────────┘
```

Use the divider midpoint as the signal connection:

| Sensor | Divider midpoint to |
| --- | ---: |
| Start LDR (first sensor encountered) | 21 |
| Stop LDR (second sensor encountered) | 20 |

With this arrangement, blocking the light lowers the analog reading, matching
the sketch's `< 500` trigger condition. The exact reading depends on the LDR,
fixed resistor, and ambient light. Check readings with the intended lighting
and adjust the divider resistor and/or the threshold in the sketch if needed.
Keep both sensor signals within the Teensy's allowed input-voltage range.

Mount the sensors in a line along the object's path, with a measured separation.
Arrange a stable light source so the object interrupts each sensor in sequence.
The start sensor must be interrupted first, followed by the stop sensor.
Shielding the sensors from changing room light can make triggering more
consistent.

### Setup switch and buttons

These inputs are read as HIGH when active. Since the sketch configures them as
`INPUT` (not with internal pull-ups), each needs a defined LOW state when
inactive. Use an external pull-down resistor, for example 10 kΩ from the input
to ground, and wire the switch/button between the input and +5 V:

| Control | Teensy pin | Active behavior |
| --- | ---: | --- |
| Setup/run switch | 6 | HIGH selects setup mode |
| Scale down button | 7 | HIGH decreases scale by 0.5 |
| Scale up button | 8 | HIGH increases scale by 0.5 |

Connect the pull-down resistor and controls to the Teensy ground. Do not leave
these inputs floating. The buttons are not debounced; holding one changes the
scale repeatedly.

## Upload and test

1. Build the circuit with power disconnected. Verify display polarity, resistor
   placement, and all common-ground connections before connecting USB.
2. Install the Arduino IDE and Teensyduino support, then open
   `TeensyLDRSpeedo7Seg.ino`.
3. Select **Teensy 2.0** as the board, choose the appropriate USB/port options,
   and compile and upload the sketch.
4. On startup, the sketch briefly turns on the display segments one at a time.
   This is a basic wiring check.
5. Select setup mode and use the buttons to change the scale. Measure the
   sensor separation and determine what scale gives the desired effective
   distance (`LDRdistance × scale`); test the result against a known interval.
6. Return to run mode. Interrupt the start sensor and then the stop sensor.
   Confirm that elapsed seconds appear while timing and that a speed is shown
   after the stop sensor is triggered.

Use a slowly moving, hand-propelled object for initial tests. Do not put hands,
loose wires, or other objects near moving machinery.

## Notes and limitations in the current sketch

- The comments at the top of the `.ino` file describe older switch pin numbers
  and ambiguous display wiring. The pin table above follows the declarations
  and logic in the sketch itself: controls are on 6–8, and pins 12–19 are
  segment outputs.
- `Serial.print()` diagnostics are present, but `setup()` does not call
  `Serial.begin()`. USB serial output may therefore not be available as written.
- The sensor threshold is fixed at 500. LDR dividers and room lighting can
  change the readings, so test the actual sensor values and tune the threshold.
- The scale is held only in RAM. It resets to 76 after power cycling or reset.
- Timing is based on the interval between two threshold crossings. Sensor
  response, object width, alignment, and changing illumination all affect the
  measurement.
- The four-digit display has limited range. Very small/large values, negative
  values, and values with more characters than fit have not been robustly
  handled by the sketch.
- The source does not include a project-specific build or test configuration.

## License

The repository includes a `LICENSE` file identifying the work as CC0 1.0
Universal.
