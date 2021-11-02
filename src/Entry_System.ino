/* started on 11DEC2017 - uploaded on 06.01.2018 by Dieter

Commands from Raspi
'>A<'   - UNLOCK DOOR
'>B<'   - 1 BEEP
'>C<'   - 2 BEEP
'>D<'   - 3 BEEP
'>P<'   - UNLOCK PERMANENT
'>L<'   - BACK TO NORMAL MODE
'>F<'   - CLEAR DISPLAY
'>G<'   - BACKLIGHT ON
'>H<'   - BACKLIGHT OFF
'R1C'   - print to LCD row 1 continous
'R2C'   - print to LCD row 2 continous
'R1T'   - print to LCD row 1 timed
'R2T'   - print to LCD row 2 timed
'WDTL'  - watch dog timer low pulse
'WDTH'  - watch dog timer high pulse

last changes on 28.01.2020 by Michael Muehl
changed: add externel watch dog with ATTiny, change Version length.
 */
#define Version "3.5.5"// (Test =3.5.5 ==> 3.5.6)

#include <Arduino.h>
#include <Wiegand.h>
#include <TaskScheduler.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// #include <CRC32.h>

// Pin Assignments
#define DATA0 2         // input  2PIN 5 - Wiegand Data 0
#define DATA1 3         // input  2PIN 6 - Wiegand Data 1
#define SIEDLE 4        // input  KPIN 1,2 [opto] - open signal SIEDLE
#define RING 5          // input  KPIN 3,4 [opto] - SIEDLE ring signal - added by Dieter Haude on 24.05.2018
#define LOCK_SENSE 6    // input  1PIN 2 [opto] - sense lock contact
#define FREE_2 7        // input  1PIN 3 -
#define OPEN_PULSE 8    // output 1PIN 4 - open pulse long as HIGH
#define OPEN_PERM 9     // output 1PIN 5 - open door permanantely as long as high
#define LED 10          // output 2PIN 7 - LED changes to green if LOW
#define BEEP 11         // output 2PIN 8 - peep sound on if LOW

#define LED_1 A3        // output - LED 1 red
#define LED_2 A2        // output - LED 2 yellow
#define LED_3 A1        // output - LED 3 green
#define PULS_WDT 13     // output - LED blue, watch dog timer pulse

#define SEC_LIGHT 5     // number of seconds set backlight on after writing value to display

//LED_BUILTIN           // LED_BUILTIN blue
/*
 * 1. open Pulse or permanent open - gelb
 * 2. Door not locked - grün
 * 3. SIEDLE Signal - rot
 * 4. TBD - blau?
 */

// Callback methods prototypes
void tRFRCallback();
void tDStCallback();
void LOCK_DOOR();
void LED_BEEP();
void DispBlCl();

//Tasks
Task tRFR(TASK_SECOND / 2, TASK_FOREVER, &tRFRCallback);  // task checking the RFID-Reader
Task tDLo(TASK_SECOND / 10, TASK_ONCE, &LOCK_DOOR);       // task to lock the door again
Task tDSt(TASK_SECOND / 5, TASK_FOREVER, &tDStCallback);  // check door contacts status
Task tRBe(1,TASK_ONCE, &LED_BEEP);                        // task for Reader beeper
Task tBCl(TASK_SECOND / 10, TASK_ONCE, &DispBlCl);        // task to clear display and Backlight off


WIEGAND wg;
Scheduler runner;
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// VARIABLES
byte ahis = 0;       // door status history value ****
byte bhis = 1;       // manuell door open signal von SIEDLE
byte rhis = 1;       // ring signal von SIEDLE - added by Dieter Haude on 24.05.2018
byte SEC_OPEN = 1;   // number of seconds to hold door open after valid UID
// external watch dog
byte wdTimeL = 3; // value * (Task tRFR(TASK_SECOND / 2)
byte wdTimeH = 1; // value * (Task tRFR(TASK_SECOND / 2)
byte wdCount = 0; // counter for watch dog

String inStr = "";   // a string to hold incoming data

int bufferCount;     // Anzahl der eingelesenen Zeichen

void setup() {
  Serial.begin(115200);

  wg.begin();        // start Wiegand Bus Control
  inStr.reserve(17); // reserve for instr serial input

  Wire.begin();      // I2C
  lcd.init();        // initialize the LCD

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BEEP, OUTPUT);
  pinMode(OPEN_PULSE, OUTPUT);
  pinMode(OPEN_PERM, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(PULS_WDT, OUTPUT);

  pinMode(LOCK_SENSE, INPUT_PULLUP);
  pinMode(SIEDLE, INPUT_PULLUP);
  pinMode(RING, INPUT_PULLUP);  // added by Dieter Haude on 24.05.2018

  digitalWrite(OPEN_PULSE, HIGH);
  digitalWrite(OPEN_PERM, HIGH);
  digitalWrite(BEEP, HIGH);
  digitalWrite(LED, HIGH);
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, LOW);
  digitalWrite(LED_3, LOW);
  digitalWrite(PULS_WDT, LOW);

  runner.init();
  runner.addTask(tRFR);
  runner.addTask(tDLo);
  runner.addTask(tDSt);
  runner.addTask(tRBe);
  runner.addTask(tBCl);

