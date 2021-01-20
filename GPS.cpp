// from https://raw.githubusercontent.com/DennisSc/PPS-ntp-server/master/src/GPS.cpp

#include <Arduino.h>
#include "DateTime.h"
#include "GPS.h"
#include "InputCapture.h"
#include "settings.h"

#define GPS_CODE_ZDA "GPZDA"
#define GPS_CODE2_ZDA "GNZDA"
#define GPS_CODE_RMC "GPRMC"
#define GPS_CODE_GGA "GPGGA"
 
/**
 * Save new date and time to private variables
 */
void GPSDateTime::commit() {
  time_ = newTime_;
  year_ = newYear_;
  month_ = newMonth_;
  day_ = newDay_;
  // RMC takes the PPS snapshot on the GPS_CODE_GGA message
#ifndef GPS_USES_RMC
  ppsCounter_ = pps.getCount();
  ppsMillis_ = pps.getMillis();
  dateMillis = millis();
#endif
}

void GPSDateTime::time(String time) {
  newTime_ = time.toFloat() * 100;
}

uint16_t GPSDateTime::hour() {
  return time_ / 1000000;
}

uint16_t GPSDateTime::minute() {
  return (time_ / 10000) % 100;
}

uint16_t GPSDateTime::second() {
  return (time_ / 100) % 100;
}

void GPSDateTime::day(String day) {
  newDay_ = day.toInt();
}
uint16_t GPSDateTime::day(void) { return day_; };

void GPSDateTime::month(String month) {
  newMonth_ = month.toInt();
}
uint16_t GPSDateTime::month(void) { return month_; };

void GPSDateTime::year(String year) {
  newYear_ = year.toInt();
}
uint16_t GPSDateTime::year(void) { return year_; };

void GPSDateTime::rmctime(String timestr) {
  newTime_ = timestr.toFloat() * 100;
}

void GPSDateTime::rmcdate(String datestr) {
  int date = datestr.toInt();
  newDay_ = date / 10000;
  newMonth_ = (date / 100) % 100;
  newYear_ = date % 100 + 2000;
}

/**
 * Decode NMEA line to date and time
 * $GPZDA,174304.36,24,11,2015,00,00*66
 * $0    ,1        ,2 ,3 ,4   ,5 ,6 *7  <-- pos
 * @return line decoded
 */
bool GPSDateTime::decode() {
  char c = gpsUart_->read();

  if (c == '$') {
   
    tmp = "\0";
    msg = "$";
    count_ = 0;
    parity_ = 0;
    validCode = true;
    isNotChecked = true;
    isUpdated_ = false;
    return false;
  }

  if (!validCode) {
    return false;
  }
  msg += c;
  if (c == ',' || c == '*') {
    // determinator between values
    switch (count_) {
      case 0: // ID
#ifdef GPS_USES_RMC
        if (tmp.equals(GPS_CODE_RMC)) {
          validCode = true;
        } else if (tmp.equals(GPS_CODE_GGA)) {
          ppsCounter_ = pps.getCount();
          ppsMillis_ = pps.getMillis();
          dateMillis = millis();
          validCode = false;
#else // GPS_USES_RMC
        if (tmp.equals(GPS_CODE_ZDA) || tmp.equals(GPS_CODE2_ZDA)) {
          validCode = true;
#endif
        } else {
          validCode = false;
        }
        break;
      case 1: // time
#ifdef GPS_USES_RMC
        this->rmctime(tmp);
#else
        this->time(tmp);
#endif
        break;
      case 2: // day
#ifndef GPS_USES_RMC
        this->day(tmp);
#endif
        break;
      case 3: // month
#ifndef GPS_USES_RMC
        this->month(tmp);
#endif
        break;
      case 4: // year
#ifndef GPS_USES_RMC
        this->year(tmp);
#endif
        break;
#ifdef GPS_USES_RMC
      case 9:
        this->rmcdate(tmp);
        break;
#endif
      case 5: // timezone fields are ignored
      case 6:
      default:
        break;
    }
    if (c == ',') {
      parity_ ^= (uint8_t) c;
    }
    if (c == '*') {
      isNotChecked = false;
    }
    tmp = "\0";
    count_++;
  } else if (c == '\r' || c == '\n') {
    // carriage return, so check
    String checksum = tmp;
    String checkParity = String(parity_, HEX);
    checkParity.toUpperCase();

    validString = checkParity.equals(checksum);

    if (validString) {
      this->commit();
      isUpdated_ = true;
      // commit datetime
    }

    // end of string
    tmp = "\0";
    count_ = 0;
    parity_ = 0;
    return validString;
  } else {
    // ordinary char
    tmp += c;
    if (isNotChecked) {
      // XOR of all characters from $ to *
      parity_ ^= (uint8_t) c;
    }
  }

  return false;
}



/**
 * Return instance of DateTime class
 * @return DateTime
 */
DateTime GPSDateTime::GPSnow() {
  return DateTime(this->year(), this->month(), this->day(), this->hour(), this->minute(), this->second());
}
