

// Import required libraries
#include <SoftwareSerial.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Hash.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>


//	I2C method Helper
#define byte uint8_t
#define sendWire( X ) write( static_cast<uint8_t>( X ) )
#define receiveWire( X ) read( X )

//	RTC define
#define RTCC_R			0xA3
#define RTCC_W			0xA2

#define RTCC_SEC		1
#define RTCC_MIN		2
#define RTCC_HR			3
#define RTCC_DAY		4
#define RTCC_WEEKDAY	5
#define RTCC_MONTH		6
#define RTCC_YEAR		7
#define RTCC_CENTURY	8

//	register addresses in the rtc
#define RTCC_STAT1_ADDR			0x00
#define RTCC_STAT2_ADDR			0x01
#define RTCC_SEC_ADDR			0x02
#define RTCC_MIN_ADDR			0x03
#define RTCC_HR_ADDR			0x04
#define RTCC_DAY_ADDR			0x05
#define RTCC_WEEKDAY_ADDR		0x06
#define RTCC_MONTH_ADDR			0x07
#define RTCC_YEAR_ADDR			0x08
#define RTCC_ALRM_MIN_ADDR		0x09
#define RTCC_SQW_ADDR			0x0D

#define RTCC_CENTURY_MASK		0x80
#define RTCC_VOLTLOW_MASK		0x80

//	square wave contants
#define SQW_1HZ					B10000011
#define SQW_32HZ				B10000010
#define SQW_1024HZ				B10000001
#define SQW_32KHZ				B10000000
#define SQW_DISABLE				B00000000



//	Uncomment to disable Debugging
//#define DEBUG

//	Uncomment for enable noneMCU LED
#define NONEMCU_LED

//	Uncomment for enable RTC Blinking LED
#define RTC_LED

//	Uncomment for enable LED
#define RTC_BUZZER

// Uncomment for enable Low Voltage WatchDog
//#define WATCH_LOWVOLTAGE



// Set internal noneMCU LED GPIO
#define	ledPin				2
// TOS State from Atari
#define atariTOSStatePin	3			//	RX Pin
// Set Select TOS noneMCU GPIO
#define selTOSPin			0			//	D3
// Set Reset Atari noneMCU GPIO
#define resetPin			1			//	TX Pin
// Set Buzzer noneMCU GPIO
#define buzzerPin			D0

// I2C SCL NodeMCU GPIO
#define i2cSCLPin			D1
// I2C SDA NodeMCU GPIO
#define i2cSDAPin			D2

// Software Serial RX NodeMCU GPIO
#define softSerialRXPin		D5
// Software Serial TX NodeMCU GPIO
#define softSerialTXPin		D6


//	default Wifi settings - must be blank
String wifiMode;
String apSSID;
String apPassword;
String apIP;
String staSSID;
String staPassword;
String staIP;
String darkTheme;

//	Wifi Scan SSID Data 
String networkSSID;


//	HTTP Web Page
//const char strHTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head></head></body></body></html>)rawliteral";

const char WM_HTTP_HEAD_START[] PROGMEM = "<!DOCTYPE html><html lang='en'><head><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/><title>{v}</title>";

const char HTTP_HEAD_LABEL[]	PROGMEM	= "<!DOCTYPE html><html lang='en'><HEAD><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" charset=\"UTF-8\"><LINK REL=\"icon\" TYPE=\"image/png\" HREF=\"./atari_icon.png\"><link rel=\"stylesheet\" type=\"text/css\" href=\"./style.css\"><TITLE>Atari RTC & TOS Settings</TITLE><script type=\"text/javascript\" src=\"./function.js\"></script></HEAD>\n<BODY>\n<hr></hr><h1 style=\"font-size:16px;\">Atari RTC & TOS Settings\n<hr></hr>\n";
const char HTTP_ROOT_LABEL[]	PROGMEM	= "<!DOCTYPE html><html lang='en'>\n<HEAD><meta http-equiv=\"refresh\" content=\"1; url=/\" charset=\"UTF-8\"><LINK REL=\"icon\" TYPE=\"image/png\" HREF=\"./atari_icon.png\"><link rel=\"stylesheet\" type=\"text/css\" href=\"./style.css\"><script type=\"text/javascript\" src=\"./function.js\"></script><TITLE>Atari RTC & TOS Settings</TITLE>\n</HEAD>\n<BODY>\n<script>setDarkTheme();</script>";
const char HTTP_END_LABEL[]		PROGMEM	= "\n</BODY>\n</HTML>";


//	I2C RTC Chip Address
const byte Rtcc_Addr = RTCC_R >> 1;

//	RTC Inject Data Service 
byte cmd = 0x00;
byte inj = 255;

//	RTC Data
byte rtcDate[ 7 ];
//					  ss,   mm,   hh,   DD,   MM,   YY	 Magic
byte stDate[ 7 ] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC };
byte century = 1;


//	Count Wifi Connection Attempt
byte networkCount;


#ifdef WATCH_LOWVOLTAGE
unsigned long	previousMillis	= 0;
const long		interval		= 10000;
bool			lowVoltage		= false;
#endif


unsigned long previousMillis = 0;

//	Reset Atari Data Helper
volatile boolean startReset;
//	Reset Atari at Power ON Starup - help to update sending RTC data to Atari
volatile boolean powerOnReset;


//	For Serial Keyboard Monitor - RX0, TX0 For NodeMCU	- Dont Care for TX Pin ( softSerialTX Pin )
SoftwareSerial Ctrl( softSerialRXPin, softSerialTXPin );

//	Create AsyncWebServer object on port 80
AsyncWebServer server( 80 );

// FTP config access - Define our user and Password security HERE
#ifdef DEBUG
#include <ESP8266FtpServer.h>
	String userIdentity = "ftpNodeMCU";
	String userPassword = "12345";
	FtpServer ftpSrv;
#endif


byte getAtariTOSState()
{
	
	return digitalRead( atariTOSStatePin );
}

String getAtariTOSVersion()
{
	byte AtariTOSVersion = getAtariTOSState();

#ifdef DEBUG
	Serial.print( F( "Atari TOS Version : " ) );
	Serial.println( AtariTOSVersion, DEC );
#endif

	return String( ( AtariTOSVersion == LOW ) ? "LOW TOS" : "HIGH TOS" );
}

