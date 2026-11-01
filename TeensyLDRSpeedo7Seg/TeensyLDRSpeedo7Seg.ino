/*
TEENSY 2.0 - LDR SCALE SPEEDOMETER

Times an object passing two light gates in either direction and displays scale
speed in mph. Either LDR can start the timer; the other stops it. An LDR is
blocked when its analog reading is below triggerVal. Both sensors must not be
blocked together to start a measurement. After a measurement, the configured
cooldown must elapse before another can start.

Parts:
- Teensy 2.0, USB cable, and a stable light source aimed at each LDR
- 2 LDRs and 2 voltage-divider resistors (10 kOhm starting value)
- 4-digit common-cathode seven-segment display with decimal points
- 8 current-limiting resistors, one per segment line including decimal point;
  choose values for the display current and Teensy I/O limits
- 3 normally-open momentary buttons: settings, down, up
- 3 external 10 kOhm pull-down resistors
- For button filtering: 3 non-polarised 1 uF capacitors (rated >= Vcc)
  and 3 series 1 kOhm resistors

Connections (Teensy/Arduino pin numbers, NOT display package-pin numbers):
  Digit common cathodes, left to right: 1, 2, 3, 4
  Segment anodes A, B, C, D, E, F, G, DP: 12, 13, 14, 15, 16, 17, 18, 19
  Put a separate current-limiting resistor in EACH segment line. Check the
  display datasheet for its package pinout; use drivers if required by current.
  A segment lights when its anode is HIGH and its selected digit cathode is LOW.

  Start LDR signal: 21; stop LDR signal: 20
  For each sensor: Vcc -> LDR -> signal node -> 10 kOhm -> GND.
  This makes the analog reading fall when light is blocked.

  Settings button: 6; down button: 7; up button: 8
  All three inputs are active HIGH and use INPUT, with external pull-downs.
  Use the board's logic supply Vcc (normally 5 V on an unmodified Teensy 2.0).
  Connect all grounds together.

Button debounce capacitor wiring (repeat separately for pins 6, 7, and 8):

  Vcc --- normally-open button --- 1 kOhm ---+--- Teensy input
                                           |
                                           +--- 10 kOhm --- GND
                                           |
                                           +--- 1 uF ------ GND

  The capacitor connects from the input node to GND, in parallel with the
  pull-down resistor, NOT across the button. Place it close to the input/GND
  wiring. Ceramic capacitors have no polarity. The series resistor limits
  capacitor charging current; the pull-down discharges it on release (about
  10 ms RC time constant with these starting values). Check for reliable presses
  with your buttons; this RC filter reduces bounce but is not a guarantee of
  one event per press. Do not add these capacitors to the LDR timing inputs:
  filtering there changes threshold-crossing times.

Settings operation:
  Tap and release settings to cycle: run -> distance -> threshold -> scale ->
  cooldown -> run. Holding settings does not intentionally repeat modes.
  Accepted rising edges must be more than 200 ms apart (software lockout).
  Each setting first displays its mode number for 0.5 s, then its value.

  Mode  Setting             Default   Down/up step       Limit
    1   LDR separation      100 mm    1 mm               minimum 1 mm
    2   Dark threshold      500       5 ADC counts       0..1023
    3   Scale multiplier    76.00     0.05               minimum 0.01
    4   Cooldown            2.0 s     0.1 s              minimum 0 s

  Hold down/up to repeat changes, with a 50 ms delay per adjustment plus loop
  overhead; pressing both cancels adjustment. Down/up have no separate software
  debounce. Settings return to run after 10 s without a mode change or value
  adjustment. A sensor trigger also returns to run: sensing remains active in
  settings. Changes apply immediately and are held only in RAM; reset/power-off
  restores all defaults.

Speed calculation:
  effective distance (cm) = LDRdistance (mm) * 0.1 * scale
  speed (mph) = effective distance / elapsed seconds * 0.0223694
  Defaults give 760 cm (7.6 m) effective distance for a physical 100 mm gap.
  In run mode the display shows elapsed seconds while timing, otherwise the
  last speed, both to two decimal places. Settings use the precision above.
  Serial diagnostics start at 9600 baud. Startup scans the display segments.

Pinout reference: https://www.pjrc.com/teensy/pinout.html
See README.md for detailed connections, calibration, and limitations.
*/

