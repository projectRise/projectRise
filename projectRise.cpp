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
#include <SPI.h>
#include <SdFat.h>
#include <RTClib.h>
#include "types.h"
#include "WeatherShield.hpp"
#include "LightTracker.hpp"
#include "debug.hpp"
#include "Sleep_n0m1.h" //A library that sets the Arduino into sleep mode

#define ARRCNT(x)   (sizeof((x)) / sizeof(*(x)))
#define STRLEN(x)   (ARRCNT((x)) - 1U)

const uint8_t PIN_SD_CS = SS;
SdFat SD;

#define timetosleep 50000 //defines how many ms you want arduino to sleep
Sleep sleep;
unsigned long sleepTime; //how long you want the arduino to sleep

WeatherShield weatherShield(A3, A1, A2, 3.3f);

RTC_DS3231 rtc;

LightTracker lightTracker(44, 45, A8, A9, A10, A11);

//Hardware pin definitions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
const byte PIN_STAT_BLUE            = 7;
const byte PIN_STAT_GREEN           = 8;

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

    DebugPrintLine("Light tracker...");
    lightTracker.Begin();

    pinMode(PIN_STAT_BLUE, OUTPUT); //Status LED Blue
    pinMode(PIN_STAT_GREEN, OUTPUT); //Status LED Green

    DebugPrintLine("Weather shield...");
    if(!weatherShield.Begin())
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
    unsigned long now = millis();
    if((long)(now - nextUpdate) >= 0)
    {
        saveToFile();
        nextUpdate += UPDATE_INTERVAL;
    }

    if(lightTracker.Poll())
    {
        delay(35);
    }
    else
    {
        //sleep.pwrDownMode(); //set sleep mode
        //sleep.sleepDelay(sleepTime); //sleep for: sleepTime
    }
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
    result.data[1].value = weatherShield.GetHumidity();
    if(result.data[2].value == 998)
    {
        return false;
    }

    result.header.timestamp = rtc.now().unixtime();
    result.header.count = ARRCNT(result.data);
    result.data[0].type = 1;
    result.data[0].value = weatherShield.GetTemperature();
    //result.data[1].type = 2;
    //result.data[1].value = weatherShield.GetHumidity();
    result.data[2].type = 3;
    result.data[2].value = weatherShield.GetPressure();
    result.data[3].type = 4;
    result.data[3].value = weatherShield.GetLightLevel();
    result.data[4].type = 5;
    result.data[4].value = weatherShield.GetBatteryLevel();

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