void setSelectTOS( byte selectTOS )
{	
	digitalWrite( selTOSPin, selectTOS ? HIGH : LOW );
	
}

String getAtariRTC()
{
	String strDateTime;

	if ( !testPCF8563() )
	{
		strDateTime = "Not Defined";
	}
	else
	{
		getDate();
		getTime();

		strDateTime  = String( setTwoDigits (bcdToDec( rtcDate[ 4 ] ) ) );	//	Hour
		strDateTime += ":";
		strDateTime += setTwoDigits( bcdToDec( rtcDate[ 5 ] ) );			//	Minute
		strDateTime += "/";
		strDateTime += ( century == 0 ) ? "20" : "19";						//	Century
		strDateTime += setTwoDigits( rtcDate[ 0 ] );						//	Year
		strDateTime += "-";
		strDateTime += setTwoDigits( rtcDate[ 1 ] );						//	Month
		strDateTime += "-";
		strDateTime += setTwoDigits( rtcDate[ 2 ] );						//	Day

	#ifdef DEBUG
		Serial.println( F( "GET Atari RTC : " ) + strDateTime + F( "\n" ) );
	#endif
	}

	return String( strDateTime );
}

//	Replaces placeholder
String processor( const String& var )
{
#ifdef DEBUG
	Serial.print( F( "Processor value : " ) );
	Serial.println( var );
#endif

	if ( var == "ATARITOSVERSION" )
	{

		return getAtariTOSVersion();
	}

	if ( var == "ATARIRTC" )
	{

		return getAtariRTC();
	}

}



///////////////////////////////
//		Helper function		//
/////////////////////////////

//	Helpfull function to add '0' digit
String setTwoDigits( byte value )
{

	return ( value < 10 ) ? "0" + String( value ) : String( value );
}

//	Helpfull function for BCD Format
byte decToBcd ( byte val )
{

	return ( ( val / 10 * 16 ) + ( val % 10 ) );
}

byte bcdToDec( byte val )
{

	return ( ( val / 16 * 10 ) + ( val % 16 ) );
}

String getSplitParamValue( String data, char separator, byte index )
{
	int found = 0;

	int strIndex[]	= { 0, -1 };
	int maxIndex	= data.length() - 1;

	for ( byte i = 0; i <= maxIndex && found <= index; i++ )
	{
		if ( ( data.charAt( i ) == separator ) || ( i == maxIndex ) )
		{
			found++;

			strIndex[0] = strIndex[1] + 1;
			strIndex[1] = ( i == maxIndex ) ? i + 1 : i;
		}
	}

	return ( found > index ) ? data.substring( strIndex[0], strIndex[1] ) : "";
}

//
int removeDigit( String str )
{

	return str.toInt();
}

//
#ifdef RTC_LED
void debugInternalLED()
{
	digitalWrite( ledPin, LOW );
	delay( 350 );
	digitalWrite( ledPin, HIGH );

}
#endif


//
#ifdef NONEMCU_LED
void setLED( byte state )
{
	digitalWrite( ledPin, state ? LOW : HIGH );
	
}
#endif

///////////////////////////////
// PCF8563 RTC Function		//
/////////////////////////////

//	Test if PCF8563 Work Fine
bool testPCF8563()
{
  byte dataRSV = 0xFF;

  Wire.beginTransmission( Rtcc_Addr );		//  Issue I2C start signal
  Wire.sendWire( RTCC_STAT1_ADDR );			//  sendWire addr low byte, req'd
  Wire.endTransmission();

  Wire.requestFrom( Rtcc_Addr, 1 );

  dataRSV = Wire.receiveWire();

  return ( dataRSV == 0x08 ) ? true : false;
}

//	Set Time - in DEC Format
void setTime( byte hour, byte minute, byte second )
{
	Wire.beginTransmission( Rtcc_Addr );		//	Issue I2C start signal
	Wire.sendWire( RTCC_SEC_ADDR );				//	sendWire addr low byte, req'd

	Wire.sendWire( decToBcd( second ) );		//	set seconds
	Wire.sendWire( decToBcd( minute ) );		//	set minutes
	Wire.sendWire( decToBcd( hour ) );			//	set hour

	Wire.endTransmission();

}

// Set Date
void setDate( byte day, byte weekday, byte mon, byte century, byte year )
{
  //	year val is 00 to 99, xx
  //	with the MSB byte of month = century
  //	0 = 20xx
  //	1 = 19xx

  rtcDate[ 1 ] = decToBcd( mon );
  if ( century == 1 )
  {
    rtcDate[ 1 ] |= RTCC_CENTURY_MASK;
  }
  else
  {
    rtcDate[ 1 ] &= ~RTCC_CENTURY_MASK;
  }

  Wire.beginTransmission( Rtcc_Addr );	//	Issue I2C start signal
  Wire.sendWire( RTCC_DAY_ADDR );

  Wire.sendWire( decToBcd( day ) );		//	set day
  Wire.sendWire( decToBcd( weekday ) );	//	set weekday
  Wire.sendWire( rtcDate[ 1 ] );			//	set month, century to 1 or 0
  Wire.sendWire( decToBcd( year ) );		//	set year to 99

  Wire.endTransmission();

}

//	call this first to load current date values to variables */
void getDate()
{
  // set the start byte of the date data */
  Wire.beginTransmission( Rtcc_Addr );
  Wire.sendWire( RTCC_DAY_ADDR );
  Wire.endTransmission();

  Wire.requestFrom( Rtcc_Addr, 4 ); //request 4 bytes

  // Get Day from PCF RTC
  // 0x3f = 0b00111111
  rtcDate[ 2 ] = bcdToDec( Wire.receiveWire() & 0x3F );

  // Get WeekDay from PCF RTC
  // 0x07 = 0b00000111
  rtcDate[ 3 ] = bcdToDec( Wire.receiveWire() & 0x07 );

  // Get raw month data byte and set month and century with it.
  rtcDate[ 1 ] = Wire.receiveWire();
  if ( rtcDate[ 1 ] & RTCC_CENTURY_MASK )
  {
    century = 1;
  }
  else
  {
    century = 0;
  }
  //	0x1f = 0b00011111
  rtcDate[ 1 ] = bcdToDec( rtcDate[ 1 ] & 0x1F );

  // Get Year from PCF RTC
  rtcDate[ 0 ] = bcdToDec( Wire.receiveWire() );

}


