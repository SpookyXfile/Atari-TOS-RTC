/******************************************************************
 Created with PROGRAMINO IDE for Arduino - 06.08.2018 15:23:03
 Project     : Atari ST IKBD clock injector with DS3231 RTC
 Libraries   : SoftwareSerial, Wire
 Author      : TzOk
 Description : ARD_RX0 from KB_5, ARD_TX1 to ST_5,
               ARD_D5 from/to KB/ST_6, ARD_A3(17) to DS3231 Vcc.
******************************************************************/

#include <SoftwareSerial.h>
#include <Wire.h>

#define DS3231_ADDRESS (0x68)
#define DS3231_REG_TIME (0x00)

SoftwareSerial Ctrl(5,6);

byte cmd;
byte inj = 255;
byte dsDate[7];
byte stDate[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC};
//                  ss,   mm,   hh,   DD,   MM,   YY

void setup()
{ 
  pinMode(17, OUTPUT);
  digitalWrite(17, HIGH); // Power for DS3231
  Serial.begin(7812);
  Serial.setTimeout(10);
  Ctrl.begin(7812);
  Ctrl.listen();
  Wire.begin();
}

void loop()
{
  if (Ctrl.available())
  {
    cmd = Ctrl.read();
    if (cmd == 0x1C) // Read RTC
    {
      Wire.beginTransmission(DS3231_ADDRESS);
      Wire.write(DS3231_REG_TIME);
      Wire.endTransmission();
      Wire.requestFrom(DS3231_ADDRESS, 7);
      while(!Wire.available()) {};
      for (byte i = 6; i < 255; i--)
      {
        dsDate[i] = Wire.read();
      }
      stDate[5] = (dsDate[1] & 0x80) ? dsDate[0] + 0xA0 : dsDate[0]; // YY : if CENTURY bit is SET => stDate = dsDate + 100
      stDate[4] = dsDate[1] & 0x1F; // MM
      stDate[3] = dsDate[2]; // DD
      stDate[2] = dsDate[4] & 0x3F; // hh
      stDate[1] = dsDate[5]; // mm
      stDate[0] = dsDate[6]; // ss
      inj = 6;
    }
    else if (cmd == 0x1B) // Set RTC
    {
      for (byte i = 5; i < 255; i--)
      {
        while(!Ctrl.available()) {};
        stDate[i] = Ctrl.read();
      }
      dsDate[0] = (stDate[5] < 0xA0) ? stDate[5] : stDate[5] - 0xA0; // YY : if stDate > 99 => dsDate = stDate - 100
      dsDate[1] = (stDate[5] < 0xA0) ? stDate[4] : stDate[4] + 0x80; // MM : if stDate > 99 => set CENTURY bit
      dsDate[2] = stDate[3]; // DD
      dsDate[3] = 0x01; // Day of Week : don't care, any valid value 1-7 will be ok
      dsDate[4] = stDate[2]; // hh
      dsDate[5] = stDate[1]; // mm
      dsDate[6] = stDate[0]; // ss
      Wire.beginTransmission(DS3231_ADDRESS);
      Wire.write(DS3231_REG_TIME);
      for (byte i = 6; i < 255; i--)
      {
        Wire.write(dsDate[i]);
      }
      Wire.endTransmission();
    }
  }
}

void serialEvent() {
  if (inj == 255)
    Serial.write(Serial.read());
  else
  {
    Serial.read();
    Serial.write(stDate[inj--]);
  }
}

