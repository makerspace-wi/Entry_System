#include <Wiegand.h>
#include <TaskScheduler.h>
#include <Ethernet.h>
#include <SPI.h>

// --- Ethernet Settings (Static IP) ---
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress staticIP(192, 168, 1, 100); // Your desired static IP
IPAddress gateway(192, 168, 1, 1);    // Router IP
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress dns(8, 8, 8, 8);            // Optional: DNS server (Google)
EthernetServer ethServer(80);         // Port 80 (HTTP)

EthernetClient ethClient;

// --- Door Control Settings ---
#define SEC_OPEN 10  // Seconds to hold door open
#define SECONDS 1000 // Multiplier for second

// --- Pin Assignments (RP2040-ETH Mini) ---
#define DATA0 2      // Wiegand Data 0
#define DATA1 3      // Wiegand Data 1
#define LED 4        // LED (active LOW)
#define BEEP 5       // Beeper (active LOW)
#define OPEN_CLOSE 6 // Lock control (HIGH = unlock)
#define RIEGEL 7     // Door sensor (INPUT_PULLUP)
#define KLINKE 8     // Door handle sensor (INPUT_PULLUP)

// Callback methods prototypes
void t1Callback();
void t3Callback();
void LOCK_DOOR();
void LED_BEEP();

WIEGAND wg;
Scheduler runner;

// Tasks
Task t1(500, TASK_FOREVER, &t1Callback, &runner, true); // task checking the RFID-Reader
Task t2(100, TASK_ONCE, &LOCK_DOOR, &runner, false);     // task to lock the door again
Task t3(250, TASK_FOREVER, &t3Callback, &runner,true); // check door contacts status
Task t4(1, TASK_ONCE, &LED_BEEP, &runner, false);        // task for Reader beeper

// VARIABLES
bool ahis = true;  // Riegel history value
bool bhis = true;  // Klinke history value
byte dshis = true; // door status history value

int bufferCount; // Anzahl der eingelesenen Zeichen
char buffer[10]; // Serial Input-Buffer

void setup()
{
  Serial.begin(9600);
  wg.begin();

  // --- Pin Modes & Default States ---
  pinMode(DATA0, INPUT);
  pinMode(DATA1, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(BEEP, OUTPUT);
  pinMode(OPEN_CLOSE, OUTPUT);
  pinMode(RIEGEL, INPUT_PULLUP);
  pinMode(KLINKE, INPUT_PULLUP);

  digitalWrite(OPEN_CLOSE, LOW);
  digitalWrite(BEEP, HIGH);
  digitalWrite(LED, HIGH);

  // --- Ethernet with Static IP ---
  Ethernet.init(17); // CS Pin for W5500 (RP2040-ETH Mini)
  Ethernet.begin(mac, staticIP, dns, gateway, subnet);

  // Check if Ethernet is connected
  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println("Ethernet shield not found!");
    while (1)
      ;
  }
  if (Ethernet.linkStatus() == LinkOFF)
  {
    Serial.println("Ethernet cable not connected!");
  }

  ethServer.begin();
  Serial.print("Static IP: ");
  Serial.println(Ethernet.localIP());

  // --- Start Tasks ---
  runner.startNow();

}

// --- TASK FUNCTIONS ---
void t1Callback()
{
  if (wg.available())
  {
    if (wg.getCode() < 1000)
      return; // Ignore invalid codes
    t1.disable();
    Serial.print("card;");
    Serial.println(wg.getCode());

    // Send RFID to Ethernet Client (if connected)
    if (ethClient && ethClient.connected())
    {
      ethClient.print("card;");
      ethClient.println(wg.getCode());
    }

    t1.enableDelayed(5 * SECONDS); // Delay next read
  }
}

void t3Callback()
{
  byte a = digitalRead(RIEGEL);
  if (a != ahis)
  {
    ahis = a;
    Serial.print("door;");
    Serial.println(a);

    // Send door status to Ethernet Client
    if (ethClient && ethClient.connected())
    {
      ethClient.print("door;");
      ethClient.println(a);
    }
  }
}

// --- DOOR CONTROL ---
void LOCK_DOOR()
{
  digitalWrite(OPEN_CLOSE, LOW);
  digitalWrite(LED, HIGH);
}

void UNLOCK_DOOR()
{
  digitalWrite(OPEN_CLOSE, HIGH);
  digitalWrite(LED, LOW);
  t2.restartDelayed(SEC_OPEN * SECONDS); // Auto-lock after delay
}

// --- BEEPER CONTROL ---
void LED_BEEP()
{
  digitalWrite(BEEP, LOW);
  t4.setCallback(&LED_BEEP2);
  t4.restartDelayed(200);
}
void LED_BEEP2()
{
  digitalWrite(BEEP, HIGH);
  t4.setCallback(&LED_BEEP3);
  t4.restartDelayed(100);
}
void LED_BEEP3() { t4.setCallback(&LED_BEEP); }

void LED_2TBEEP()
{
  digitalWrite(BEEP, LOW);
  t4.setCallback(&LED_2TBEEP2);
  t4.restartDelayed(200);
}
void LED_2TBEEP2()
{
  digitalWrite(BEEP, HIGH);
  t4.setCallback(&LED_BEEP);
  t4.restartDelayed(100);
}

void LED_3TBEEP()
{
  digitalWrite(BEEP, LOW);
  t4.setCallback(&LED_3TBEEP2);
  t4.restartDelayed(200);
}
void LED_3TBEEP2()
{
  digitalWrite(BEEP, HIGH);
  t4.setCallback(&LED_2TBEEP);
  t4.restartDelayed(100);
}

// --- ETHERNET COMMAND HANDLER ---
void handleEthCommand(char cmd)
{
  switch (cmd)
  {
  case 'a':
  case 'A':
    UNLOCK_DOOR();
    break;
  case 'b':
  case 'B':
    t4.setCallback(&LED_BEEP);
    t4.restart();
    break;
  case 'c':
  case 'C':
    t4.setCallback(&LED_2TBEEP);
    t4.restart();
    break;
  case 'd':
  case 'D':
    t4.setCallback(&LED_3TBEEP);
    t4.restart();
    break;
  }
}

void loop()
{
  runner.execute();

  // Handle Ethernet Clients
  ethClient = ethServer.available();
  if (ethClient && ethClient.connected())
  {
    while (ethClient.available())
    {
      char c = ethClient.read();
      if (c == '>')
        bufferCount = 0; // Start of command
      buffer[bufferCount++] = c;

      if (c == '<' && bufferCount >= 3)
      { // End of command
        if (buffer[0] == '>' && buffer[2] == '<')
        {
          handleEthCommand(buffer[1]);
        }
        bufferCount = 0;
      }
    }
  }
}