//	call this first to load current time values to variables
void getTime()
{
  //	set the start byte, get the 2 status bytes
  Wire.beginTransmission( Rtcc_Addr );
  Wire.sendWire( RTCC_STAT1_ADDR );
  Wire.endTransmission();

  Wire.requestFrom( Rtcc_Addr, 5 );	//	request 5 bytes
  Wire.receiveWire();					//	Status 1 Register
  Wire.receiveWire();					//	Status 2 Register

  //	Get Second and Low Voltage bit from PCF RTC - bcdToDec( .. )
  rtcDate[ 6 ] = Wire.receiveWire();

  //	Get minute from PCF RTC
  rtcDate[ 5 ] = Wire.receiveWire() & 0x7F;

  //	Get hour from PCF RTC
  //	0x3F = 0b00111111
  rtcDate[ 4 ] = Wire.receiveWire() & 0x3F;

#ifdef WATCH_LOWVOLTAGE
  //	Get PCF Low Voltage
  lowVoltage = ( bool )( rtcDate[ 6 ] & RTCC_VOLTLOW_MASK );
#endif

  //	Get Second from PCF RTC - bcdToDec( .. )
  //	0x7f = 0b01111111
  rtcDate[ 6 ] = rtcDate[ 6 ] & 0x7F;

}



//////////////////////////
// Special Function		//
//////////////////////////
//	Set the square wave pin output
void setSquareWave( byte frequency )
{
  Wire.beginTransmission( Rtcc_Addr );	// Issue I2C start signal
  Wire.sendWire( RTCC_SQW_ADDR );
  Wire.sendWire( frequency );
  Wire.endTransmission();

}

//	Disable square wave pin output
void clearSquareWave()
{
  setSquareWave( SQW_DISABLE );

}

#ifdef WATCH_LOWVOLTAGE
bool getVoltageLowFlag()
{
  getTime();

  return lowVoltage;
}
#endif

void resetVoltageLowFlag( bool state )
{
  if ( state == false )
  {
    rtcDate[ 6 ] |= RTCC_VOLTLOW_MASK;
  }
  else
  {
    rtcDate[ 6 ] &= ~RTCC_VOLTLOW_MASK;
  }

}

//	Print date & time for debuging
void printDateTime()
{
#ifdef DEBUG
  getDate();
  getTime();

  Serial.print( F( "Hour: " ) );
  Serial.print( bcdToDec( rtcDate[ 4 ] ) );
  Serial.print( F( " Min: " ) );
  Serial.print( bcdToDec( rtcDate[ 5 ] ) ) ;
  Serial.print( F( " Sec: " ) );
  Serial.print( bcdToDec( rtcDate[ 6 ] ) ) ;
  Serial.println();
  Serial.print( F( " Day: " ) );
  Serial.print( rtcDate[ 2 ] );
  Serial.print( F( " Month: " ) );
  Serial.print( rtcDate[ 1 ] );
  Serial.print( F( " Year: " ) );
  if ( century == 0 )
  {
    Serial.print( F( "20" ) );
  }
  else
  {
    Serial.print( F( "19" ) );
  }
  Serial.print( rtcDate[ 0 ] );
  Serial.print( F( " Weekday: " ) );
  Serial.print( rtcDate[ 3 ] );
#endif

}

byte eepromRead( byte address )
{

  return EEPROM.read( address );
}

byte eepromWrite( byte address, byte data )
{
  EEPROM.write( address, data );

}

void eepromReadDateTime()
{
  for ( byte i = 6; i < 255; i-- )
  {
    byte Buf = eepromRead( 0x10 + i );

#ifdef DEBUG
    Serial.println( F( "Data " ) );
    Serial.print( i, DEC );
    Serial.print( F( " : BCD : " ) );
    Serial.print( Buf, DEC );
    Serial.print( F(  " - Data Decode : " ) );
    Serial.print( bcdToDec( Buf ), DEC );
    Serial.println();
#endif

  }

}

void eepromWriteDateTime( byte buffer[ 7 ]  )
{
  for ( byte i = 6; i < 255; i-- )
  {
    eepromWrite( 0x10 + i, buffer[ i ] );

  }

}

//	Buzzer Function : 'duration' parameter in milli-seconds ( min 50 ms )
#ifdef RTC_BUZZER
void enableBuzzer( int duration, byte repeat )
{
  int x = ( duration < 50 ) ? 166 : ( int )( ( duration / 300.0f ) * 1000 );

  repeat = ( repeat < 1 ) ? 1 : repeat;

  for ( byte i = 0; i < repeat; i++ )
  {
    for ( int j = 0; j < x; j++ )
    {
      digitalWrite( buzzerPin, HIGH );
      delayMicroseconds( 150 );
      digitalWrite( buzzerPin, LOW );
      delayMicroseconds( 150 );
    }

    if ( repeat >= 2 )	delay( 500 );
  }

}
#endif

void rebootNodeMCU()
{
	ESP.restart();
  
}

//	Reset Atari
void resetAtariOP()
{
	//if ( startReset == true )
	if ( ( startReset == true ) || ( powerOnReset == true ) )
	{
		
		pinMode( resetPin, OUTPUT );
		delay( 10 );
		digitalWrite( resetPin, LOW );
		delay( 350 );
		digitalWrite( resetPin, HIGH );

	#ifdef NONEMCU_LED
		debugInternalLED();
	#endif

	#ifdef RTC_BUZZER
		enableBuzzer( 250, 1 );
	#endif

	#ifdef DEBUG
		Serial.println( F( "Resetting Atari..." ) );
	#endif

		startReset = false;
		
		pinMode( resetPin, INPUT_PULLUP );
	
	}
		
	powerOnReset = false;

}

