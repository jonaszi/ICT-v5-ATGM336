//#pragma GCC diagnostic error "-Wconversion"

#define DEBUG_LEVEL 1 // 0 = OFF
#define CW_DEBUG 1 // 0 = OFF

/*
   HABalloon by KD2NDR, Miami Florida October 25 2018
   Improvements by YO3ICT, Bucharest Romania, April-May 2019
   Improvements By SA6BSS, Sweden, fall 2020
   You may use and modify the following code to suit
   your needs so long as it remains open source
   and it is for non-commercial use only.
*/
#include <si5351.h>
#include <JTEncode.h>
#include <TimeLib.h>
#include <TinyGPS++.h>

#if DEBUG_LEVEL >= 1
#include <SoftwareSerial.h>

void logMessage(byte message);
void logValue(byte message, float value);
void logString(byte message, const char* value);

#define DEBUGPRINT(message) logMessage(message)
#define DEBUGVALUE(message, value) logValue(message, value)
#define DEBUGSTRING(message, value) logString(message, value)
#else
#define DEBUGPRINT(message)
#define DEBUGVALUE(message, value)
#define DEBUGSTRING(message, value)
#endif

#define WSPR_CTC                2668   // CTC value for WSPR - 10672 @ 16Mhz //5336 @ 8Mhz //2668 @ 4Mhz //1334 @ 2Mhz //667 @ 1Mhz
#define WSPR_TONE_SPACING       146    // 146 ~1.46 Hz

#define WSPR_FREQ     14097192UL      // <<<<< SET TX FREQUENCY HERE

#define DEBUG_FREQ    14097192UL      // Debug CW frequency (can be same as WSPR_FREQ, as only one transmission possible at the same time)
#define CW_DIT_MS     100             // TODO find reasonable value

#define own_wdt_reset() __asm__ __volatile__ ("wdr")

// Enumerations
TinyGPSPlus gps;
Si5351 si5351;
JTEncode jtencode;

#if DEBUG_LEVEL >= 1
SoftwareSerial debugSerial(8, 11); // RX (8 = PB0 = not connected), TX (11 = PB3 = MOSI)
#endif

// Global variables
unsigned long freq;
char call[] = "LY1OS"; // WSPR Standard callsign
char call_telemetry[7]; // WSPR telemetry callsign
char loc_telemetry[5]; // WSPR telemetry locator
uint8_t dbm_telemetry; // WSPR telemetry dbm
char loc4[5]; // 4 digit gridsquare locator
long lat, lon, oldlat, oldlon; // Used for location
uint8_t tx_buffer[255]; // WSPR Tx buffer
uint8_t symbol_count; // JTencode
uint16_t tone_delay, tone_spacing; // JTencode
int alt_meters = 0;
bool telemetry_set = false;
int Sats = 0;
int gps_speed = 0;
volatile bool proceed = false;

#include "TelemFunctions.h" // Various telemetry functions
#include "Timing.h" // Scheduling

ISR(TIMER1_COMPA_vect)
{
  proceed = true;
}

void(* resetFunc) (void) = 0;

ISR(WDT_vect) // Use WDT interrupt rather than reset, as reset doesn't work (hangs application). Probably because of some
{             // Arduino infrastructure code messing up WDT before the point it could be initialised
  resetFunc();
}

void setup()
{
#if DEBUG_LEVEL >= 1
  setupSoftwareSerialLogging();
#endif

  DEBUGPRINT(1);

  own_wdt_configure();

  setSyncProvider(0); // make sure sync provider not used
  setSyncInterval(3600); // should not use, but set to 1hr just in case

  setupRfPins();
  rf_off();

  setupGpsPins();
  GPS_VCC_on();

  configureGps();
  configureTimerInterrupts();

  own_wdt_reset();

  DEBUGPRINT(2);

  cwDebug(1);
}

void loop()
{
  own_wdt_reset();
  while (Serial.available() > 0)
  {
    if (gps.encode(Serial.read()) && (timeStatus() == timeNotSet))  // GPS related functions need to be in here to work with tinyGPS Plus library
    { // Only sets time if already not done previously
      setGPStime();   // Sets system time to GPS UTC time for sync
      DEBUGPRINT(3);
      cwDebug(2); // Does it interfer with GPS??
    }
  }

  if (gps.location.isValid()) // Once set to true, this seems to always stay true, but not much we can do about it
  {
    TXtiming(); // Process timing
  }
}

#if DEBUG_LEVEL >= 1

inline void setupSoftwareSerialLogging()
{
  debugSerial.begin(57600);
}

void logMessage(byte message)
{
  logTime();
  debugSerial.println(message);
}

void logValue(byte message, float value)
{
  logTime();
  debugSerial.print(message);
  debugSerial.print("=");
  debugSerial.println(value);
}

void logString(byte message, const char* value)
{
  logTime();
  debugSerial.print(message);
  debugSerial.print("=");
  debugSerial.println(value);
}

void logTime()
{
  debugSerial.print(hour());
  debugSerial.print(":");
  debugSerial.print(minute());
  debugSerial.print(":");
  debugSerial.print(second());
  debugSerial.print(" #");
}

#endif

inline void setupRfPins()
{
  pinMode(2, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
}

inline void setupGpsPins()
{
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH); //gps pin 5 on

  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
}

inline void configureGps()
{
  delay(1000);
  Serial.write("$PCAS04,1*18\r\n"); //Sets navsystem of the ATGM to GPS only
  delay(1000);
}

inline void configureTimerInterrupts()
{
  noInterrupts(); // Set up Timer1 for interrupts every symbol period.
  TCCR1A = 0;
  TCNT1  = 0;
  TCCR1B = (1 << CS12) |
           (1 << CS10) |
           (1 << WGM12);
  TIMSK1 = (1 << OCIE1A);
  OCR1A = WSPR_CTC;
  interrupts();
}

inline void own_wdt_configure()
{
  cli();
  own_wdt_reset();
  WDTCSR |= (1<<WDCE) | (1<<WDE); // Start timed sequence
  WDTCSR = (1<<WDIE) | (1<<WDP3) | (1<<WDP0); // 8s
  sei();
}
