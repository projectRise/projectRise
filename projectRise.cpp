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
#include <limits.h>
#include <errno.h>
#include <Wire.h> //I2C needed for sensors
#include "SparkFunMPL3115A2.h" //Pressure sensor - Search "SparkFun MPL3115" and install from Library Manager
#include "SparkFun_Si7021_Breakout_Library.h" //Humidity sensor - Search "SparkFun Si7021" and install from Library Manager
#include <SPI.h>
#include <SdFat.h>
#include <LowPower.h>
#include <RTClib.h>
#include "types.h"
#include "LightTracker.hpp"
#include "debug.hpp"
#include "Sleep_n0m1.h" //A library that sets the Arduino into sleep mode

#define ARRCNT(x)   (sizeof((x)) / sizeof(*(x)))
#define STRLEN(x)   (ARRCNT((x)) - 1U)

const uint8_t PIN_SD_CS             = SS;
SdFat SD;

#define timetosleep 50000 //defines how many ms you want arduino to sleep
Sleep sleep;
unsigned long sleepTime; //how long you want the arduino to sleep

MPL3115A2 pressureSensor;   //Create an instance of the pressure sensor
Weather humiditySensor;     //Create an instance of the humidity sensor

RTC_DS3231 rtc;

LightTracker lt(10, 9, A2, A3, A0, A1);

//Hardware pin definitions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
const byte PIN_STAT_BLUE            = 7;
const byte PIN_STAT_GREEN           = 8;

const byte PIN_VREF                 = A3;
const byte PIN_LIGHTSENSOR          = A1;
const byte PIN_BATTERYSENSOR        = A2;

const unsigned long UPDATE_INTERVAL = 1000UL;

#define LOGDIR                      "weather"
#define LOGFILE_TEXT                "data.log"
#define LOGFILE_BINARY              "data.dat"

//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
unsigned long nextUpdate            = 0UL; //The millis counter to see when a second rolls by
unsigned int counter                = 0U;

bool getSensorValues(collection_t& result);
void saveToFile(void);
float getBatteryLevel(void);
float getLightLevel(void);

bool writeTextFile(const char* const filepath, const collection_t & content);
bool writeBinaryFile(const char* const filepath, const collection_t& content);
bool readBinaryFile(const char* const filepath, collection_t* const result, size_t* const resultCount, const size_t count, const size_t index = 0U);
void printSensorValues(const collection_t& content);

void commandMode(void);

void testReadFromFile(void);

#define CMD_INIT    "$$$"
#define CMD_OK      "OK"
#define CMD_FAIL    "FAIL"
#define CMD_UNKNOWN "UNKNOWN"
#define CMD_ABORT   "ABORT"
#define CMD_DT      "DT"

void setup(void)
{
    //delay(2500);
    Serial.begin(9600);

    sleepTime = timetosleep; //set sleep time in ms, max sleep time is 49.7 days
    //Leds that show status on board
    while(!Serial);

    DebugPrintLine("Initializing...");

    pinMode(24, OUTPUT);
    pinMode(25, OUTPUT);
    pinMode(27, OUTPUT);

    DebugPrintLine("SD card...");
    if(!SD.begin(PIN_SD_CS))
    {
        DebugPrintLine("Error: Failed to set up the device");
        while(true);
    }

    if(!SD.chdir(LOGDIR))
    {
        DebugPrintLine("Creating directory log directory...");
        if(!SD.mkdir(LOGDIR))
        {
            DebugPrintLine("Error: Failed to create directory");
            while(true);
        }

        if(!SD.chdir(LOGDIR))
        {
            DebugPrintLine("Error: Failed to change directory");
            while(true);
        }
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
    if(humiditySensor.getRH() == 998.0f || pressureSensor.readPressure() < 0.0f) //Humidty sensor failed to respond
    {
        DebugPrintLine("Failed");
        while(true);
    }

    DebugPrintLine("RTC...");
    if(!rtc.begin())
    {
        DebugPrintLine("Failed");
        while(true);
    }

    DateTime tmpTime = rtc.now();
    DebugPrint("Current time: ");
    char formatBuffer[] = "YYYY-MM-DD hh:mm:ss";
    DebugPrint(tmpTime.toString(formatBuffer));
    DebugPrint(" (");
    DebugPrint(tmpTime.unixtime());
    DebugPrintLine(")");

    if(rtc.lostPower())
    {
        DebugPrintLine("Warning: RTC may have lost track of time");
        delay(2500UL);
    }

    DebugPrintLine("Waiting for setup command...");
    Serial.setTimeout(2500UL);
    commandMode();
    Serial.setTimeout(1000UL); // Restore default timeout

    DebugPrintLine("Done");
    DebugPrintLine();
    DebugFlush();

    //while(true);

    //#define TEST_INPUT
    #ifdef TEST_INPUT
    testReadFromFile();
    while(true);
    #endif
}

void loop(void)
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

    lt.Poll();
    sleep.pwrDownMode(); //set sleep mode
    sleep.sleepDelay(sleepTime); //sleep for: sleepTime
}

