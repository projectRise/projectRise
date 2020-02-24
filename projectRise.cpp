/*
 Weather Shield Example
 By: Nathan Seidle
 SparkFun Electronics
 Date: June 10th, 2016
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

 This example prints the current humidity, air pressure, temperature and light levels.

 The weather shield is capable of a lot. Be sure to checkout the other more advanced examples for creating
 your own weather station.

 Updated by Joel Bartlett
 03/02/2017
 Removed HTU21D code and replaced with Si7021
 */

#include <stdint.h>
#include <Wire.h> //I2C needed for sensors
#include "SparkFunMPL3115A2.h" //Pressure sensor - Search "SparkFun MPL3115" and install from Library Manager
#include "SparkFun_Si7021_Breakout_Library.h" //Humidity sensor - Search "SparkFun Si7021" and install from Library Manager
#include <SPI.h>
#include "SdFat.h"
#include "LowPower.h"
#include "types.h"
#include "debug.hpp"

const uint8_t PIN_SD_CS             = SS;
SdFat SD;

MPL3115A2 pressureSensor;   //Create an instance of the pressure sensor
Weather humiditySensor;     //Create an instance of the humidity sensor

//Hardware pin definitions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
const byte PIN_STAT_BLUE            = 7;
const byte PIN_STAT_GREEN           = 8;

const byte PIN_VREF                 = A3;
const byte PIN_LIGHTSENSOR          = A1;
const byte PIN_BATTERYSENSOR        = A2;

const unsigned long UPDATE_INTERVAL = 1000UL;

#define LOGFILE_TEXT                "weather.log"
#define LOGFILE_BINARY              "weather.dat"

//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
unsigned long nextUpdate            = 0UL; //The millis counter to see when a second rolls by
unsigned int counter                = 0U;

bool getSensorValues(collection_t& result);
void saveToFile();
float getBatteryLevel();
float getLightLevel();

void setup()
{
    //delay(2500);
    Serial.begin(9600);

    //Leds that show status on board
    while(!Serial);

    DebugPrintLine("Initializing...");

    pinMode(24, OUTPUT);
    pinMode(25, OUTPUT);
    pinMode(27, OUTPUT);

    DebugPrintLine("SD card...");
    if(!SD.begin(PIN_SD_CS))
    {
        DebugPrintLine("Failed");
        while(true);
    }

    DebugPrintLine("Weather shield...");
    pinMode(PIN_STAT_BLUE, OUTPUT); //Status LED Blue
    pinMode(PIN_STAT_GREEN, OUTPUT); //Status LED Green

    pinMode(PIN_VREF, INPUT);
    pinMode(PIN_LIGHTSENSOR, INPUT);

    //Configure the pressure sensor
    pressureSensor.begin(); // Get sensor online
    pressureSensor.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
    pressureSensor.setOversampleRate(7); // Set Oversample to the recommended 128
    pressureSensor.enableEventFlags(); // Enable all three pressure and temp event flags

    //Configure the humidity sensor
    humiditySensor.begin();
    if(humiditySensor.getRH() == 998) //Humidty sensor failed to respond
    {
        DebugPrintLine("Failed");
        while(true);
    }

    DebugPrintLine("Done");
    DebugPrintLine();
    DebugFlush();
}

void loop()
{
    // Enter idle state for 8 s with the rest of peripherals turned off
    // Each microcontroller comes with different number of peripherals
    // Comment off line of code where necessary

    // ATmega328P, ATmega168
    //LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, 
    //              SPI_OFF, USART0_OFF, TWI_OFF);

    // ATmega32U4
    //LowPower.idle(SLEEP_8S, ADC_OFF, TIMER4_OFF, TIMER3_OFF, TIMER1_OFF, 
    //		  TIMER0_OFF, SPI_OFF, USART1_OFF, TWI_OFF, USB_OFF);

    // ATmega2560
    //LowPower.idle(SLEEP_2S, ADC_OFF, TIMER5_OFF, TIMER4_OFF, TIMER3_OFF, 
    //        TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART3_OFF, 
    //        USART2_OFF, USART1_OFF, USART0_OFF, TWI_OFF);

    // ATmega256RFR2
    //LowPower.idle(SLEEP_8S, ADC_OFF, TIMER5_OFF, TIMER4_OFF, TIMER3_OFF, 
    //      TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF,
    //      USART1_OFF, USART0_OFF, TWI_OFF);

    unsigned long now = millis();
    if((long)(now - nextUpdate) >= 0)
    {
        //Prints before the LowPower.powerSave() function wont run.
        // ATmega2560
        digitalWrite(25, LOW);
        digitalWrite(27, HIGH);
        LowPower.powerSave(SLEEP_8S, ADC_OFF, BOD_OFF, TIMER2_OFF);
        DebugPrintLine("Wake up");
        digitalWrite(27, LOW);
        digitalWrite(25, LOW);
        digitalWrite(24, HIGH);
        //digitalWrite(PIN_STAT_BLUE, HIGH); //Blink stat LED

        saveToFile();

        nextUpdate += UPDATE_INTERVAL;
    }

    //delay(2000);
    //DebugPrintLine("Sleep");
}