int digitPins[] = { 1, 2, 3, 4 };
int segmentPins[] = { 12, 13, 14, 15, 16, 17, 18, 19 };
int startLDRpin = 21;
int stopLDRpin = 20;

int setupSwitchPin = 6;
int buttonDownPin = 7;
int buttonUpPin = 8;
int switchPinVal = 0;
int buttonDownPinVal = 0;
int buttonUpPinVal = 0;
unsigned long buttonTime = 0;

float valStartLDR;
float valStopLDR;
int triggerVal = 500;
int timerRun = 0;  // 0 = idle, 1 = running start->stop, 2 = running stop->start
int cooldownTenths = 20;  // cooldown in 0.1 s units
unsigned long timeEnd = -2000UL;  // allows first trigger immediately

int mode = 0;  // 0 = running, 1 = LDR distance (mm), 2 = threshold, 3 = scale, 4 = cooldown
unsigned long modeTime = 0;   // when the mode last changed / was last adjusted
unsigned long pressTime = 0;  // minimum interval between accepted settings presses
bool lastPress = false;

unsigned long timeStart;
unsigned long timeNow;
unsigned long timeStop = 0.000;

float speed;
int LDRdistance = 100;      // mm
float cms2mph = 0.0223694;  // cm per sec in mph
float scale = 76.0;
float speedcmsec;

//      #  = {a,b,c,d,e,f,g};

int number[10][7] = {

  { 1, 1, 1, 1, 1, 1, 0 },  // number 0
  { 0, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 0, 1, 1, 0, 1 },
  { 1, 1, 1, 1, 0, 0, 1 },
  { 0, 1, 1, 0, 0, 1, 1 },
  { 1, 0, 1, 1, 0, 1, 1 },
  { 1, 0, 1, 1, 1, 1, 1 },
  { 1, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1 },
  { 1, 1, 1, 1, 0, 1, 1 }  // number 9

};  // end array

float timeValue = 0.000;

void setup() {
  Serial.begin(9600);

  pinMode(startLDRpin, INPUT);     // first sensor (either sensor can start timing)
  pinMode(stopLDRpin, INPUT);      // second sensor
  pinMode(setupSwitchPin, INPUT);  // active-HIGH settings button
  pinMode(buttonDownPin, INPUT);   // active-HIGH down button
  pinMode(buttonUpPin, INPUT);     // active-HIGH up button

  // Cathodes (digits) pin initialization

  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
  }

  // Anodes (Segments) pin initialization

  for (int i = 0; i < 8; i++) {  //8th pin is the decimal point
    pinMode(segmentPins[i], OUTPUT);
  }


  delay(500);

  for (int k = 0; k < 4; k++) {
    digitalWrite(digitPins[k], LOW);
  }

  for (int i = 0; i < 8; i++) {
    digitalWrite(segmentPins[i], HIGH);
    delay(50);
    digitalWrite(segmentPins[i], LOW);
    delay(50);
  }
  for (int k = 0; k < 4; k++) {
    digitalWrite(digitPins[k], HIGH);
  }
}  // end setup loop

void displayChar(int input, int position, int totalLength, int dotPosition) {
  /*
  The displayChar function takes the inputs, as integers, input (digit to be displayed), position (where it lies in the number),
  totalLength (length of the whole number to be displayed, in characters), and dotPosition (where the decimal point is) from the 
  function sendToDisplay. 

  displayChar firstly ensures that if the character position to be displayed is beyond the decimal point - which itself 
  is given its own position by the sendToDisplay function, then the position of that character is reduced by one.

  If the position is 3 or less (i.e., the 4th digit in the 7-segment display where the leftmost is the 0th digit), then
  it drops the cathode for that digit to zero (via digitPins[i]) and a loop starts, firstly a loop to run the ON code 10x for brightness,
  containing instructions to loop through each individual segment and light it according to whether the input number's array determines if it's
  meant to be on or off; then a section to turn the decimal point on or not. 

  Finally the function resets the cathodes and anodes to their default state ready to be called again.
  */

  if (dotPosition >= 0 && position > dotPosition) {
    position = position - 1;
  }

  if (position <= 3) {
    digitalWrite(digitPins[position], LOW);  // sets the digit's cathode to low to allow current to flow

    for (int j = 0; j < 10; j++) {
      for (int i = 0; i < 7; i++) {

        digitalWrite(segmentPins[i], number[input][i]);  //turns on the segmentPin[] as determined by the 'input' row of the number[] array

        if (position == dotPosition - 1) {
          digitalWrite(segmentPins[7], HIGH);
        }
      }
    }
    digitalWrite(digitPins[position], HIGH);
  }

  for (int j = 0; j < 8; j++) {
    digitalWrite(segmentPins[j], LOW);
    digitalWrite(segmentPins[7], LOW);
  }

}  //end displayChar function

