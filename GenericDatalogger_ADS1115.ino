// 12/9/2014 - Generic 4-channel Voltage Datalogger 16-bit v0.02

// Maximum voltage reading with v0.02 of the device is upto 3.3V only, which is the voltage between VCC and GND of ADS1115.
// Do NOT supply greater than 3.3V to the channels or it may damage the ADC.
// Use a 5V Pro Mini to get more range or create a voltage divider circuit.


#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <PowerSaver.h>
#include <DS3234lib3.h>
#include <SdFat.h>

PowerSaver chip;    // declare object for PowerSaver class
Adafruit_ADS1115 ads;    // declare object for Adafruit_ADS1115 class

// Main code stuff   ******************************
#define POWA 14    // pin 14 (A0) supplies power to microSD card breakout and 16-bit ADC
int SDcsPin = 9;
long interval = 10;  // interval in seconds (value automatically assigned by the GUI)

// RTC stuff   ******************************
DS3234 RTC;    // declare object for DS3234 class
int dayStart = 0, hourStart = 0, minStart = 0;    // start time: day of the month, hour, minute
                                                  // values automatically assigned by the GUI
 
// SD card stuff   ******************************
#define LED 4  // pin 7 controls LED
SdFat sd;
SdFile file;
char filename[] = "log.csv";    // file name should be of the format "12345678.123". Cannot be more than 8 characters in length
 
// Stuff stuff ****************************************************************
ISR(PCINT0_vect)  // setup interrupts on digital pin 8
{
  PORTB ^= (1<<PORTB1);
}

// setup ****************************************************************
void setup()
{
  Serial.begin(19200);
  pinMode(POWA, OUTPUT);
  digitalWrite(POWA, HIGH);    // turn on SD card and ADC
  pinMode(LED, OUTPUT);
  
  ads.begin();
  //ads.setGain(GAIN_TWO);  // set gain here
  
  RTC.fetchAndSetTime();  // syncs date and time with the PC's clock
  RTC.getLaunchParameters(interval, dayStart, hourStart, minStart); // get parameters from GUI
  readFileName();  // read filename from EEPROM (assigned by GUI)
  delay(500);    // give some delay to ensure the RTC gets proper date/time
  
  if(!sd.init(SPI_FULL_SPEED, SDcsPin))  // initialize SD card on the SPI bus
  {
    delay(100);
    SDcardError();
  }
  else
  {
    delay(50);
    file.open(filename, O_CREAT | O_APPEND | O_WRITE);  // open file in write mode and append data to the end of file
    delay(1);
    String time = RTC.timeStamp();    // get date and time from RTC
    file.println();
    file.print("Date/Time,Channel 0,Channel 1,Channel 2,Channel 3");    // Print header to file
    file.println();
    PrintFileTimeStamp();
    file.close();    // close file - very important
                     // give some delay by blinking status LED to wait for the file to properly close
    digitalWrite(LED, HIGH);
    delay(10);
    digitalWrite(LED, LOW);
    delay(10);
  }
  
  RTC.checkInterval(hourStart, minStart, interval); // Check if the logging interval is in secs, mins or hours
  RTC.alarm2set(dayStart, hourStart, minStart);  // Configure begin time
  RTC.alarmFlagClear();  // clear alarm flag
  chip.sleepInterruptSetup();    // setup sleep function on the ATmega328p. Power-down mode is used here
}

