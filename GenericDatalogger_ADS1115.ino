// 3/30/2014 - Generic 4-channel Voltage Datalogger 16-bit v0.02

// If you're using a 3.3V Pro Mini, the maximum voltage reading with v0.02 of the logger is upto 3.3V only,
// which is the voltage between VCC and GND of ADS1115.Do NOT supply greater than 3.3V to the channels
// or it may damage the ADC.

// Use a 5V Pro Mini to get more range or create a voltage divider circuit
// microSD card still needs 3.3V - use a voltage regulator


#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <PowerSaver.h>
#include <DS3234lib3.h>
#include <SdFat.h>

// Launch Variables   ******************************
long interval = 60;  // set logging interval in SECONDS, eg: set 300 seconds for an interval of 5 mins
int dayStart = 12, hourStart = 14, minStart = 30;    // define logger start time: day of the month, hour, minute
char filename[15] = "log.csv";    // Set filename Format: "12345678.123". Cannot be more than 8 characters in length, contain spaces or begin with a number

// Global objects and variables   ******************************
#define POWA 14    // pin 14 (A0) supplies power to microSD card breakout and 16-bit ADC
#define LED 4  // pin 4 controls LED
int SDcsPin = 9; // pin 9 is CS pin for MicroSD breakout
int SHT_clockPin = 3;  // pin used for SCK on SHT15 breakout
int SHT_dataPin = 5;  // pin used for DATA on SHT15 breakout

PowerSaver chip;  	// declare object for PowerSaver class
DS3234 RTC;    // declare object for DS3234 class
SHT15 sensor(SHT_clockPin, SHT_dataPin);  // declare object for SHT15 class
SdFat sd; 		// declare object for SdFat class
SdFile file;		// declare object for SdFile class

// ISR ****************************************************************
ISR(PCINT0_vect)  // Interrupt Vector Routine to be executed when pin 8 receives an interrupt.
{
  //PORTB ^= (1<<PORTB1);
  asm("nop");
}

// setup ****************************************************************
void setup()
{
  Serial.begin(19200); // open serial at 19200 bps
  
  pinMode(POWA, OUTPUT);
  pinMode(LED, OUTPUT);
  
  digitalWrite(POWA, HIGH);    // turn on SD card and ADC
  delay(1);    // give some delay to ensure RTC and SD are initialized properly

  ads.begin();
  //ads.setGain(GAIN_TWO);  // set gain here
  
  if(!sd.begin(SDcsPin, SPI_FULL_SPEED))  // initialize SD card on the SPI bus  - very important
  {
    delay(10);
    SDcardError();
  }
  else
  {
    delay(10);
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
  }
  
  RTC.checkInterval(hourStart, minStart, interval); // Check if the logging interval is in secs, mins or hours
  RTC.alarm2set(dayStart, hourStart, minStart);  // Configure begin time
  RTC.alarmFlagClear();  // clear alarm flag
  
  chip.sleepInterruptSetup();    // setup sleep function & pin change interrupts on the ATmega328p. Power-down mode is used here
}

// loop ****************************************************************
void loop()
{
  digitalWrite(POWA, LOW);  // turn off microSD card to save power
  delay(1);  // give some delay for SD card power to be low before processor sleeps to avoid it being stuck
  
  chip.turnOffADC();    // turn off ADC to save power
  pinMode(A0, OUTPUT); // this is set for this particular version of the datalogger only
                       // because the POWER pin is A0 and it is configured as INPUT after
                       // turning ADC off which results in some power draw by the microSD card during sleep mode
  
  chip.turnOffSPI();    // turn off SPI bus to save power
  //chip.turnOffWDT();  // turn off WatchDog Timer to save power (does not work for Pro Mini - only works for Uno)
  chip.turnOffBOD();    // turn off Brown-out detection to save power
  
  chip.goodNight();    // put processor in extreme power down mode - GOODNIGHT!
                       // this function saves previous states of analog pins and sets them to LOW INPUTS
                       // average current draw on Mini Pro should now be around 0.195 mA (with both onboard LEDs taken out)
                       // Processor will only wake up with an interrupt generated from the RTC, which occurs every logging interval
  
  
  chip.turnOnADC();    // enable ADC after processor wakes up
  chip.turnOnSPI();   // turn on SPI bus once the processor wakes up
  delay(1);    // important delay to ensure SPI bus is properly activated
  
  RTC.alarmFlagClear();    // clear alarm flag
  pinMode(POWA, OUTPUT);
  digitalWrite(POWA, HIGH);  // turn on SD card power
  delay(1);    // give delay to let the SD card get full powa
  
  RTC.checkDST(); // check and account for Daylight Savings Time in US
  
  // get ADC values from all channels
  float adc0 = samples(0);
  float adc1 = samples(1);
  float adc2 = samples(2);
  float adc3 = samples(3);
    
  // convert adc values to voltage
  float V0 = voltage(adc0, 0);  // set gain here based on setting above. Options: 0(default), 1, 2, 4, 8, 16
  float V1 = voltage(adc1, 0);
  float V2 = voltage(adc2, 0);
  float V3 = voltage(adc3, 0);
  
  pinMode(SDcsPin, OUTPUT);
  if(!sd.begin(SDcsPin, SPI_FULL_SPEED))    // very important - reinitialize SD card on the SPI bus
  {
    delay(10);
    SDcardError();
  }
  else
  {
    delay(10);
    file.open(filename, O_WRITE | O_AT_END);  // open file in write mode
    delay(1);
    
    String time = RTC.timeStamp();    // get date and time from RTC
    SPCR = 0;  // reset SPI control register
    
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

// file timestamps ****************************************************************
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