#define TEST_READOFFSET     0U                                          // Position in the file to start reading (should be even divisible by size of 'collection_t').
#define TEST_ELEMENTCOUNT   2U                                          // How many 'collection_t' to request per read (a.k.a. 'collection_t' count of the buffer).
#define TEST_READCOUNT      5U                                          // Total number of read operations.
#define TEST_TOTALCOUNT     ((TEST_ELEMENTCOUNT) * (TEST_READCOUNT))    // The total number of 'collection_t' values to read.
void testReadFromFile(void)
{
    collection_t vals[TEST_ELEMENTCOUNT];
    size_t readCount = 0U;
    const size_t max = TEST_READOFFSET + TEST_TOTALCOUNT;
    for(size_t i = TEST_READOFFSET; i < max; i += TEST_ELEMENTCOUNT)
    {
        if(!readBinaryFile(LOGFILE_BINARY, vals, &readCount, TEST_ELEMENTCOUNT, i))
        {
            DebugPrintLine("Error: Could not read from file \'" LOGFILE_BINARY "\'");
            break;
        }

        for(size_t j = 0U; j < TEST_ELEMENTCOUNT; j++)
        {
            DebugPrint("VALUE["); DebugPrint(i + j); DebugPrintLine("]: ");
            printSensorValues(vals[j]);
        }
    }
}

void commandMode(void)
{
    Serial.println(CMD_INIT);

    #define CMD_BUFSIZE 16
    char cmdBuffer[16 + 1];
    size_t cmdLen = Serial.readBytesUntil('\n', cmdBuffer, sizeof(cmdBuffer));
    if(cmdLen == 0U)
    {
        return;
    }
    else if(cmdBuffer[cmdLen - 1] != '\r')
    {
        Serial.println(CMD_ABORT);
        return;
    }

    cmdBuffer[cmdLen - 1] = '\0';
    if(strcmp(cmdBuffer, CMD_INIT) != 0)
    {
        Serial.print(CMD_FAIL);
        return;
    }

    Serial.println(CMD_OK);
    Serial.flush();

    Serial.setTimeout(ULONG_MAX);
    bool active = true;
    while(active)
    {
        do
        {
            cmdLen = Serial.readBytesUntil('\n', cmdBuffer, sizeof(cmdBuffer));
            if(cmdLen == 0U || cmdBuffer[cmdLen - 1] != '\r')
            {
                active = false;
                Serial.println(CMD_ABORT);
                break;
            }

            cmdBuffer[cmdLen - 1] = '\0';
            if(strcmp(cmdBuffer, CMD_INIT) == 0)
            {
                active = false;
                Serial.println(CMD_OK);
            }
            else if(strncmp(cmdBuffer, CMD_DT, STRLEN(CMD_DT)) == 0)
            {
                errno = 0;
                unsigned long tmp = strtoul(cmdBuffer + STRLEN(CMD_DT), nullptr, 10);
                if(tmp == 0UL || errno != 0)
                {
                    Serial.print(CMD_FAIL);
                    break;
                }

                DateTime tmpTime = DateTime(tmp);
                rtc.adjust(tmpTime);
                Serial.print(CMD_OK);
                Serial.println(tmpTime.unixtime());
            }
            else
            {
                Serial.println(CMD_UNKNOWN);
            }
        } while(false); // Easier flow control

        Serial.flush();
    }
}

