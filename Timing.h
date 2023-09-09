void sendWsprMessage(bool isTelemetry)
{
  DEBUGPRINT(30);
  if (telemetry_set)
  {
    DEBUGPRINT(32);
    GPS_VCC_off();
    delay(10);
    rf_on();
    freq = WSPR_FREQ;
    if (isTelemetry)
    {
      DEBUGPRINT(33);
      setModeWSPR_telem(); // set WSPR telemetry mode
    }
    else
    {
      DEBUGPRINT(34);
      setModeWSPR(); // set WSPR standard mode
    }
    encode(); // begin radio transmission
    rf_off();
    delay(5);
    GPS_VCC_on();
  }
}

void calculateTelemetry()
{
  DEBUGPRINT(28);
  loc4calc(); // Get position and update 4-char locator, 6-char locator and last 2 chars of 8-char locator
  call_telem(); // Update WSPR telemetry callsign based on previous information : position and altitude in meters
  loc_dbm_telem(); // Update WSPR telemetry locator and dbm. Get temperature, voltage, speed in knots, GPS status and sats number
  telemetry_set = true;
}

inline void invalidateTelemetry()
{
  DEBUGPRINT(29);
  telemetry_set = false;
}

void TXtiming() // Timing
{
  // run additional scripts here to generate data prior to TX if there is a large delay involved.
  if ((timeStatus() == timeSet) && (second() == 0))
  {
    DEBUGPRINT(26);

    int minute10 = minute() % 10;

    DEBUGVALUE(27, minute10);

    switch (minute10)
    {
      case 0: calculateTelemetry();
        sendWsprMessage(false);
        break;

      case 1: break;

      case 2: sendWsprMessage(true);
        break;

      case 3: break;
      case 4: sendWsprMessage(false);
        break;
      case 5: break;
      case 6: sendWsprMessage(false);
        invalidateTelemetry();
        break;
      case 7: break;
      case 8: cwDebug(3);
        break;
      case 9: setGPStime(); // refresh time from GPS every 10 mins
        break;
      default: break;
    }
  }
}
