//#pragma GCC diagnostic error "-Wconversion"

#define DEBUG_LEVEL 1 // 0 = OFF

/*
   HABalloon by KD2NDR, Miami Florida October 25 2018
   Improvements by YO3ICT, Bucharest Romania, April-May 2019
   Improvements By SA6BSS, Sweden, fall 2020
   You may use and modify the following code to suit
   your needs so long as it remains open source
   and it is for non-commercial use only.
*/
#include <Sodaq_wdt.h>
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

void setup()
{
  #if DEBUG_LEVEL >= 1
  setupSoftwareSerialLogging();
  DEBUGPRINT(1);
  #endif
  
  sodaq_wdt_enable(WDT_PERIOD_8X);

  setupRfPins();
  rf_off();

  setupGpsPins();
  GPS_VCC_on();

  configureGps();
  configureTimerInterrupts();
  
  sodaq_wdt_reset();

  DEBUGPRINT(2);
}

void loop()
{
  sodaq_wdt_reset();
  while (Serial.available() > 0)
  {
    if (gps.encode(Serial.read()) && (timeStatus() == timeNotSet))  // GPS related functions need to be in here to work with tinyGPS Plus library
    {                                                               // Only sets time if already not done previously
      DEBUGPRINT(3);
      setGPStime();   // Sets system time to GPS UTC time for sync
    }
  }

  if (gps.location.isValid())
  {
    DEBUGPRINT(4);
    TXtiming(); // Process timing
  }
}

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