void sendToDisplay(float input, int decimals) {
  /* 
Takes the raw value as a float, and converts it to a string 
so that each character can be sent across to the displayChar function.
The decimal point position is noted and sent separately. 

*/
  String strFromInput = String(input, decimals);
  int lenInputString = strFromInput.length();
  int dotPosition = strFromInput.indexOf(".");

  for (int i = 0; i < lenInputString; i++) {
    String stringDigit = strFromInput[i];
    int intStringDigit = stringDigit.toInt();
    // displayChar(int number, int position, int totalLength, int dotPosition)

    if (i != dotPosition) {
      displayChar(intStringDigit, i, lenInputString - 1, dotPosition);
    }

  }  // end length for loop


}  // end sendToDisplay function


void loop() {

  // READING VALUES FROM THE LDRs SECTION //

  valStartLDR = analogRead(startLDRpin);
  valStopLDR = analogRead(stopLDRpin);


  bool startDark = valStartLDR < triggerVal;
  bool stopDark = valStopLDR < triggerVal;
  unsigned long now = millis();

  if (timerRun == 0) {
    // trigger from either LDR, once the cooldown has elapsed
    if ((startDark != stopDark) && (now - timeEnd >= cooldownTenths * 100UL)) {
      timerRun = startDark ? 1 : 2;  // 1 = start LDR first, 2 = stop LDR first
      timeStart = now;
      mode = 0;  // a trigger event leaves settings
      Serial.println("Timer triggered");
    }
  } else if (timerRun == 1 ? stopDark : startDark) {
    timeStop = now - timeStart;
    timeEnd = now;
    timerRun = 0;
    Serial.println("Timer stopped");
    speedcmsec = (LDRdistance * 0.1 * scale) / (timeStop / 1000.000);
    Serial.print("Time taken (sec): ");
    Serial.println(timeStop / 1000.000);
    Serial.print("Speed in cm/sec: ");
    Serial.println(speedcmsec);
    speed = speedcmsec * cms2mph;
    Serial.print("Speed in mph: ");
    Serial.println(speed);
  }


  bool press = digitalRead(setupSwitchPin);
  if (press && !lastPress && now - pressTime > 200) {
    pressTime = now;
    modeTime = now;
    mode = (mode + 1) % 5;  // 1..4 then back to 0 (running)
  }
  lastPress = press;

  if (mode != 0) {
    if (now - modeTime >= 10000) {
      mode = 0;  // timeout
    } else if (now - pressTime < 500) {
      sendToDisplay(mode, 0);  // show mode number before the setting
    } else {
      int d = digitalRead(buttonUpPin) - digitalRead(buttonDownPin);
      if (d) {
        modeTime = now;
        if (mode == 1) LDRdistance = max(1, LDRdistance + d);
        else if (mode == 2) triggerVal = constrain(triggerVal + d * 5, 0, 1023);
        else if (mode == 3) scale = max(0.01, scale + d * 0.05);
        else cooldownTenths = max(0, cooldownTenths + d);
        delay(50);
      }
      if (mode == 1) sendToDisplay(LDRdistance, 0);
      else if (mode == 2) sendToDisplay(triggerVal, 0);
      else if (mode == 3) sendToDisplay(scale, 2);
      else sendToDisplay(cooldownTenths / 10.0, 1);
    }
  } else if (timerRun != 0) {
    sendToDisplay((now - timeStart) / 1000.000, 2);
  } else {
    sendToDisplay(speed, 2);
  }

}  // end main loop
