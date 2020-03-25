/*! projectRise.cpp
    Main code of the energy harvesting/weather station project
    \file       projectRise.cpp
    \authors    albrdev (albrdev@gmail.com), Ziiny (sebastian.cobert@gmail.com)
    \date       2020-02-06
*/

//#define ENABLE_SOFTWARE_SPI_CLASS 1

#include <stdint.h>
#include <SPI.h>
#include <SdFat.h>
#include <RTClib.h>
#include <Sleep_n0m1.h>         //  A library that sets the Arduino into sleep mode
#include <lmic.h>
#include <hal/hal.h>
#include "types.h"
#include "lora.hpp"
//#include "WeatherShield.hpp"    // SparkFun weather shield wrapper
#include "LightTracker.hpp"
#include "CommandHandler.hpp"
#include "misc.hpp"
#include "debug.hpp"
#include "sdios.h"

#include <Wire.h>
//#include <SPI.h>
#include <Adafruit_BMP280.h>

#define USE_SDIO 0

#define ARRCNT(x)   (sizeof((x)) / sizeof(*(x)))
#define STRLEN(x)   (ARRCNT((x)) - 1U)

#define ANALOG_MAX  ((1 << 10) - 1)

#define CONFIG_MISO_PIN 24
#define CONFIG_MOSI_PIN 23
#define CONFIG_SCK_PIN  25
#define CONFIG_CS_PIN   22
//SdFat sd;
SdFatSoftSpi<CONFIG_MISO_PIN, CONFIG_MOSI_PIN, CONFIG_SCK_PIN> sd;

unsigned long sleepDuration = 60UL * 1000UL; // Sleep duration between measurements

//WeatherShield weatherShield(A3, A1, A2, 3.3f);

RTC_DS3231 rtc;

LightTracker lightTracker(44, 45, A8, A9, A10, A11, 0.25 * ANALOG_MAX, 0.05 * ANALOG_MAX);

Adafruit_BMP280 bmp;

char commandBuffer[16 + 1];
CommandHandler commandHandler(Serial, commandBuffer, sizeof(commandBuffer), handleCommand);

#define LOGDIR                      "weather"
#define LOGFILE_TEXT                "data.log"
#define LOGFILE_BINARY              "data.dat"

unsigned long nextUpdate            = 0UL; // The millis counter to see when a second rolls by
unsigned int counter                = 0U;

bool getSensorValues(collection_t& result);
void saveToFile(void);

bool writeTextFile(const char* const filepath, const collection_t & content);
bool writeBinaryFile(const char* const filepath, const collection_t& content);
bool readBinaryFile(const char* const filepath, collection_t* const result, size_t* const resultCount, const size_t count, const size_t index = 0U);
void printSensorValues(const collection_t& content);

void testReadFromFile(void);

bool setupSensors(void)
{
    DebugPrintLine("BMP280...");
    if(!bmp.begin())
    {
        DebugPrintLine(F("Failed"));
        while(true);
    }

    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

    pinMode(A15, INPUT);
    return true;
}

