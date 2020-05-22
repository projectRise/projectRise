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
#include "types.h"
#include "CommandHandler.hpp"
#include "Sunrise.h"
#include "LightTracker.hpp"
#include "misc.hpp"
#include "debug.hpp"
#include "sdios.h"

#define USE_SDIO                0

#define ANALOG_MAX              ((1 << 10) - 1)

#define PIN_EN                  2
#define PIN_NRDY                3

#define MS_PER_H                ((60UL * 60UL) * 1000UL)
#define MEASUREMENT_INTERVAL    (5UL * 1000UL)

#define LOGDIR                  "environg"
#define LOGFILE_TEXT            "data.log"
#define LOGFILE_BINARY          "data.dat"

#define TEST_READOFFSET         0U                                          // Position in the file to start reading (should be even divisible by size of 'payload_t').
#define TEST_ELEMENTCOUNT       2U                                          // How many 'payload_t' to request per read (a.k.a. 'payload_t' count of the buffer).
#define TEST_READCOUNT          5U                                          // Total number of read operations.
#define TEST_TOTALCOUNT         ((TEST_ELEMENTCOUNT) * (TEST_READCOUNT))    // The total number of 'payload_t' values to read.

char commandBuffer[16 + 1];
payload_t measurement;
unsigned long int sleepDuration = 60UL * 1000UL; // Sleep duration between measurements
unsigned int counter = 0U;

CommandHandler commandHandler(Serial, commandBuffer, sizeof(commandBuffer), handleCommand);
SdFat sd;
RTC_DS3231 rtc;

Sunrise sunrise(0x69);
volatile uint8_t isReady = false;
uint16_t hourCount = 0U;
unsigned long int nextHour;
unsigned long int nextMeasurement;
void nrdyISR(void);
bool awaitISR(unsigned long int timeout = 2000UL);

LightTracker lightTracker(5, 6, A0, A1, A2, A3, 0.25f * ANALOG_MAX, 0.05f * ANALOG_MAX);

void setupSD(void);
void setupRTC(void);
void setupSunrise(void);

bool readCO2(void);
bool writeTextFile(const char* filepath, const payload_t& content);
bool writeBinaryFile(const char* filepath, const payload_t& content);
bool readBinaryFile(const char* filepath, payload_t* result, size_t* resultCount, size_t count, size_t index = 0U);

void switchMode(measurementmode_t mode);
void delayUntil(unsigned long int time);

void setup(void)
{
    delay(2500UL);
    Serial.begin(9600);
    while(!Serial);

    DebugPrintLineF("Initializing...");

    #ifdef SD_ENABLE
    DebugPrintLineF("SD card...");
    setupSD();
    #endif

    #ifdef RTC_ENABLE
    setupRTC();
    #endif

    #ifdef SENSOR_ENABLE
    setupSunrise();
    #endif

    #ifdef LIGHT_TRACKER_ENABLE
    DebugPrintLineF("Light tracker...");
    lightTracker.Begin();
    #endif

    DebugPrintLineF("Waiting for setup command...");
    Serial.setTimeout(2500UL);
    Serial.println(CMD_INIT);
    while(commandHandler.Receive())
    {
        commandHandler.Flush();
    }
    Serial.setTimeout(1000UL); // Restore default timeout

    DebugPrintLineF("Done");
    DebugPrintLine();
    DebugFlush();

    //#define TEST_INPUT
    #ifdef TEST_INPUT
    testReadFromFile();
    while(true);
    #endif

    nextHour = millis() + MS_PER_H;
    nextMeasurement = millis();
}

void loop(void)
{
    measurement = { 0xFFFFFFFF, 0xFFFF, 0U, 0U };

    #ifdef RTC_ENABLE
    DateTime now = rtc.now();
    measurement.timestamp = now.unixtime();
    #endif

    #ifdef SENSOR_ENABLE
    if(!readCO2())
    {
        DebugPrintLineF("Error: Failed to retrieve sensor values");
        return;
    }
    #endif

    #ifdef OUTPUT_BINARY
    #ifdef SD_ENABLE
    if(!writeBinaryFile(LOGFILE_BINARY, measurement))
    {
        DebugPrintLineF("Error: Could not open \'" LOGFILE_BINARY "\' for writing");
    }
    #endif
    #endif

    #ifdef OUTPUT_CSV
    #ifdef SD_ENABLE
    if(!writeTextFile(LOGFILE_TEXT, measurement))
    {
        DebugPrintLine("Error: Could not open \'" LOGFILE_TEXT "\' for writing");
    }
    #endif
    #endif

    DebugPrint("VALUE["); DebugPrint(counter); DebugPrintLine("]");
    printSensorValues(measurement);
    counter++;

    #ifdef LIGHT_TRACKER_ENABLE
    while(lightTracker.Poll())
    {
        delay(20);
    }
    #endif

    #ifdef SLEEP_ENABLE
    sleep.pwrDownMode();
    sleep.sleepDelay(sleepDuration);
    #else
    nextMeasurement += sleepDuration;
    delayUntil(nextMeasurement);
    #endif
}