void loadConfig()
{
	String thisLine;
	
	//	Load Config
	File fileRead = SPIFFS.open( "/settings.ini", "r" );
			
	if ( !fileRead )
	{
	#ifdef DEBUG
		Serial.println( F( "settings.ini couldn't be opened! - Load default Wifi settings" ) );
	#endif
			
		//	default settings
		//	Warning : default authentification password must be at least 8 characters ( otherwise wifi server dont work properly )
		//wifiMode 	= "AccessPoint";
		wifiMode 	= "AccessStation";
					
		apSSID		= "AtariWebServer";
		apPassword	= "test12345678";
		apIP		= "192.168.43.20";

		staSSID		= "X_CROSS";
		staPassword	= "zobbilamouche";
		staIP		= "192.168.1.20";
		darkTheme	= "unchecked";
				
		fileRead.close();
		
		return;
	}
	else
	{
		char buffer[128];
		
	#ifdef DEBUG
		Serial.println( F( "settings.ini loaded - Result :" ) );
	#endif
	
		while ( fileRead.available() )
		{
			int x = fileRead.readBytesUntil( '\r\n', buffer, sizeof( buffer ) - 1 );
			
			// Remove Extra Character
			buffer[x-1] = 0;
			buffer[x] = 0;
			
			thisLine = buffer;
			//thisLine.trim();
						
		#ifdef DEBUG	
			Serial.print( F( "SETTINGS Param : " ) );
			Serial.println( thisLine );
		#endif	

			if ( thisLine.indexOf( "apSSID=" ) == 0 )		apSSID		= thisLine.substring( 7 );
			if ( thisLine.indexOf( "apPassword=" ) == 0 )	apPassword	= thisLine.substring( 11 );
			if ( thisLine.indexOf( "apIP=" ) == 0 )			apIP		= thisLine.substring( 5 );
			if ( thisLine.indexOf( "wifimode=" ) == 0 )		wifiMode	= thisLine.substring( 9 );
			if ( thisLine.indexOf( "staSSID=" ) == 0 )		staSSID		= thisLine.substring( 8 );
			if ( thisLine.indexOf( "staPassword=" ) == 0 )	staPassword	= thisLine.substring( 12 );
			if ( thisLine.indexOf( "staIP=" ) == 0 )		staIP		= thisLine.substring( 6 );
			if ( thisLine.indexOf( "darkTheme=" ) == 0 )	darkTheme	= thisLine.substring( 10 );
			
			if ( ( thisLine.length() == 0 ) || !thisLine.indexOf( "=" ) )	break;
			
		}
		
		//thisLine.trim();
	#ifdef DEBUG
		Serial.print( F( "Default DarkTheme value : " ) );
		Serial.println( darkTheme );
	#endif
			
	}			
	fileRead.close();
	
}

//	TODO - Set Atari RTC for PCF RTC
/*
void setAtariRTC()
{	
	while ( true )
	{
		if ( Ctrl.available() )
		{
			cmd = Ctrl.read();
			
			if ( cmd == 0x1C )
			{
				// Read PCF8563 RTC
				getDate();
				getTime();

				stDate[ 5 ] = ( century == 1 ) ? decToBcd( rtcDate[ 0 ] ) :
												 decToBcd( rtcDate[ 0 ] ) + 0xA0;	// YY
				stDate[ 4 ] = decToBcd( rtcDate[ 1 ] & 0x1F );						// MM
				stDate[ 3 ] = decToBcd( rtcDate[ 2 ] );								// DD
				stDate[ 2 ] = rtcDate[ 4 ] & 0x3F;									// hh
				stDate[ 1 ] = rtcDate[ 5 ];											// mm
				stDate[ 0 ] = rtcDate[ 6 ];											// ss
				
				setLED( true );
				
				for ( byte i = 6; i >= 0; --i )
				{
					Serial.read();
					Serial.write( stDate[ i ] );
					
				}
				
				break;
			}		
		}
		
	}

}
*/


