/*
   Telemetry functions
*/

void setGPStime() // Sets the system time from the GPS
{
  if (gps.date.isValid() && gps.time.isValid() && (gps.date.year() > 2022))
  {
    DEBUGPRINT(5);
    byte hour = gps.time.hour();
    byte minute = gps.time.minute();
    byte second = gps.time.second();
    DEBUGVALUE(6, hour);
    DEBUGVALUE(7, minute);
    DEBUGVALUE(8, second);

    setTime(hour, minute, second, 1, 1, 2000); // (hr,min,sec,day,month,yr)
  }
}

void loc4calc() // Determine the locator from the GPS data
{
  DEBUGPRINT(9);
  lon = (gps.location.lng() * 100000) + 18000000L;
  lat = (gps.location.lat() * 100000) +  9000000L;

  loc4[0] = 'A';
  loc4[1] = 'A';
  loc4[2] = '0';
  loc4[3] = '0';

  loc4[0] +=  lon / 2000000;
  loc4[1] +=  lat / 1000000;
  loc4[2] += (lon % 2000000) / 200000;
  loc4[3] += (lat % 1000000) / 100000;

  DEBUGSTRING(10, loc4);
}

void call_telem() // Determine the telemetry callsign
{
  DEBUGPRINT(11);
  alt_meters = gps.altitude.meters();
  DEBUGVALUE(12, alt_meters);
  char MH[2] = {'A', 'A'};
  MH[0] += ((lon % 200000) / 8333);
  MH[1] += ((lat % 100000) / 4166);
  DEBUGSTRING(13, MH);
  call_telemetry[0] = 'Q'; // telemetry channel 11
  call_telemetry[2] = '3';
  int a = MH[0] - 'A';
  int b = MH[1] - 'A';
  int c = a * 24 + b;
  int d = int(alt_meters / 20);
  long e = (long)(1068L * c + d);
  long f = float(e / 17576L);

  if (f < 10)
  {
    call_telemetry[1] = f + '0';
  }
  else
  {
    call_telemetry[1] = f - 10 + 'A';
  }

  long g = e - f * 17576L;
  int h = int(g / 676);
  call_telemetry[3] = h + 'A';
  long i = g - h * 676L;
  int j = int(i / 26);
  call_telemetry[4] = j + 'A';
  long k = i - j * 26L;
  int l = int(k / 1);
  call_telemetry[5] = l + 'A';
}

inline float readVcc()
{
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  float volts = result / 1000.0; // in V
  return volts;
}

void loc_dbm_telem() // Determine the locator and dBm value for the telemetry transmission
{
  DEBUGPRINT(14);
  Sats =  gps.satellites.value();
  DEBUGVALUE(15, Sats);
  gps_speed = gps.speed.knots();
  DEBUGVALUE(16, gps_speed);
  int wADC = 0;
  int temp = 0;
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN);
  delay(20);
  for (int i = 0; i < 5; i++)
  {
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA, ADSC));
    wADC = wADC + ADCW;
    delay(20);
  }

  wADC = wADC / 5;
  temp = (wADC - 304.21 ) / 1.124;
  DEBUGVALUE(17, temp);
  delay(20);

  float volt = readVcc();
  DEBUGVALUE(18, volt);
  volt = constrain(volt, 1.8, 3.75);
  float batt_raw = map(volt * 100, 1.8 * 100, 3.75 * 100, 0, 39); // map only works with int type
  batt_raw = constrain(batt_raw, 0, 39);
  DEBUGVALUE(31, batt_raw);

  if (temp < -49) temp = -49;
  if (temp > 39) temp = 39;
  int GPS = 0;
  if ((lon != oldlon) || (lat != oldlat))
  {
    GPS = 1;
  }
  else
  {
    GPS = 0;
  }

  oldlon = lon;
  oldlat = lat;
  if (Sats < 5) Sats = 0; else Sats = 1;
  int temp_raw = (int)(1024L * (temp * 0.01 + 2.73)) / 5;
  temp_raw = (int)(temp_raw - 457) / 2;

  long x = (temp_raw * 40L ) + batt_raw;
  long y = (x * 42L) + (int)gps_speed / 2;
  long z = (y * 2L) + (int)GPS;
  long xy = (z * 2L) + (int)Sats;
  long a = (int)(xy / 34200L);
  loc_telemetry[0] = a + 'A';
  long b = xy - (34200L * a);
  long c = (int)(b / 1900L);
  loc_telemetry[1] = c + 'A';
  long d = b - (1900L * c);
  long e = (int)(d / 190L);
  loc_telemetry[2] = e + '0';
  long f = d - (190L * e);
  long g = (int) (f / 19L);
  loc_telemetry[3] = g + '0';
  dbm_telemetry = f - (g * 19L);
  if (dbm_telemetry == 0) dbm_telemetry = 0;
  else if (dbm_telemetry == 1) dbm_telemetry = 3;
  else if (dbm_telemetry == 2) dbm_telemetry = 7;
  else if (dbm_telemetry == 3) dbm_telemetry = 10;
  else if (dbm_telemetry == 4) dbm_telemetry = 13;
  else if (dbm_telemetry == 5) dbm_telemetry = 17;
  else if (dbm_telemetry == 6) dbm_telemetry = 20;
  else if (dbm_telemetry == 7) dbm_telemetry = 23;
  else if (dbm_telemetry == 8) dbm_telemetry = 27;
  else if (dbm_telemetry == 9) dbm_telemetry = 30;
  else if (dbm_telemetry == 10) dbm_telemetry = 33;
  else if (dbm_telemetry == 11) dbm_telemetry = 37;
  else if (dbm_telemetry == 12) dbm_telemetry = 40;
  else if (dbm_telemetry == 13) dbm_telemetry = 43;
  else if (dbm_telemetry == 14) dbm_telemetry = 47;
  else if (dbm_telemetry == 15) dbm_telemetry = 50;
  else if (dbm_telemetry == 16) dbm_telemetry = 53;
  else if (dbm_telemetry == 17) dbm_telemetry = 57;
  else if (dbm_telemetry == 18) dbm_telemetry = 60;
  //Serial.println(dbm_telemetry);
}