bool getSensorValues(collection_t& result)
{
    result.data[1].type = 2;
    result.data[1].value = humiditySensor.getRH();
    if(result.data[2].value == 998)
    {
        return false;
    }

    result.header.timestamp = 1582529742 + (millis() / 1000UL); // TODO: Get value from RTC
    result.header.count = sizeof(result.data) / sizeof(*result.data);
    result.data[0].type = 1;
    result.data[0].value = humiditySensor.getTemp();
    //result.data[1].type = 2;
    //result.data[1].value = humiditySensor.getRH();
    result.data[2].type = 3;
    result.data[2].value = pressureSensor.readPressure();
    result.data[3].type = 4;
    result.data[3].value = getLightLevel();
    result.data[4].type = 5;
    result.data[4].value = getBatteryLevel();

    return true;
}

void saveToFile()
{
    collection_t values;
    if(!getSensorValues(values))
    {
        DebugPrintLine("Error: Failed to retrieve sensor values");
        return;
    }

    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    #ifdef OUTPUT_TEXT
    File textLog = SD.open(LOGFILE_TEXT, FILE_WRITE);
    if(textLog)
    {
        textLog.print("VALUE["); textLog.print(counter); textLog.println("]");
        textLog.print("    timestamp=");   textLog.println(values.header.timestamp);
        textLog.print("    count=");       textLog.println(values.header.count);
        textLog.print("    temperature="); textLog.println(values.data[0].value, 2);
        textLog.print("    humidity=");    textLog.println(values.data[1].value, 2);
        textLog.print("    pressure=");    textLog.println(values.data[2].value, 2);
        textLog.print("    light=");       textLog.println(values.data[3].value, 2);
        textLog.print("    battery=");     textLog.println(values.data[4].value, 2);
        textLog.println();
        textLog.flush();

        textLog.close();
    }
    else
    {
        DebugPrintLine("Error: Could not open \'" LOGFILE_TEXT "\'");
    }
    #endif

    #ifdef OUTPUT_BINARY
    File binaryLog = SD.open(LOGFILE_BINARY, FILE_WRITE);
    if(binaryLog)
    {
        binaryLog.write(&values, sizeof(values));
        binaryLog.flush();
        binaryLog.close();
    }
    else
    {
        DebugPrintLine("Error: Could not open \'" LOGFILE_BINARY "\'");
    }
    #endif

    DebugPrint("VALUE["); DebugPrint(counter); DebugPrintLine("]");
    DebugPrint("    timestamp=");     DebugPrintLine(values.header.timestamp);
    DebugPrint("    count=");         DebugPrintLine(values.header.count);
    DebugPrint("    temperature=");   DebugPrintLine(values.data[0].value, 2);
    DebugPrint("    humidity=");      DebugPrintLine(values.data[1].value, 2);
    DebugPrint("    pressure=");      DebugPrintLine(values.data[2].value, 2);
    DebugPrint("    light=");         DebugPrintLine(values.data[3].value, 2);
    DebugPrint("    battery=");       DebugPrintLine(values.data[4].value, 2);
    DebugPrintLine();
    DebugFlush();

    counter++;
}

#define VOLT    3.3
//#define VOLT    5.0
//Returns the voltage of the light sensor based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float getLightLevel()
{
    float operatingVoltage = analogRead(PIN_VREF);

    float lightSensor = analogRead(PIN_LIGHTSENSOR);

    operatingVoltage = VOLT / operatingVoltage; //The reference voltage is 3.3V

    lightSensor = operatingVoltage * lightSensor;

    return lightSensor / VOLT;
}

//Returns the voltage of the raw pin based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
//Battery level is connected to the RAW pin on Arduino and is fed through two 5% resistors:
//3.9K on the high side (R1), and 1K on the low side (R2)
float getBatteryLevel()
{
    float operatingVoltage = analogRead(PIN_VREF);

    float rawVoltage = analogRead(PIN_BATTERYSENSOR);

    operatingVoltage = VOLT / operatingVoltage; //The reference voltage is 3.3V

    rawVoltage = operatingVoltage * rawVoltage; //Convert the 0 to 1023 int to actual voltage on PIN_BATTERYSENSOR pin

    rawVoltage *= 4.9; //(3.9k+1k)/1k - multiple PIN_BATTERYSENSOR voltage by the voltage divider to get actual system voltage

    return rawVoltage;
}