void setup(void)
{
    //delay(2500);
    Serial.begin(9600);

    //Leds that show status on board
    while(!Serial);

    DebugPrintLine("Initializing...");

    DebugPrintLine("SD card...");
    if(!sd.begin(CONFIG_CS_PIN))
    {
        digitalWrite(9, HIGH);
        DebugPrintLine("Error: Failed to set up the device");
        while(true);
    }

    if(!sd.chdir(LOGDIR))
    {
        DebugPrintLine("Creating directory log directory...");
        if(!sd.mkdir(LOGDIR))
        {
            DebugPrintLine("Error: Failed to create directory");
            while(true);
        }

        if(!sd.chdir(LOGDIR))
        {
            DebugPrintLine("Error: Failed to change directory");
            while(true);
        }
    }

    DebugPrintLine("LoRa...");
    setupLoRa();

    DebugPrintLine("Light tracker...");
    lightTracker.Begin();

    /*DebugPrintLine("Weather shield...");
    if(!weatherShield.Begin())
    {
        DebugPrintLine("Failed");
        while(true);
    }*/

    DebugPrintLine("Sensors...");
    setupSensors();

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
    Serial.println(CMD_INIT);
    while(commandHandler.Receive())
    {
        commandHandler.Flush();
    }
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
        nextUpdate += sleepDuration;
    }

    os_runloop_once();

    if(lightTracker.Poll())
    {
        delay(20);
    }
    else
    {
        //sleep.pwrDownMode(); //set sleep mode
        //sleep.sleepDelay(sleepDuration); //sleep for: sleepDuration
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

bool getSensorValues(collection_t& result)
{
    Adafruit_Sensor* bmpTemp = bmp.getTemperatureSensor();
    Adafruit_Sensor* bmpPressure = bmp.getPressureSensor();

    sensors_event_t temp_event, pressure_event;
    bmpTemp->getEvent(&temp_event);
    bmpPressure->getEvent(&pressure_event);

    result.header.timestamp = rtc.now().unixtime();
    result.header.count = ARRCNT(result.data);
    result.data[0].type = 1;
    result.data[0].value = temp_event.temperature;
    result.data[1].type = 2;
    result.data[1].value = pressure_event.pressure;
    result.data[2].type = 3;
    result.data[2].value =  (float)analogRead(A15) / (float)ANALOG_MAX;

    return true;
}

/*bool getSensorValues(collection_t& result)
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
}*/

bool writeTextFile(const char* const filepath, const collection_t& content)
{
    File file = sd.open(filepath, FILE_WRITE);
    if(!file)
    {
        return false;
    }

    file.print("timestamp=");    file.print(content.header.timestamp);    file.print(", ");
    file.print("count=");        file.print(content.header.count);        file.print(", ");
    file.print("temperature=");  file.print(content.data[0].value, 2);    file.print(", ");
    file.print("pressure=");     file.print(content.data[2].value, 2);    file.print(", ");
    file.print("light=");        file.print(content.data[3].value, 2);
    file.println();
    file.flush();

    file.close();
    return true;
}

/*bool writeTextFile(const char* const filepath, const collection_t& content)
{
    File file = sd.open(filepath, FILE_WRITE);
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
}*/

bool writeBinaryFile(const char* const filepath, const collection_t& content)
{
    File file = sd.open(filepath, FILE_WRITE);
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

    File file = sd.open(filepath, FILE_READ);
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
    DebugPrint("pressure=");    DebugPrint(content.data[1].value, 2);   DebugPrint(", ");
    DebugPrint("light=");       DebugPrint(content.data[2].value, 2);
    DebugPrintLine();
    DebugFlush();
}

/*void printSensorValues(const collection_t& content)
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
}*/

collection_t measurementBuffer;
void saveToFile(void)
{
    if(!getSensorValues(measurementBuffer))
    {
        DebugPrintLine("Error: Failed to retrieve sensor values");
        return;
    }

    DebugPrint("VALUE["); DebugPrint(counter); DebugPrintLine("]: "); //*
    printSensorValues(measurementBuffer); //*
    counter++; //*
    return; //*

    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    #ifdef OUTPUT_TEXT
    if(!writeTextFile(LOGFILE_TEXT, measurementBuffer))
    {
        DebugPrintLine("Error: Could not open \'" LOGFILE_TEXT "\' for writing");
    }
    #endif

    #ifdef OUTPUT_BINARY
    if(!writeBinaryFile(LOGFILE_BINARY, measurementBuffer))
    {
        DebugPrintLine("Error: Could not open \'" LOGFILE_BINARY "\' for writing");
    }
    #endif

    DebugPrint("VALUE["); DebugPrint(counter); DebugPrintLine("]: ");
    printSensorValues(measurementBuffer);

    counter++;
}