// Turn on the blacklight and print a message.
// Grundstellung nach Start
  lcd.clear();
  lcd.backlight();
  lcd.print("Hello");
  delay(500);
  tRFR.enable();        // start cyclic readout of reader
  tDSt.enable();        // start cyclic readout of door status
  lcd.clear();
  lcd.noBacklight();
  Serial.println("entry;POR;V" + String(Version));  // Entry System restart MM 15.07.19
}

// FUNCTIONS (Tasks) ----------------------------
void tRFRCallback() {
  // signal for watch dog timer (software)
  if (wdCount == wdTimeL) digitalWrite(PULS_WDT, HIGH);
  if (wdCount == (wdTimeL + wdTimeH) && digitalRead(PULS_WDT)) {
    digitalWrite(PULS_WDT, LOW);
    wdCount = 0;
  }
  if (wg.available())  {                // check for data on Wiegand Bus
    Serial.println((String)"card;" + wg.getCode() + "; W" + wg.getWiegandType());
    wg.delCode();
  }
  ++wdCount;
 }

void tDStCallback() {
  byte a = digitalRead(LOCK_SENSE); // true --> door open, false --> door closed
  byte b = digitalRead(SIEDLE);     // false --> open signal from SIEDLE
  byte r = digitalRead(RING);       // false --> ring signal
  if(a != ahis) {
    ahis = a;
    Serial.println((String)"lock;" + a);
    a ? digitalWrite(LED_2, true) : digitalWrite(LED_2, false);
    }

  if(b != bhis) {
    bhis = b;
    Serial.println((String)"siedle;" + b);
    b ? digitalWrite(LED_1, false) : digitalWrite(LED_1, true);
    }

  // added by Dieter Haude on 24.05.2018
  if(r != rhis) {
    rhis = r;
    Serial.println((String)"ring;" + r); // corrected by Dieter on 28.06.2018
    }
}
// END OF TASKS ---------------------------------

// FUNCTIONS ------------------------------------
void LOCK_DOOR(void) {              // BACK TO NORMAL LOCKING
  digitalWrite(OPEN_PULSE, true);
  digitalWrite(OPEN_PERM, true);
  digitalWrite(LED, true);          // LED RFID-Reader
  digitalWrite(LED_3, false);
}

void UNLOCK_DOOR(void) {            // send Unlock Door pulse for 'SEC_OPEN' seconds
  digitalWrite(OPEN_PULSE, false);
  digitalWrite(LED, false);         // LED RFID-Reader
  digitalWrite(LED_3, true);
  tDLo.restartDelayed(TASK_SECOND * SEC_OPEN);  // start task in 'SEC_OPEN' sec to close door
  }

void UNLOCK_PERM(void) {            // Unlock Door permanantely
  digitalWrite(OPEN_PERM, false);
  digitalWrite(LED, false);         // LED RFID-Reader
  digitalWrite(LED_3, true);
  }

void LED_BEEP() {
  digitalWrite(BEEP, false);
  tRBe.setCallback(&LED_BEEP2);
  tRBe.restartDelayed(200);
}

void LED_BEEP2() {
  digitalWrite(BEEP, true);
  tRBe.setCallback(&LED_BEEP3);
  tRBe.restartDelayed(100);
}

void LED_BEEP3() {
  tRBe.setCallback(&LED_BEEP);
}

void LED_2TBEEP() {
  digitalWrite(BEEP, false);
  tRBe.setCallback(&LED_2TBEEP2);
  tRBe.restartDelayed(200);
}

void LED_2TBEEP2() {
  digitalWrite(BEEP, true);
  tRBe.setCallback(&LED_BEEP);
  tRBe.restartDelayed(100);
}

void LED_3TBEEP() {
  digitalWrite(BEEP, false);
  tRBe.setCallback(&LED_3TBEEP2);
  tRBe.restartDelayed(200);
}

void LED_3TBEEP2() {
  digitalWrite(BEEP, true);
  tRBe.setCallback(&LED_2TBEEP);
  tRBe.restartDelayed(100);
}