void setupSD(void)
{
    DebugPrintLineF("SD card...");

    if(!sd.begin(SS, SD_SCK_MHZ(50)))
    {
        digitalWrite(9, HIGH);
        DebugPrintLine("Error: Failed to set up the device");
        sd.initErrorPrint();
        while(true);
    }

    if(!sd.chdir(LOGDIR))
    {
        DebugPrintLine("Creating directory...");
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
}

void setupRTC(void)
{
    DebugPrintLineF("RTC...");
    if(!rtc.begin())
    {
        DebugPrintLineF("Failed");
        while(true);
    }

    DateTime tmpTime = rtc.now();
    DebugPrintF("Current time: ");
    char formatBuffer[] = "YYYY-MM-DD hh:mm:ss";
    DebugPrint(tmpTime.toString(formatBuffer));
    DebugPrintF(" (");
    DebugPrint(tmpTime.unixtime());
    DebugPrintLineF(")");

    if(rtc.lostPower())
    {
        DebugPrintLineF("Warning: RTC may have lost track of time");
        delay(2500UL);
    }
}

void setupSunrise(void)
{
    DebugPrintLineF("Sensor...");

    pinMode(PIN_NRDY, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_NRDY), nrdyISR, FALLING);

    // Initial device setup. Also retrieves initial measurement state.
    if(!sunrise.Begin(PIN_EN, true))
    {
        DebugPrintLineF("Error: Could not initialize the device");
        while(true);
    }

    sunrise.Awake();
    switchMode(measurementmode_t::MM_SINGLE);

    metercontrol_t mc;
    if(!sunrise.GetMeterControlEE(mc))
    {
        DebugPrintLineF("*** ERROR: Could not get meter control");
        while(true);
    }

    if(!mc.nrdy)
    {
        DebugPrintLineF("*** ERROR: NRDY option should be enabled");
        while(true);
    }

    sunrise.Sleep();
}

bool readCO2(void)
{
    // Wake up the sensor.
    sunrise.Awake();
    DebugPrintLineF("Measuring...");

    // Count hours (taking time rollover in consideration).
    if((long)(millis() - nextHour) >= 0L)
    {
        hourCount++;
        nextHour += MS_PER_H;
    }

    // Single measurement mode requires host device to increment ABC time.
    uint16_t tmpHourCount = sunrise.GetABCTime();
    if(hourCount != tmpHourCount)
    {
        sunrise.SetABCTime(hourCount);
        DebugPrintLineF("Setting ABC time:  "); Serial.println(hourCount);
    }

    // Reset ISR variable and start new measurement.
    isReady = false;
    if(!sunrise.StartSingleMeasurement())
    {
        DebugPrintLineF("*** ERROR: Could not start single measurement");
        return false;
    }

    // Wait for the sensor to complate the measurement and signal the ISR (or timeout).
    unsigned long int measurementStartTime = millis();
    if(!awaitISR())
    {
        DebugPrintLineF("*** ERROR: ISR timeout");
        return false;
    }

    unsigned long int measurementDuration = millis() - measurementStartTime;
    Serial.print("Duration:          "); Serial.println(measurementDuration);

    // Read and print measurement values.
    if(!sunrise.ReadMeasurement())
    {
        DebugPrintLineF("*** ERROR: Could not read measurement");
        return false;
    }

    measurement.error = sunrise.GetErrorStatusRaw();
    measurement.co2 = sunrise.GetCO2();

    DebugPrint("Error status:      "); DebugPrintLine(measurement.error, BIN);
    DebugPrint("CO2:               "); DebugPrintLine(measurement.co2);
    DebugPrint("Temperature:       "); DebugPrintLine(sunrise.GetTemperature());

    // Put the sensor into sleep mode and wait for the next measurement.
    sunrise.Sleep();
    return true;
}

bool writeTextFile(const char* filepath, const payload_t& content)
{
    File file = sd.open(filepath, FILE_WRITE);
    if(!file)
    {
        return false;
    }

    file.print(content.timestamp);  file.print(";");
    file.print(content.co2);        file.print(";");
    file.print(content.battery);    //file.print(";");
    file.println();
    file.flush();

    file.close();
    return true;
}

bool writeBinaryFile(const char* filepath, const payload_t& content)
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

bool readBinaryFile(const char* filepath, payload_t* result, size_t* resultCount, size_t count, size_t index)
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

void nrdyISR(void)
{
    isReady = true;
}

bool awaitISR(unsigned long int timeout)
{
    timeout += millis();
    while(!isReady && (long)(millis() - timeout) < 0L);
    return isReady;
}

void delayUntil(unsigned long int time)
{
    while((long)(millis() - time) < 0L);
}

void switchMode(measurementmode_t mode)
{
    while(true)
    {
        measurementmode_t measurementMode;
        if(!sunrise.GetMeasurementModeEE(measurementMode))
        {
            Serial.println("*** ERROR: Could not get measurement mode");
            while(true);
        }

        if(measurementMode == mode)
        {
            break;
        }

        Serial.println("Attempting to switch measurement mode...");
        if(!sunrise.SetMeasurementModeEE(mode))
        {
            Serial.println("*** ERROR: Could not set measurement mode");
            while(true);
        }

        if(!sunrise.HardRestart())
        {
            Serial.println("*** ERROR: Failed to restart the device");
            while(true);
        }
    }
}
