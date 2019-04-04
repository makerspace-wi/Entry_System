/* started on 11DEC2017 - uploaded on 06.01.2018 by Dieter
 * last changes on 17.08.2018 by Dieter Haude
 * changed: added watchdog
 */
#define Version "3.2"

#include <Wiegand.h>
#include <TaskScheduler.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <CRC32.h>
#include <avr/wdt.h>    // added by Dieter Haude on 17.08.2018

#define SECONDS 1000    // multiplier for second

// Pin Assignments
#define DATA0 2         // input - Wiegand Data 0
#define DATA1 3         // input - Wiegand Data 1
#define SIEDLE 4        // input open signal SIEDLE
#define RING 5          // input SIEDLE ring signal - added by Dieter Haude on 24.05.2018
#define LOCK_SENSE 6    // input - sense lock contact
#define free_2 7
#define OPEN_PULSE 8    // output - open pulse long as HIGH
#define OPEN_PERM 9     // output - open door permanantely as long as high
#define LED 10          // output - LED changes to green if LOW
#define BEEP 11         // output - peep sound on if LOW

#define LED_1 A3        // LED 1 red
#define LED_2 A2        // LED 2 yellow
#define LED_3 A1        // LED 3 green
#define SEC_LIGHT 5     // number of seconds let backlight on after writing value to display

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
Task tRFR(500, TASK_FOREVER, &tRFRCallback);  // task checking the RFID-Reader
Task tDLo(100, TASK_ONCE, &LOCK_DOOR);        // task to lock the door again
Task tDSt(200, TASK_FOREVER, &tDStCallback);  // check door contacts status
Task tRBe(1,TASK_ONCE, &LED_BEEP);            // task for Reader beeper
Task tBCl(100, TASK_ONCE, &DispBlCl);         // task to clear display and Backlight off


WIEGAND wg;
Scheduler runner;
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// VARIABLES
byte ahis = 0;       // door status history value ****
byte bhis = 1;       // manuell door open signal von SIEDLE
byte rhis = 1;       // ring signal von SIEDLE - added by Dieter Haude on 24.05.2018
byte SEC_OPEN = 1;    // number of seconds to hold door open after valid UID

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

  pinMode(LOCK_SENSE, INPUT_PULLUP);
  pinMode(SIEDLE, INPUT_PULLUP);
  pinMode(RING, INPUT_PULLUP);  // added by Dieter Haude on 24.05.2018

  digitalWrite(OPEN_PULSE, true);
  digitalWrite(OPEN_PERM, true);
  digitalWrite(BEEP, true);
  digitalWrite(LED, true);
  digitalWrite(LED_1, false);
  digitalWrite(LED_2, false);
  digitalWrite(LED_3, false);

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
  wdt_enable(WDTO_2S);  // Set up Watchdog Timer 2 seconds - added by Dieter Haude on 17.08.2018
  tRFR.enable();        // start cyclic readout of reader
  tDSt.enable();        // start cyclic readout of door status
  lcd.clear();
  lcd.noBacklight();
}

// FUNCTIONS (Tasks) ----------------------------
void tRFRCallback() {
  // Setze Watchdog Zähler zurück
  wdt_reset();  // added by Dieter Haude on 17.08.2018
  if (wg.available() && wg.getCode() > 5)  {                // check for data on Wiegand Bus
   Serial.println((String)"card;" + wg.getCode());
    wg.delCode();                                           //
  }
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
  tDLo.restartDelayed(SEC_OPEN * SECONDS);  // start task in 'SEC_OPEN' sec to close door
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
  byte len = inStr.length();          // check lenght changed by MM 10.01.2018
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
    tBCl.restartDelayed(SEC_LIGHT * SECONDS);      // changed by DieterH on 18.10.2017
    return;
  }

  if (inStr.substring(0, 3) == "R2T") {  // print to LCD row 2 changed DH 24.1.
    inStr.concat("              ");     // add blanks to string  changed by MM 10.01.2018
    lcd.backlight(); lcd.setCursor(0,1);
    lcd.print(inStr.substring(3,19));   // cut string lenght to 16 char  changed by MM 10.01.2018
    tBCl.restartDelayed(SEC_LIGHT * SECONDS);      // changed by DieterH on 18.10.2017
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