void DispBlCl() {
  lcd.noBacklight();
  lcd.clear();
}
/*
void send_crc32(String Str) {
  unsigned int numBytes = Str.substring(2).length();
  char byteBuffer[17];
  Str.substring(2).toCharArray(byteBuffer, 17);
  uint32_t checksum = CRC32::calculate(byteBuffer, numBytes);
  Serial.print("crc;"); // Wrote first text
  Serial.println(checksum,HEX); // and before line end checksum as HEX
}
*/

// End Funktions --------------------------------

// Funktions Serial Input (Event) ---------------
void evalSerialData() {
  // send_crc32(inStr.substring(0,len));
  if (inStr.substring(0, 3) == ">A<") {   // UNLOCK DOOR
    UNLOCK_DOOR();
    return;
  }
  if (inStr.substring(0, 3) == ">B<") {   // 1 BEEP
    tRBe.setCallback(&LED_BEEP);
    tRBe.restart();
    return;
  }
  if (inStr.substring(0, 3) == ">C<") {   // 2 Beep's
    tRBe.setCallback(&LED_2TBEEP);
    tRBe.restart();
    return;
  }
  if (inStr.substring(0, 3) == ">D<") {   // 3 Beep's
    tRBe.setCallback(&LED_3TBEEP);
    tRBe.restart();
    return;
  }
  if (inStr.substring(0, 3) == ">P<") {   // UNLOCK PERMANENT
    UNLOCK_PERM();
    return;
  }
  if (inStr.substring(0, 3) == ">L<") {   // BACK TO NORMAL MODE
    LOCK_DOOR();
    return;
  }
  if (inStr.substring(0, 3) == ">F<") {   // CLEAR DISPLAY
    lcd.clear();
    return;
  }
  if (inStr.substring(0, 3) == ">G<") {   // BACKLIGHT ON
    lcd.backlight();
    return;
  }
  if (inStr.substring(0, 3) == ">H<") {   // BACKLIGHT OFF
    lcd.noBacklight();
    return;
  }

  if (inStr.substring(0, 3) == "R1C") {  // print to LCD row 1 continous changed DH 24.1.
    inStr.concat("              ");     // add blanks to string  changed by MM 10.01.2018
    lcd.backlight(); lcd.setCursor(0,0);
    lcd.print(inStr.substring(3,19)); // cut string lenght to 16 char  changed by MM 10.01.2018
    return;
  }

  if (inStr.substring(0, 3) == "R2C") {  // print to LCD row 2 continous changed DH 24.1.
    inStr.concat("              ");     // add blanks to string  changed by MM 10.01.2018
    lcd.backlight(); lcd.setCursor(0,1);
    lcd.print(inStr.substring(3,19));   // cut string lenght to 16 char  changed by MM 10.01.2018
    return;
  }

  if (inStr.substring(0, 3) == "R1T") {  // print to LCD row 1 timed changed DH 24.1.
    inStr.concat("              ");     // add blanks to string  changed by MM 10.01.2018
    lcd.backlight(); lcd.setCursor(0,0);
    lcd.print(inStr.substring(3,19)); // cut string lenght to 16 char  changed by MM 10.01.2018
    tBCl.restartDelayed(TASK_SECOND * SEC_LIGHT);      // changed by DieterH on 18.10.2017
    return;
  }

  if (inStr.substring(0, 3) == "R2T") {  // print to LCD row 2 changed DH 24.1.
    inStr.concat("              ");     // add blanks to string  changed by MM 10.01.2018
    lcd.backlight(); lcd.setCursor(0,1);
    lcd.print(inStr.substring(3,19));   // cut string lenght to 16 char  changed by MM 10.01.2018
    tBCl.restartDelayed(TASK_SECOND * SEC_LIGHT);      // changed by DieterH on 18.10.2017
  }

  if (inStr.substring(0, 4) == "WDTL" && inStr.length() > 4 && inStr.length() < 7) {  // change watch dog low pulse.  && inStr.length() <= 7
    if (inStr.substring(4).toInt() > 0) {
      wdTimeL = inStr.substring(4).toInt();
      digitalWrite(PULS_WDT, LOW);
    }
  }

  if (inStr.substring(0, 4) == "WDTH" && inStr.length() > 4  && inStr.length() < 7) {  // change watch dog high pulse
    if (inStr.substring(4).toInt() > 0) {
      wdTimeH = inStr.substring(4).toInt();
      digitalWrite(PULS_WDT, HIGH);
    }
  }
}

/*
 SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent() {
  char inChar = (char)Serial.read();
  if (inChar == '\x0d') {
    evalSerialData();
    inStr ="";
  } else {
    inStr += inChar;
  }
}
// End Funktions Serial Input -------------------

// PROGRAM LOOP AREA ----------------------------
void loop() {
  runner.execute();
}