// loop ****************************************************************
void loop()
{
  digitalWrite(POWA, LOW);  // turn off microSD card to save power
  delay(5);  // give some delay for SD card power to be low before processor sleeps to avoid it being stuck
  
  sleeptime();
  
  chip.turnOnADC();    // enable ADC after processor wakes up
  chip.turnOnSPI();   // turn on SPI bus once the processor wakes up
  delay(1);    // important delay to ensure SPI bus is properly activated
  RTC.alarmFlagClear();    // clear alarm flag
  pinMode(POWA, OUTPUT);
  digitalWrite(POWA, HIGH);  // turn on SD card power
  delay(50);    // give delay to let the SD card get full powa
  pinMode(SDcsPin, OUTPUT);
  if(!sd.init(SPI_FULL_SPEED, SDcsPin))    // very important - reinitialize SD card on the SPI bus
  {
    delay(100);
    SDcardError();
  }
  else
  {
    delay(50);
    file.open(filename, O_WRITE | O_AT_END);  // open file in write mode
    delay(1);
    String time = RTC.timeStamp();    // get date and time from RTC
    SPCR = 0;
    
    // get ADC values
    float adc0 = samples(0);
    float adc1 = samples(1);
    float adc2 = samples(2);
    float adc3 = samples(3);
    
    // get voltage
    float V0 = voltage(adc0, 0);  // set gain here based on setting above. Options: 0(default), 1, 2, 4, 8, 16
    float V1 = voltage(adc1, 0);
    float V2 = voltage(adc2, 0);
    float V3 = voltage(adc3, 0);
    
    file.print(time);
    file.print(",");
    file.print(V0, 5);
    file.print(",");
    file.print(V1, 5);
    file.print(",");
    file.print(V2, 5);
    file.print(",");
    file.print(V3, 5);
    file.print(",");
    file.print(adc0);
    file.print(",");
    file.print(adc1);
    file.print(",");
    file.print(adc2);
    file.print(",");
    file.print(adc3);
    file.println();
    PrintFileTimeStamp();
    file.close();    // close file - very important
                     // give some delay by blinking status LED to wait for the file to properly close
    digitalWrite(LED, HIGH);
    delay(10);
    digitalWrite(LED, LOW);
    delay(10);  
  }
  
  RTC.setNextAlarm();      //set next alarm before sleeping
  delay(1);
}

// Perform multiple iterations to get higher accuracy ADC values (reduce noise) ******************************************
float samples(int pin)
{
  float n=5.0;  // number of iterations to perform
  float sum=0.0;  //store sum as a 32-bit number
  for(int i=0;i<n;i++)
  {
    float value = ads.readADC_SingleEnded(pin);
    sum = sum + value;
    delay(10);
  }
  float average = sum/n;   //store average as a 32-bit number with decimal accuracy
  return average;
}

// Get voltage ****************************************************************
float voltage(float adc, int gain)
{
  float V;
  switch(gain)
  {
    case 0:  // default 2/3x gain setting for +/- 6.144 V
      V = adc * 0.0001875;
      break;
    case 1:  // 1x gain setting for +/- 4.096 V
      V = adc * 0.000125;
      break;
    case 2:  // 2x gain setting for +/- 2.048 V
      V = adc * 0.0000625;
      break;
    case 4:  // 4x gain setting for +/- 1.024 V
      V = adc * 0.00003125;
      break;
    case 8:  // 8x gain setting for +/- 0.512 V
      V = adc * 0.000015625;
      break;
    case 16:  // 16x gain setting for +/- 0.256 V
      V = adc * 0.0000078125;
      break;
      
    default:
      V = 0.0;
  }
  return V;
}

// file timestamps
void PrintFileTimeStamp() // Print timestamps to data file. Format: year, month, day, hour, min, sec
{ 
  file.timestamp(T_WRITE, RTC.year, RTC.month, RTC.day, RTC.hour, RTC.minute, RTC.second);    // edit date modified
  file.timestamp(T_ACCESS, RTC.year, RTC.month, RTC.day, RTC.hour, RTC.minute, RTC.second);    // edit date accessed
}

// Read file name ****************************************************************
void readFileName()  // get the file name stored in EEPROM (set by GUI)
{
  for(int i = 0; i < 12; i++)
  {
    filename[i] = EEPROM.read(0x06 + i);
  }
}

// SD card Error response ****************************************************************
  
void SDcardError()
{
    for(int i=0;i<3;i++)   // blink LED 3 times to indicate SD card write error
    {
      digitalWrite(LED, HIGH);
      delay(50);
      digitalWrite(LED, LOW);
      delay(150);
    }
}
 
//****************************************************************

void sleeptime()
{
    chip.turnOffADC();
    pinMode(A0, OUTPUT); // this is set for this particular version of the datalogger only
                         // because the POWER pin is A0 and it is configured as INPUT after
                         // turning ADC off which results in some power draw by the microSD card during sleep mode
    chip.turnOffWDT();
    chip.turnOffSPI();
    chip.turnOffBOD();
    chip.goodNight();
}

//****************************************************************