bool getSensorValues(collection_t& result)
{
    result.data[1].type = 2;
    result.data[1].value = humiditySensor.getRH();
    if(result.data[2].value == 998)
    {
        return false;
    }

    result.header.timestamp = rtc.now().unixtime();
    result.header.count = ARRCNT(result.data);
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

bool writeTextFile(const char* const filepath, const collection_t& content)
{
    File file = SD.open(filepath, FILE_WRITE);
    if(!file)
    {
        return false;
    }

    file.print("timestamp=");    file.print(content.header.timestamp);    file.print(", ");
    file.print("count=");        file.print(content.header.count);        file.print(", ");
    file.print("temperature=");  file.print(content.data[0].value, 2);    file.print(", ");
    file.print("humidity=");     file.print(content.data[1].value, 2);    file.print(", ");
    file.print("pressure=");     file.print(content.data[2].value, 2);    file.print(", ");
    file.print("light=");        file.print(content.data[3].value, 2);    file.print(", ");
    file.print("battery=");      file.print(content.data[4].value, 2);
    file.println();
    file.flush();

    file.close();
    return true;
}

bool writeBinaryFile(const char* const filepath, const collection_t& content)
{
    File file = SD.open(filepath, FILE_WRITE);
    if(!file)
    {
        return false;
    }

    file.write(&content, sizeof(content));
    file.flush();
    file.close();
    return true;
}

bool readBinaryFile(const char* const filepath, collection_t* const result, size_t* const resultCount, const size_t count, const size_t index)
{
    if(resultCount != nullptr)
    {
        *resultCount = 0U;
    }

    if(count < 1U)
    {
        return false;
    }

    File file = SD.open(filepath, FILE_READ);
    if(!file)
    {
        return false;
    }

    size_t totalSize = sizeof(*result) * count;
    if(index > 0U)
    {
        if(!file.seek(sizeof(*result) * index))
        {
            return false;
        }
    }

    int avail = file.available();
    if(avail <= 0 || (size_t)avail < totalSize)
    {
        return false;
    }

    int readSize = file.read(result, totalSize);
    if(readSize < 0)
    {
        return false;
    }

    if(resultCount != nullptr)
    {
        *resultCount = (size_t)readSize / sizeof(*result);
    }

    file.close();
    return true;
}

void printSensorValues(const collection_t& content)
{
    DebugPrint("timestamp=");   DebugPrint(content.header.timestamp);   DebugPrint(", ");
    DebugPrint("count=");       DebugPrint(content.header.count);       DebugPrint(", ");
    DebugPrint("temperature="); DebugPrint(content.data[0].value, 2);   DebugPrint(", ");
    DebugPrint("humidity=");    DebugPrint(content.data[1].value, 2);   DebugPrint(", ");
    DebugPrint("pressure=");    DebugPrint(content.data[2].value, 2);   DebugPrint(", ");
    DebugPrint("light=");       DebugPrint(content.data[3].value, 2);   DebugPrint(", ");
    DebugPrint("battery=");     DebugPrint(content.data[4].value, 2);
    DebugPrintLine();
    DebugFlush();
}

void saveToFile(void)
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
    if(!writeTextFile(LOGFILE_TEXT, values))
    {
        DebugPrintLine("Error: Could not open \'" LOGFILE_TEXT "\' for writing");
    }
    #endif

    #ifdef OUTPUT_BINARY
    if(!writeBinaryFile(LOGFILE_BINARY, values))
    {
        DebugPrintLine("Error: Could not open \'" LOGFILE_BINARY "\' for writing");
    }
    #endif

    DebugPrint("VALUE["); DebugPrint(counter); DebugPrintLine("]: ");
    printSensorValues(values);

    counter++;
}

#define WS_VOLTAGE    3.3
//Returns the voltage of the light sensor based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float getLightLevel(void)
{
    float operatingVoltage = analogRead(PIN_VREF);

    float lightSensor = analogRead(PIN_LIGHTSENSOR);

    operatingVoltage = WS_VOLTAGE / operatingVoltage; //The reference voltage is 3.3V

    lightSensor = operatingVoltage * lightSensor;

    return lightSensor / WS_VOLTAGE;
}

//Returns the voltage of the raw pin based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
//Battery level is connected to the RAW pin on Arduino and is fed through two 5% resistors:
//3.9K on the high side (R1), and 1K on the low side (R2)
float getBatteryLevel(void)
{
    float operatingVoltage = analogRead(PIN_VREF);

    float rawVoltage = analogRead(PIN_BATTERYSENSOR);

    operatingVoltage = WS_VOLTAGE / operatingVoltage; //The reference voltage is 3.3V

    rawVoltage = operatingVoltage * rawVoltage; //Convert the 0 to 1023 int to actual voltage on PIN_BATTERYSENSOR pin

    rawVoltage *= 4.9; //(3.9k+1k)/1k - multiple PIN_BATTERYSENSOR voltage by the voltage divider to get actual system voltage

    return rawVoltage;
}