//////////////////////////
// 		SETUP			//
//////////////////////////
void setup()
{
	byte Error				= 0;
	byte wifiConnectCount	= 0;
		
	startReset		= false;
	powerOnReset	= true; 


//	Pins Settings	
#ifndef DEBUG
	//	Atari TOS State Pin
	pinMode( atariTOSStatePin, INPUT );
	  
	//	Select TOS Pin - Must first on Setup for select proper TOS
	byte stateTOS = getAtariTOSState();
	delay( 2 );
	pinMode( selTOSPin, OUTPUT );
	setSelectTOS( stateTOS );
#endif


#ifdef NONEMCU_LED
	pinMode( ledPin, OUTPUT );
	digitalWrite( ledPin, HIGH );
#else
	pinMode( ledPin, INPUT );
#endif


	//	join I2C Bus
	Wire.begin( i2cSDAPin, i2cSCLPin );
	
	//  Test if RTC Breakout work ( PCF8563 or I2C work properly
	if ( testPCF8563() == true )
	{
	#ifdef DEBUG
		Serial.println();
		Serial.println( F( "PCF8563 & I2C OK !" ) );
	#endif
	 
	#ifdef RTC_LED
		setSquareWave( SQW_1HZ );
	#else	
		clearSquareWave();
	#endif

		//	Gey RTC date & verify if RTC is in factory mode hardcoded ( 00/00/2000 )
		getTime();
		getDate();

		if ( rtcDate[ 0 ] == 0 && rtcDate[ 1 ] == 0 && rtcDate[ 2 ] == 0 )			//	&& century == 0 ) ???? Test century not necessary
		{
		  setDate( 26, 2, 8, 1, 90 );
		  setTime( 12, 00, 00 );
		}

	}
	else
	{
		Error++;
	#ifdef DEBUG
		Serial.println();
		Serial.println( F( "PCF8563 & I2C FAULT !" ) );
	#endif
	}
	

#ifdef DEBUG
	Serial.begin( 115200 );

	while ( !Serial )
	{
		;;
	}
	
	Serial.print( "Atari RTC/TOS Module Debug" );
	Serial.println();
#else
	Serial.begin( 7812 );
	delay( 150 );
	//	Swap to Serial 2 pins ( D7->RX2, D8->TX2 )
	Serial.swap();
	
	delay( 10 );
	
	//	Reset Atari Pin
	pinMode( resetPin, INPUT_PULLUP );
	///digitalWrite( resetPin, LOW );
#endif

	Ctrl.begin( 7812 );
	Ctrl.listen();
	
	/////setAtariRTC();


	// Initialize SPIFFS
	if ( !SPIFFS.begin() )
	{
		Error++;
	#ifdef DEBUG
		Serial.println( F( "An Error has occurred while mounting SPIFFS" ) );
	#endif
	}
	else
	{
	#ifdef DEBUG
		Serial.println( F( "Mounting SPIFFS OK" ) );
	#endif
	}


#ifdef RTC_BUZZER
	pinMode( buzzerPin, OUTPUT );
	digitalWrite( buzzerPin, LOW );
#else
	pinMode( buzzerPin, INPUT );
#endif
	
	//	Set Static IP for WebServer
	IPAddress ip;
	IPAddress gateway;
	
	loadConfig();
		
	networkCount = WiFi.scanNetworks();
			
#ifdef DEBUG
	Serial.print( F( "Scan Wifi Network : " ) );
	Serial.println( networkCount, DEC );
#endif
				
	if ( networkCount == 0 )
	{
		networkSSID = "";
	#ifdef DEBUG	
		Serial.print( F( "No networks found" ) );
	#endif
	}
	else
	{
		for ( byte i = 0; i < networkCount; ++i )
		{
		#ifdef DEBUG	
			Serial.print( F( "SSID Name : " ) );
			Serial.println( WiFi.SSID( i ) );
			Serial.print( F( "Power : " ) );
			Serial.print( WiFi.RSSI( i ) );
			Serial.println( F( " dbm" ) );
		#endif	
			
			if ( WiFi.RSSI( i ) >= -70 )	networkSSID += WiFi.SSID( i ) + ",";
			//if ( WiFi.encryptionType( i ) == ENC_TYPE_NONE )
		
		}
		
		networkSSID.remove( networkSSID.length() );
	#ifdef DEBUG	
		Serial.print( F( "Network SSID Name : " ) );
		Serial.println( networkSSID );
	#endif	
	}

	// Connect to Wi-Fi Mode
	#ifdef DEBUG	
		Serial.print( F( "Select nodeMCU as : " ) );
	#endif
	if ( wifiMode == "AccessPoint" )
	{
	#ifdef DEBUG	
		Serial.println( F( "Access Point" ) );
	#endif
		WiFi.mode( WIFI_AP );
		
		ip		= IPAddress( 192, 168, 43, 20 );
		gateway	= IPAddress( 192, 168, 43, 1 );
		
		IPAddress subnet( 255, 255, 255, 0 );
		
		if ( WiFi.softAPConfig( ip, gateway, subnet ) == false )
		{
		#ifdef DEBUG
			Serial.println( F( "Couldn't create AP" ) );
		#endif
		
			Error++;
		}
		else
		{
		#ifdef DEBUG
			Serial.print( F( "Creating AP : " ) );
			Serial.println( apSSID );
		#endif
		}

		while( WiFi.softAP( apSSID.c_str(), apPassword.c_str(), 11, false, 4 ) == false )
		{
			delay( 500 );
		#ifdef DEBUG
			Serial.print( F( "." ) );
		#endif  
		}
		
		if ( WiFi.softAPIP() == IPAddress( 0, 0, 0, 0 ) )
		{	
			//revertToAP = true;
		}	
		else
		{
		#ifdef DEBUG
			Serial.println();
			Serial.print( F( "Connected! - IP address : " ) );
			Serial.println( WiFi.softAPIP() );
		#endif
		}

	}
	else if ( wifiMode == "AccessStation" )
	{
	#ifdef DEBUG	
		Serial.println( F( "Station Access" ) );
		Serial.print( F( "Connecting to : " ) );
		Serial.println( staSSID.c_str() );
    #endif
      
		WiFi.mode( WIFI_STA );
		
		ip		= IPAddress( 192, 168, 1, 20 );
		gateway	= IPAddress( 192, 168, 1, 1 );
		
		IPAddress subnet( 255, 255, 255, 0 );
		
		WiFi.config( ip, gateway, subnet );
		
		WiFi.begin( staSSID.c_str(), staPassword.c_str() );

		while ( WiFi.status() != WL_CONNECTED )
		{
			delay( 500 );
		#ifdef DEBUG
			Serial.print( F( " # " ) );
		#endif

			if ( wifiConnectCount == 10 )
			{
				
			#ifdef RTC_BUZZER
				enableBuzzer( 250, 2 );
			#endif
				
				break;
			}
			
			wifiConnectCount++;
		}	
		
		if ( WiFi.localIP() == IPAddress( 0, 0, 0, 0 ) )
		{
			//revertToAP = true;
		}	
		else
		{
		#ifdef DEBUG
			Serial.println();
			Serial.print( F( "Connected! - IP address : " ) );
			Serial.println( WiFi.localIP() );
		#endif
		
		#ifdef DEBUG
			//ftpSrv.begin( "ftpNodeMCU", "12345" );
			ftpSrv.begin( userIdentity, userPassword );
			Serial.println( F( "Start FTP OK" ) );
		#endif

		}

    }
	else
	{
	#ifdef DEBUG
		Serial.println();
		Serial.print( F( "No Wifi Activated" ) );
	#endif
	}

	delay( 50 );

  
#ifdef DEBUG
	Serial.print( F( "\nRTC Module & Wifi Connected : " ) );
#endif
	if ( Error == 0 )
	{
	#ifdef DEBUG
		Serial.println( F( "OK\n" ) );
	#endif
	
	#ifdef RTC_BUZZER
		enableBuzzer( 250, 1 );
	#endif
	}
	else
	{
	#ifdef DEBUG
		Serial.print( F( "Failed\n" ) );
	#endif
	
	#ifdef RTC_BUZZER
	  enableBuzzer( 250, 3 );
	#endif
	}
	

	// Route for root / web page
	server.on( "/", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		request->send( SPIFFS, "/index.html", String(), false, processor );

	} );
	
	// Route to load style.css file
	server.on( "/atari_icon.png", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		request->send( SPIFFS, "/atari_icon.png", "icon/png" );

	} );
	
	// Route to load style.css file
	server.on( "/hide_eye.png", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		request->send( SPIFFS, "/hide_eye.png", "icon/png" );

	} );
	
	// Route to load style.css file
	server.on( "/eye.png", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		request->send( SPIFFS, "/eye.png", "icon/png" );

	} );

	// Route to load style.css file
	server.on( "/style.css", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		request->send( SPIFFS, "/style.css", "text/css" );

	} );
	  
	// Route to load style.css file
	server.on( "/function.js", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		request->send( SPIFFS, "/function.js", "text/javascript" );

	} );


	// Route for select TOS
	server.on( "/selTOS", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		// TOS select State Value
		byte selectTOSState = LOW;
	
		byte paramsNr = request->params();

	#ifdef DEBUG
		Serial.print( F( "\nParam request count : " ) );
		Serial.print( paramsNr );
	#endif

		for ( int i = 0; i < paramsNr; i++ )
		{
			AsyncWebParameter* p = request->getParam( i );

			String requestName	= p->name();
			String requestValue	= p->value();

		#ifdef DEBUG
			Serial.print( F( "Param name: " ) );
			Serial.println( p->name() );
			Serial.print( F( "Param value: " ) );
			Serial.println( p->value() );
			Serial.println( F( "------" ) );
		#endif

			if ( requestName.equals( "version" ) == true )
			{
				if ( ( requestValue.equals( "1" ) == true ) && ( getAtariTOSState() == LOW ) )
				{
					selectTOSState = HIGH;
				}
				else if ( ( requestValue.equals( "0" ) == true ) && ( getAtariTOSState() == HIGH ) )
				{
					selectTOSState = LOW;
				}
				else
				{
					selectTOSState = LOW;
				}
			}

			// Select LOW or HIGH TOS
			setSelectTOS( selectTOSState );

			//eepromWrite( 0x10, selectTOSState );

			if ( requestName.equals( "setReset" ) == true )
			{
				if ( requestValue.equals( "1" ) == true )
				{
					startReset = true;
				}
			}

		}

		String sPage = FPSTR( HTTP_ROOT_LABEL );
		sPage += F( "Root Redirect" );
		sPage += FPSTR( HTTP_END_LABEL );
		request->send( 200, "text/html", sPage.c_str() );

	  } );

  // Route for Set Atari Date and Time RTC
  server.on( "/setDateTime", HTTP_GET, []( AsyncWebServerRequest * request )
  {
    byte hundred;
    byte centuryBIT;
    byte decade;
    byte month;
    byte day;

    byte hour;
    byte minute;

    byte paramsNr = request->params();

#ifdef DEBUG
    Serial.print( F( "\nParam request count : " ) );
    Serial.print( paramsNr );
#endif

    for ( int i = 0; i < paramsNr; i++ )
    {
      AsyncWebParameter* p = request->getParam( i );

      String requestName	= p->name();
      String requestValue	= p->value();

#ifdef DEBUG
      Serial.print( F( "Param name: " ) );
      Serial.println( p->name() );
      Serial.print( F( "Param value: " ) );
      Serial.println( p->value() );
      Serial.println( F( "------" ) );
#endif

      if ( requestName.equals( "valueDate" ) == true )
      {
        String year_str	= getSplitParamValue( requestValue, '-', 0 );

        hundred		= removeDigit( year_str.substring( 0, 2 ) );
        centuryBIT	= ( hundred == 19 ) ? 1 : 0;
        decade		= removeDigit( year_str.substring( 2 ) );
        month		= removeDigit( getSplitParamValue( requestValue, '-', 1 ) );
        day			= removeDigit( getSplitParamValue( requestValue, '-', 2 ) );

#ifdef DEBUG
        Serial.print( F( "Date Request : - Day : " ) );
        Serial.print( day, DEC );
        Serial.print( F( " month : " ) );
        Serial.print( month, DEC );
        Serial.print( F( " Year : " ) );
        Serial.print( year_str.toInt(), DEC );
        Serial.print( F( " - century : " ) );
        Serial.print( hundred, DEC );
        Serial.print( F( " decade : " ) );
        Serial.print( decade, DEC );
#endif
      }

      if ( requestName.equals( "valueTime" ) == true )
      {
        hour	= removeDigit( getSplitParamValue( requestValue, ':', 0 ) );
        minute	= removeDigit( getSplitParamValue( requestValue, ':', 1 ) );

#ifdef DEBUG
        Serial.print( F( "Time Request : " ) );
        Serial.print( hour, DEC );
        Serial.print( F( "H" ) );
        Serial.print( minute, DEC );
#endif
      }

      if ( requestName.equals( "setReset" ) == true )
      {
        if ( requestValue.equals( "1" ) == true )
        {
          startReset = true;

        }

      }

    }

    setDate( day, 1, month, centuryBIT, decade );
    setTime( hour, minute, 30 );

#ifdef RTC_BUZZER
    enableBuzzer( 250, 1 );
#endif

	String sPage = FPSTR( HTTP_ROOT_LABEL );
	sPage += F( "Root Redirect" );
	sPage += FPSTR( HTTP_END_LABEL );
	request->send( 200, "text/html", sPage.c_str() );

  } );
  

	// Route to set GPIO to HIGH
	server.on( "/forceReset", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		//request->send( 200, "text/html", strHTML );
		//request->send( SPIFFS, "/index.html", String(), false, processor );
		//request->send( SPIFFS, "/index.html", "text/html" );

		String sPage = FPSTR( HTTP_ROOT_LABEL );
		sPage += F( "Root Redirect" );
		sPage += FPSTR( HTTP_END_LABEL );
		request->send( 200, "text/html", sPage.c_str() );
		
		//request->send( 200, "text/html", HTTP_ROOT_LABEL );

		startReset = true;

	} );
	

	server.on( "/getAtariTOSVersion", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		request->send_P( 200, "text/plain", getAtariTOSVersion().c_str() );

	} );
	

	// Route for Get Atari Date and Time RTC
	server.on( "/getAtariRTC", HTTP_GET, []( AsyncWebServerRequest * request )
	{
	#ifdef DEBUG
		Serial.print( F( "Get Atari Date and Time RTC..." ) );
	#endif

		request->send_P( 200, "text/plain", getAtariRTC().c_str() );		
		
		String sPage = FPSTR( HTTP_ROOT_LABEL );
		sPage += F( "Root Redirect" );
		sPage += FPSTR( HTTP_END_LABEL );
		request->send( 200, "text/html", sPage.c_str() );

	} );


	server.on( "/settings", HTTP_GET, []( AsyncWebServerRequest * request )
	{
		//String sPage = FPSTR( WM_HTTP_HEAD_START );
		String sPage;
		
		String data_word = networkSSID;
		
		byte paramsNr = request->params();
				
		#ifdef DEBUG
			Serial.print( F( "\nParam request count : " ) );
			Serial.println( paramsNr );
		#endif
		
		if ( paramsNr > 0 )
		{			
			File file = SPIFFS.open( "/settings.ini", "w" );					
			
			for ( int i = 0; i < paramsNr; i++ )
			{
				AsyncWebParameter* p = request->getParam( i );

				String requestName	= p->name();
				String requestValue	= p->value();

			#ifdef DEBUG
				Serial.print( F( "Param name: " ) );
				Serial.println( p->name() );
				Serial.print( F( "Param value: " ) );
				Serial.println( p->value() );
				Serial.println( F( "------" ) );
			#endif
			
				String settingsLine = p->name() + "=" + p->value();
				
				file.println( settingsLine.c_str() );
			}
			
			file.close();
			
			#ifdef DEBUG
				Serial.println( F( "settings.ini Saved" ) );
			#endif

			loadConfig();

			sPage = FPSTR( HTTP_ROOT_LABEL );
			sPage += F( "Settings saved. Reset the card if network settings were changed!" );
			sPage += FPSTR( HTTP_END_LABEL );
			request->send( 200, "text/html", sPage.c_str() );
			
		}
		else
		{
			loadConfig();
			
			//	Load HTTP Page
			sPage  = FPSTR( HTTP_HEAD_LABEL );
			
			sPage += F( "<FORM ID=\"form1\">\n<P>Wifi Mode</P>\n<DIV STYLE=\"margin-left: 1em;\">\n<INPUT TYPE =\"radio\" ID=\"ap\" NAME=\"wifimode\" VALUE=\"AccessPoint\"" );
			
			if ( wifiMode == "AccessPoint" ) sPage +=  F( " checked" );
			
			sPage += F( ">\n<LABEL FOR=\"ap\">Access Point</LABEL>\n<BR></BR>\n" );
			sPage += F( "<DIV STYLE=\"margin-left: 2em;\">\nName: <INPUT ID=\"apSSID\" TYPE=\"text\" NAME=\"apSSID\" VALUE=\"" );
			sPage += apSSID;
			sPage += F( "\" onkeyup=\"apSSIDverify()\">\n<LABEL ID=\"apSSIDlabel\"></LABEL>\n<BR></BR>\n" );
			sPage += F( "Password: <INPUT TYPE=\"password\" ID=\"apPassword\" NAME=\"apPassword\" VALUE=\"" );
			sPage += apPassword;
			sPage += F( "\" onkeyup=\"apPasswordVerify('\apPasswordlabel'\)\">\n<LABEL ID=\"apPasswordlabel\"></LABEL><input type=\"checkbox\" name=\"apPasswordCheckbox\" id=\"apPasswordCheckbox\" onclick=\"showPassword(\'apPassword\')\"><label for=\"apPasswordCheckbox\" class=\"apPasswordCheckbox\"></label>\n<BR></BR>\n" );
			
			sPage += F( "\nStatic IP: <INPUT TYPE=\"text\" required pattern=\"^([0-9]{1,3}\.){3}[0-9]{1,3}$\" ID=\"apIP\" NAME=\"apIP\" VALUE=\"" );
			sPage += apIP + F( "\" onkeypress=\"return isNumber(event)\" onkeyup=\"apIPverify()\">\n<BR></BR></DIV>" );
			
			sPage += F( "<DIV><INPUT TYPE=\"radio\" ID=\"sta\" NAME=\"wifimode\" VALUE=\"AccessStation\"" );
			
			if ( wifiMode == "AccessStation" ) sPage += F( " checked" );
			
			sPage += F( ">\n<LABEL FOR=\"sta\">Access Station</LABEL>\n<BR></BR>\n</DIV>\n<DIV STYLE=\"margin-left: 2em;\">\n" );
		
		#ifdef DEBUG
			Serial.print( F( "Scan Wifi Network : " ) );
			Serial.println( networkCount, DEC );
		#endif

			if ( networkCount == 0 && wifiMode != "AccessStation" )
			{
				sPage += F( "Access Point Select.\n<BR></BR>" );
			}
			else if ( networkCount == 0 && wifiMode == "AccessStation" )
			{
				sPage += F( "No networks found.\n<BR></BR>" );
			}
			else
			{
				// Extract Wifi SSID Name Network & Enumerate
				byte ssidCount	= 0;
				byte index		= 0;
				byte next_index;
				do
				{
					next_index = networkSSID.indexOf( ',', index );
					
					data_word = networkSSID.substring( index, next_index );
					
				#ifdef DEBUG		
					Serial.print( F( "SSID Name : " ) );
					Serial.println( data_word );
				#endif
				
					sPage += "<INPUT TYPE=\"radio\" ID=\"station" + String( ssidCount ) + "\" NAME=\"staSSID\" VALUE=\"" + data_word + "\" ";
					
					if ( data_word == staSSID ) sPage += " checked";
					
					sPage += ">\n<LABEL FOR=\"station" + String( ssidCount, DEC ) + "\">" + data_word;
					
					//if ( WiFi.encryptionType(i) == ENC_TYPE_NONE ) sPage += " (open)";
					sPage += "</LABEL>\n<BR></BR>\n";
					
					index = next_index + 1;
				}
				while( ( next_index != -1 ) && ( next_index < networkSSID.length()-1 ) );

				sPage += F( "</DIV><DIV STYLE=\"margin-left: 2em;\">\nPassword: <INPUT TYPE=\"password\" ID=\"staPassword\" NAME=\"staPassword\" VALUE=\"" );
				sPage += staPassword;
				sPage += F( "\" onkeyup=\"apPasswordVerify(\'staPasswordlabel\')\">\n<LABEL ID=\"staPasswordlabel\"></LABEL><input type=\"checkbox\" name=\"staPasswordCheckbox\" id=\"staPasswordCheckbox\" onclick=\"showPassword(\'staPassword\')\"><label for=\"staPasswordCheckbox\" class=\"staPasswordCheckbox\"></label>\n<BR></BR>\n" );
				
				sPage += F( "Static IP: <INPUT TYPE=\"text\" required pattern=\"^([0-9]{1,3}\.){3}[0-9]{1,3}$\" ID=\"staIP\" NAME=\"staIP\" VALUE=\"" );
				sPage += staIP + F( "\" onkeypress=\"return isNumber(event)\" onkeyup=\"staIPverify()\">\n<LABEL ID=\"staIPlabel\"></LABEL>\n<BR>\n</DIV>\n" );		
			
			}
			
			sPage += F( "<div><p><hr></hr><h1 style=\"font-size:18px;\">Dark Theme Switch<label class=\"switch\"><input type=\"checkbox\" id=\"switchDarkThemeCtrl\" name=\"darkTheme\" value=\"" );
			sPage += darkTheme + F( "\" " ) + darkTheme; 
			sPage += F( " onclick=\"getDarkTheme_2()\"><span class=\"slider round\"></span></label></p></div><hr></hr>\n</FORM>\n" );

			
			
			sPage += F( "\n<script>setDarkTheme_2(\'" ) + darkTheme + F( "\');</script>\n" );
			
			sPage += F( "<p><DIV STYLE=\"margin-left: 5px;\"><BUTTON class=\"button button2\" TYPE=\"submit\" ID=\"submitbtn\" FORM=\"form1\" VALUE=\"Submit\"> Save </BUTTON>\n</div></p>" );	

			sPage += FPSTR( HTTP_END_LABEL );
			
			request->send_P( 200, "text/html", sPage.c_str() );
		}	
		
	} );


	// Start HTTP Server