void setModeWSPR()
{
  DEBUGPRINT(19);
  symbol_count = WSPR_SYMBOL_COUNT;
  tone_spacing = WSPR_TONE_SPACING;
  memset(tx_buffer, 0, 255); // Clears Tx buffer from previous operation.
  jtencode.wspr_encode(call, loc4, 10, tx_buffer);
}

void setModeWSPR_telem()
{
  DEBUGPRINT(20);
  symbol_count = WSPR_SYMBOL_COUNT;
  tone_spacing = WSPR_TONE_SPACING;
  memset(tx_buffer, 0, 255); // Clears Tx buffer from previous operation.
  jtencode.wspr_encode(call_telemetry, loc_telemetry, dbm_telemetry, tx_buffer);
}

void encode() // Loop through the string, transmitting one character at a time
{
  DEBUGPRINT(21);
  uint8_t i;
  for (i = 0; i < symbol_count; i++) // Now transmit the channel symbols
  {
    //si5351.output_enable(SI5351_CLK0, 1); // Turn off the CLK0 output
    //si5351.set_freq_manual((freq * 100) + (tx_buffer[i] * tone_spacing),87500000000ULL,SI5351_CLK0);
    si5351.set_freq((freq * 100) + (tx_buffer[i] * tone_spacing), SI5351_CLK0);
    proceed = false;
    while (!proceed);
    sodaq_wdt_reset();
  }
  si5351.output_enable(SI5351_CLK0, 0); // Turn off the CLK0 output
  si5351.set_clock_pwr(SI5351_CLK0, 0);  // Turn off the CLK0 clock
}

void rf_on() // Turn on the high-side switch, activating the transmitter
{
  DEBUGPRINT(22);
  digitalWrite(2, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);
  digitalWrite(7, HIGH);
  delay(2);
  si5351.init(SI5351_CRYSTAL_LOAD_0PF, 26000000, 0); // TCXO 26MHz
  si5351.set_clock_pwr(SI5351_CLK1, 0);  // Turn off the CLK1 clock
  si5351.output_enable(SI5351_CLK1, 0);  // Turn off the CLK1 output
  si5351.set_clock_pwr(SI5351_CLK2, 0);  // Turn off the CLK2 clock
  si5351.output_enable(SI5351_CLK2, 0);  // Turn off the CLK2 output
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA); // Set for max power if desired. Check datasheet.
  si5351.output_enable(SI5351_CLK0, 0);  // Disable the clock initially
}

void rf_off() // Turn off the high-side switch
{
  DEBUGPRINT(23);
  digitalWrite(2, LOW);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);
  digitalWrite(7, LOW);
}

void GPS_VCC_on()
{
  DEBUGPRINT(24);
  Serial.begin(9600);
  delay(2);
  digitalWrite(A0, HIGH);
  digitalWrite(A1, HIGH);
  digitalWrite(A2, HIGH);
  digitalWrite(A3, HIGH);
}

void GPS_VCC_off()
{
  DEBUGPRINT(25);
  Serial.end();
  delay(2);
  digitalWrite(A0, LOW);
  digitalWrite(A1, LOW);
  digitalWrite(A2, LOW);
  digitalWrite(A3, LOW);
}

#if CW_DEBUG >= 1

// 1 device started
// 2 got GPS time
// 3 8th minute

void cwShort()
{
  si5351.set_freq(freq * 100, SI5351_CLK0);
  sodaq_wdt_reset();
  delay(CW_DIT_MS);
  si5351.output_enable(SI5351_CLK0, 0); // Turn off the CLK0 output
  si5351.set_clock_pwr(SI5351_CLK0, 0);  // Turn off the CLK0 clock
  delay(CW_DIT_MS);
}

void cwLong()
{
  si5351.set_freq(freq * 100, SI5351_CLK0);
  sodaq_wdt_reset();
  delay(CW_DIT_MS * 3);
  si5351.output_enable(SI5351_CLK0, 0); // Turn off the CLK0 output
  si5351.set_clock_pwr(SI5351_CLK0, 0);  // Turn off the CLK0 clock
  delay(CW_DIT_MS);
}

void cwDebug(byte message)
{
  if (loc4[0] != 'K' || loc4[1] != 'O' || loc4[2] != '2' || loc4[3] != '4')
  {
    return; // only send CW debug information in KO24 grid square
  }

  GPS_VCC_off();
  delay(10);
  rf_on();
  freq = DEBUG_FREQ;

  cwLong();
  cwLong();
  cwLong();

  delay(500);

  for (int i = 0; i < message; i++)
  {
    cwShort();
  }

  rf_off();
  delay(5);
  GPS_VCC_on();
}

#endif
