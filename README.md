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

The sketch is [`TeensyLDRSpeedo7Seg.ino`](TeensyLDRSpeedo7Seg/TeensyLDRSpeedo7Seg.ino).

1. The two LDR signals are read on pins 21 and 20. Either sensor can start
   timing when its reading is below `triggerVal` and the other is not blocked,
   once the cooldown has elapsed. Both blocked together cannot start timing.
2. The other sensor falling below `triggerVal` stops timing, supporting travel
   in either direction. The display does not show direction.
3. Speed in mph is `LDRdistance * 0.1 * scale / elapsedSeconds * 0.0223694`.
   `LDRdistance` is the physical sensor separation in **mm**.
4. In run mode the display shows elapsed seconds while timing, otherwise the
   last speed, both to two decimal places.

Defaults are 100 mm separation, threshold 500, scale 76.00 and cooldown 2.0 s.
The effective distance is initially **760 cm (7.6 m)**; the scale multiplier
is not the physical sensor separation.

### Settings controls

Tap and release the settings button to cycle run -> modes 1, 2, 3, 4 -> run.
Each setting shows its mode number for 0.5 s before its value. Down/up adjust
the value; holding a button repeats with a 50 ms delay per adjustment plus
loop overhead. Pressing both adjustment buttons together makes no change.

| Mode | Setting | Default | Down/up step | Limit / display |
| --- | --- | --- | --- | --- |
| 1 | Physical LDR separation | 100 mm | 1 mm | Minimum 1 mm; integer |
| 2 | Dark threshold | 500 | 5 ADC counts | 0-1023; integer |
| 3 | Scale multiplier | 76.00 | 0.05 | Minimum 0.01; two decimals |
| 4 | Cooldown after a measurement | 2.0 s | 0.1 s | Minimum 0 s; one decimal |

Changes apply immediately. Settings return to run after 10 s without a mode
change or value adjustment. Sensing remains active in settings; a new sensor
trigger also returns to run. All four settings are held in RAM and reset to
the defaults on reset or power cycling.

## Parts and tools

- Teensy 2.0 and a USB cable
- Four-digit, common-cathode, seven-segment display with decimal point
- Two LDRs
- Two fixed resistors for LDR voltage dividers (10 kΩ is a reasonable starting
  point; adjust for the lighting and LDRs)
- Current-limiting resistors for the seven segment lines and decimal-point line
- Three normally-open momentary buttons: settings, down and up
- Three 10 kΩ pull-down resistors
- Three non-polarised 1 µF capacitors rated for at least Vcc, and three 1 kΩ
  series resistors for button filtering
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
the sketch's `< triggerVal` condition (500 by default). The exact reading depends on the LDR,
fixed resistor, and ambient light. Check readings with the intended lighting
and adjust the divider resistor and/or the threshold in settings mode 2 if needed.
Keep both sensor signals within the Teensy's allowed input-voltage range.

Mount the sensors in a line along the object's path, with a measured separation.
Arrange a stable light source so the object interrupts each sensor in sequence.
Either sensor can be interrupted first, followed by the other sensor.
Shielding the sensors from changing room light can make triggering more
consistent.

### Settings buttons and debounce capacitors

All three inputs are active HIGH and configured as `INPUT`, requiring external
pull-downs. Use the board's logic supply Vcc (normally +5 V on an unmodified
Teensy 2.0). See the [PJRC pinout](https://www.pjrc.com/teensy/pinout.html)
and [power information](https://www.pjrc.com/teensy/external_power.html).

| Control | Teensy pin | Active behavior |
| --- | ---: | --- |
| Settings button | 6 | Each accepted press advances the mode |
| Down button | 7 | Decreases the selected setting |
| Up button | 8 | Increases the selected setting |

Build this circuit **separately for each of pins 6, 7 and 8**:

```text
Vcc --- normally-open button --- 1 kOhm ---+--- Teensy input (6, 7 or 8)
                                         |
                                         +--- 10 kOhm --- GND
                                         |
                                         +--- 1 uF ------ GND
```

Connect one capacitor lead to the input node and the other to common GND,
**in parallel with the pull-down resistor**, not across the button. Place it
close to the input and ground connection. Ceramic capacitors have no polarity.
Each button needs its own input node, resistor pair and capacitor; only Vcc
and GND are shared. Do not leave inputs floating.

The 1 kΩ series resistor limits charging current. The 10 kΩ pull-down defines
LOW on release and discharges the capacitor with an approximately 10 ms RC
time constant. These are starting values: check that presses register reliably
without extra mode changes. The RC filter reduces contact bounce but does not
guarantee one event per press.

The settings button also has a software lockout: accepted rising edges must
be more than 200 ms apart. Holding it does not intentionally repeat mode
changes; release before the next press. Down/up have no separate software
debounce and repeat while held. The 50 ms adjustment delay is not a full
debounce algorithm.

Keep these capacitors on the button inputs. Adding them to the LDR timing
signals can delay threshold crossings and change the measured speed.

## Upload and test

1. Build the circuit with power disconnected. Verify display polarity, resistor
   placement, and all common-ground connections before connecting USB.
2. Install the Arduino IDE and Teensyduino support, then open
   `TeensyLDRSpeedo7Seg/TeensyLDRSpeedo7Seg.ino`.
3. Select **Teensy 2.0** as the board, choose the appropriate USB/port options,
   and compile and upload the sketch.
4. On startup, the sketch briefly turns on the display segments one at a time.
   This is a basic wiring check.
5. Use mode 1 to set the measured sensor separation in mm, mode 2 to tune
   the threshold, mode 3 for scale and mode 4 for cooldown. Check mode numbers,
   adjustment steps, and return to run after 10 s without changes.
6. Return to run. Interrupt one sensor then the other; check elapsed seconds
   and the resulting speed. Wait for cooldown and repeat in the reverse
   direction. Verify speed against a known interval and distance.
7. Reset and verify that all four settings return to defaults.

Use a slowly moving, hand-propelled object for initial tests. Do not put hands,
loose wires, or other objects near moving machinery.

## Notes and limitations in the current sketch

- LDR dividers and room lighting change readings. Tune the threshold in
  mode 2 and test both sensors in the intended lighting.
- All four settings are held only in RAM and reset to their defaults.
- Serial diagnostics are initialised with `Serial.begin(9600)`.
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