#ifdef DEBUG
	Serial.println( F( "Starting ESP8266 Web Server..." ) );
	Serial.println( F( "ESP8266 Web Server Started" ) );
#endif
	server.begin();

	
/*
#ifdef WATCH_LOWVOLTAGE

  //	Set date & time to default if none set
#ifndef DEBUG
  if ( getVoltageLowFlag() )
  {
    setDate( 01, 2, 01, 1, 89 );
    setTime( 12, 00, 00 );
  }
#endif

#endif


#ifdef DEBUG
  printDateTime();
#endif
*/


}


void loop()
{
	
	//	Set TOS version from 	
	unsigned long currentMillis = millis();
	//	start TIMER RYTHM - Tick every 2s
	if ( currentMillis - previousMillis >= 5000 )
	{
		previousMillis = currentMillis; 
		
		//	Select TOS Pin - Must first on Setup for select proper TOS
		byte stateTOS = getAtariTOSState();
		delay( 2 );
		setSelectTOS( stateTOS );
	
	}

	//	Get RTC Command from Atari
	if ( Ctrl.available() )
	{
		cmd = Ctrl.read();

		if ( cmd == 0x1C )
		{

			//rtc.setDate( 19, 2, 8, 0, 21 );
			//rtc.setTime( 18, 15, 10 );

			// Read PCF8563 RTC
			getDate();
			getTime();

			stDate[ 5 ] = ( century == 1 ) ? decToBcd( rtcDate[ 0 ] ) :
											 decToBcd( rtcDate[ 0 ] ) + 0xA0;	// YY
			stDate[ 4 ] = decToBcd( rtcDate[ 1 ] & 0x1F );						// MM
			stDate[ 3 ] = decToBcd( rtcDate[ 2 ] );								// DD
			stDate[ 2 ] = rtcDate[ 4 ] & 0x3F;									// hh
			stDate[ 1 ] = rtcDate[ 5 ];											// mm
			stDate[ 0 ] = rtcDate[ 6 ];											// ss
			
			//	See Serial Event : Inject Date & Time Data from PCF RTC to Atari ST RTC
			inj = 6;

		}
		else if ( cmd == 0x1B )
		{
			//	Write to PCF8563 RTC

		#ifdef NONEMCU_LED
			digitalWrite( ledPin, LOW );
		#endif

			//	Read Atari ST RTC
			for ( byte i = 5; i < 255; i-- )
			{
				while ( !Ctrl.available() )
				{
					;;
				};

				stDate[ i ] = Ctrl.read();

			}

			//eepromWrite( stDate );

			rtcDate[ 0 ] = bcdToDec( ( stDate[ 5 ] < 0xA0 ) ? stDate[ 5 ] :
									 ( stDate[ 5 ] - 0xA0 ) );		// YY : if stDate > 99 => rtcDate = stDate - 100
			rtcDate[ 1 ] = bcdToDec( stDate[ 4 ] );					// MM
			rtcDate[ 2 ] = bcdToDec( stDate[ 3 ] );					// DD
			rtcDate[ 3 ] = 1;										// WeekDay - Don't Care
			rtcDate[ 4 ] = bcdToDec( stDate[ 2 ] );        			// hh
			rtcDate[ 5 ] = bcdToDec( stDate[ 1 ] );					// mm
			rtcDate[ 6 ] = bcdToDec( stDate[ 0 ] );					// ss

			setDate( rtcDate[ 2 ], rtcDate[ 3 ], rtcDate[ 1 ], ( stDate[ 5 ] < 0xA0 ) ? 1 : 0, rtcDate[ 0 ] );	// day, weekday, month, century, year
			setTime( rtcDate[ 4 ], rtcDate[ 5 ], rtcDate[ 6 ] );												// hh, mm, ss

			//eepromWrite( 0x20, true );

		#ifdef WATCH_LOWVOLTAGE
			  lowVoltage = true;
		#endif

		#ifdef NONEMCU_LED
			  digitalWrite( ledPin, HIGH );
		#endif
		}

	}
	
  
	//	Reset Atari function here to avoid arduino delay method issue with ESPAsyncWebServer
	//	See this note for delay here : https://github.com/me-no-dev/ESPAsyncWebServer#important-things-to-remember
	resetAtariOP();
	
	
	//	FTP Handling
	#ifdef DEBUG
		ftpSrv.handleFTP();
	#endif
	
	
	/*
	#if defined WATCH_LOWVOLTAGE
	unsigned long currentMillis = millis();

	if ( ( currentMillis - previousMillis >= interval ) )
	{
		previousMillis = currentMillis;

		if ( getVoltageLowFlag() )
		{
			eepromWrite( 0x20, true );
			lowVoltage = true;
		}
		else
			lowVoltage = false;
	}
	#endif
	*/
	

}



//////////////////////////
// 		Serial Event	//
//////////////////////////

//	- Receive and Send Data From/To Keyboard
//	- Send Clock data to Atari RTC
void serialEvent()
{
	if ( inj == 255 )
	{
	#ifdef NONEMCU_LED
		digitalWrite( ledPin, LOW );
	#endif
		Serial.write( Serial.read() );

	#ifdef NONEMCU_LED
		digitalWrite( ledPin, HIGH );
	#endif

	}
	else
	{

	#ifdef NONEMCU_LED
		digitalWrite( ledPin, LOW );
	#endif

		Serial.read();
		Serial.write( stDate[ inj-- ] );

		if ( inj == 0 )		inj = 255;

	#ifdef NONEMCU_LED
		digitalWrite( ledPin, HIGH );
	#endif
	
		//resetAtariOP();

	}

}
