/*! projectRise.cpp
    Main code of the energy harvesting/air quality project
    \file       projectRise.cpp
    \authors    albrdev (albrdev@gmail.com), Ziiny (sebastian.cobert@gmail.com)
    \date       2020-02-06
*/

#include <stdint.h>
#include <SPI.h>
#ifdef SD_ENABLE
#include <SdFat.h>
#endif

#include <RTClib.h>

#if SLEEP_ENABLE
#include <Sleep_n0m1.h>
#endif

#include "types.h"
#include "CommandHandler.hpp"
#include "Sunrise.h"
#include "LightTracker.hpp"
#include "misc.hpp"
#include "debug.hpp"
#ifdef SD_ENABLE
#include "sdios.h"
#endif

const char* ERRMSG_RTC_INIT = "Could not initialize RTC device";
const char* ERRMSG_SENSOR_INIT = "Could not initialize sensor device";
const char* ERRMSG_SENSOR_GET_METER_CONTROL = "Could not get meter control";
const char* ERRMSG_SENSOR_NRDY = "ERROR : NRDY option should be enabled";
const char* ERRMSG_SENSOR_NUMBER_OF_SAMPLES = "ERROR : Could not get number of samples";
const char* ERRMSG_SENSOR_START_MEASRUEENT = "Could not start measurement";
const char* ERRMSG_SENSOR_ISR_TIMEOUT = "ISR timeout";
const char* ERRMSG_SENSOR_READ_MEASUREMENT = "Could not read measurement";
const char* ERRMSG_SENSOR_GET_MEASUREMENT_MODE = "Could not get measurement mode";
const char* ERRMSG_SENSOR_SET_MEASUREMENT_MODE = "Could not set measurement mode";
const char* ERRMSG_SENSOR_RESTART = "Failed to restart sensor device";
const char* WRNMSG_RTC_LOST_TIME = "RTC may have lost track of time";

#ifdef SD_ENABLE
#define USE_SDIO                0
#endif

#define ANALOG_MAX              ((1 << 10) - 1)

#define PIN_ERRORLED            8
#define PIN_OKLED               9

#define SUNRISE_ADDRESS         0x69
#define PIN_EN                  2
#define PIN_NRDY                3
#define MEASUREMENT_INTERVAL    (5UL * 1000UL)
#define SECONDS_PER_HOUR        ((60U * 60U) * 1000U)

#define SAMPLE_DURATION         200UL
#define CALIBRATION_DURATION    50UL

#define LOGDIR                  "environg"
#define LOGFILE_TEXT            "data.csv"
#define LOGFILE_BINARY          "data.dat"
#define ERRORFILE               "error.log"

#define TEST_READOFFSET         0U                                          // Position in the file to start reading (should be even divisible by size of 'payload_t').
#define TEST_ELEMENTCOUNT       2U                                          // How many 'payload_t' to request per read (a.k.a. 'payload_t' count of the buffer).
#define TEST_READCOUNT          5U                                          // Total number of read operations.
#define TEST_TOTALCOUNT         ((TEST_ELEMENTCOUNT) * (TEST_READCOUNT))    // The total number of 'payload_t' values to read.

char commandBuffer[16 + 1];
payload_t measurement;
unsigned long int sleepDuration = 60UL * 1000UL; // Sleep duration between measurements
unsigned int counter = 0U;

CommandHandler commandHandler(Serial, commandBuffer, sizeof(commandBuffer), handleCommand);

#ifdef SD_ENABLE
SdFat sd;
#endif

RTC_DS3231 rtc;
DateTime start;
DateTime now;

Sunrise sunrise(SUNRISE_ADDRESS);
volatile uint8_t isReady = false;
uint16_t hourCount = 0U;
unsigned long int nextHour;
unsigned long int nextMeasurement;
unsigned long int estimatedMeasurementDuration;

LightTracker lightTracker(5, 6, A0, A1, A2, A3, 0.25f * ANALOG_MAX, 0.05f * ANALOG_MAX);

typedef enum
{
    LT_NONE,
    LT_ERROR,
    LT_WARNING
} logtype_t;

void setupSD(void);
void setupRTC(void);
void setupSunrise(void);

bool readCO2(void);
bool writeTextFile(const char* filepath, const payload_t& content);
bool writeBinaryFile(const char* filepath, const payload_t& content);
bool readBinaryFile(const char* filepath, payload_t* result, size_t* resultCount, size_t count, size_t index = 0U);
bool logError(logtype_t mode, const char* msg);

void setState(bool status, bool halt = true);
void delayUntil(unsigned long int time);
void switchMode(measurementmode_t mode);

void nrdyISR(void);
bool awaitISR(unsigned long int timeout = 2000UL);
void fileTimestamp(uint16_t* date, uint16_t* time);

void setup(void)
{
    delay(2500UL);
    Serial.begin(9600);
    while(!Serial);

    DebugPrintLineF("Initializing...");
    DebugFlush();

    pinMode(PIN_ERRORLED, OUTPUT);
    pinMode(PIN_OKLED, OUTPUT);
    digitalWrite(PIN_ERRORLED, LOW);
    digitalWrite(PIN_OKLED, LOW);

    #ifdef SD_ENABLE
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

    #ifdef SETUP_ENABLE
    DebugPrintLineF("Waiting for setup command...");
    Serial.setTimeout(2500UL);
    Serial.println(CMD_INIT);
    while(commandHandler.Receive())
    {
        commandHandler.Flush();
    }
    Serial.setTimeout(1000UL); // Restore default timeout
    #endif

    DebugPrintLineF("Done");
    DebugPrintLine();
    DebugFlush();

    //#define TEST_INPUT
    #ifdef TEST_INPUT
    testReadFromFile();
    while(true) { yield(); }
    #endif

    nextMeasurement = millis();
    start = rtc.now();

    #ifdef __DEBUG__
    char formatBuffer[] = "YYYY-MM-DD hh:mm:ss";
    DebugPrint(start.toString(formatBuffer));
    DebugPrintF(" (");
    DebugPrint(start.unixtime());
    DebugPrintLineF(")");
    #endif
}

void loop(void)
{
    setState(true);
    measurement = { 0xFFFFFFFF, 0xFFFF, 0U, 0U };

    #ifdef RTC_ENABLE
    now = rtc.now();
    measurement.timestamp = now.unixtime();
    #endif

    #ifdef SENSOR_ENABLE
    if(!readCO2())
    {
        return;
    }
    #endif

    #ifdef OUTPUT_BINARY
    #ifdef SD_ENABLE
    if(!writeBinaryFile(LOGFILE_BINARY, measurement))
    {
        DebugPrintLineF("ERROR: Could not write to file: \'" LOGFILE_BINARY "\'");
        //logError("ERROR: Could not write to file: \'" LOGFILE_BINARY "\'");
    }
    #endif
    #endif

    #ifdef OUTPUT_CSV
    #ifdef SD_ENABLE
    if(!writeTextFile(LOGFILE_TEXT, measurement))
    {
        DebugPrintLine("ERROR: Could not write to file: \'" LOGFILE_TEXT "\'");
        //logError("ERROR: Could not write to file: \'" LOGFILE_TEXT "\'");
    }
    #endif
    #endif

    DebugPrint("MEASUREMENT["); DebugPrint(counter); DebugPrintLine("]");
    printSensorValues(measurement);
    counter++;

    #ifdef LIGHT_TRACKER_ENABLE
    while(lightTracker.Poll())
    {
        delay(20);
    }
    #endif

    digitalWrite(PIN_ERRORLED, LOW);
    digitalWrite(PIN_OKLED, LOW);
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
    #ifdef SD_ENABLE
    if(!sd.begin(SS, SD_SCK_MHZ(50)))
    {
        DebugPrintLine("ERROR: Failed to initialize SD device");
        //sd.initErrorPrint();
        setState(false);
    }

    if(!sd.chdir(LOGDIR))
    {
        if(!sd.mkdir(LOGDIR))
        {
            DebugPrintLine("ERROR: Failed to create directory");
            setState(false);
        }

        if(!sd.chdir(LOGDIR))
        {
            DebugPrintLine("ERROR: Failed to change directory");
            setState(false);
        }
    }

    SdFile::dateTimeCallback(fileTimestamp);
    #endif
}

void setupRTC(void)
{
    if(!rtc.begin())
    {
        DebugPrintLineF(ERRMSG_RTC_INIT);
        logError(logtype_t::LT_ERROR, ERRMSG_RTC_INIT);
        setState(false);
    }

    if(rtc.lostPower())
    {
        DebugPrintLineF(WRNMSG_RTC_LOST_TIME);
        logError(logtype_t::LT_WARNING, WRNMSG_RTC_LOST_TIME);
        #ifdef SETUP_ENABLE
        delay(2500UL);
        #endif
    }
}

void setupSunrise(void)
{
    pinMode(PIN_NRDY, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_NRDY), nrdyISR, FALLING);

    // Initial device setup. Also retrieves initial measurement state.
    if(!sunrise.Begin(PIN_EN, true))
    {
        DebugPrintLineF(ERRMSG_SENSOR_INIT);
        logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_INIT);
        setState(false);
    }

    sunrise.Awake();
    switchMode(measurementmode_t::MM_SINGLE);

    metercontrol_t mc;
    if(!sunrise.GetMeterControlEE(mc))
    {
        DebugPrintLineF(ERRMSG_SENSOR_GET_METER_CONTROL);
        logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_GET_METER_CONTROL);
        setState(false);
    }

    if(!mc.nrdy)
    {
        DebugPrintLineF(ERRMSG_SENSOR_NRDY);
        logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_NRDY);
        setState(false);
    }

    uint16_t nos;
    if(!sunrise.GetNumberOfSamplesEE(nos))
    {
        DebugPrintLineF(ERRMSG_SENSOR_NUMBER_OF_SAMPLES);
        logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_NUMBER_OF_SAMPLES);
        setState(false);
    }

    estimatedMeasurementDuration = nos * SAMPLE_DURATION;

    sunrise.Sleep();
}

bool readCO2(void)
{
    // Wake up the sensor.
    sunrise.Awake();
    DebugPrintLineF("Measuring...");

    uint16_t hourCount = (uint16_t)((now.unixtime() - start.unixtime()) / (uint32_t)SECONDS_PER_HOUR);

    // Single measurement mode requires host device to increment ABC time between sleeps.
    sunrise.SetABCTime(hourCount);
    DebugPrintF("Setting ABC time:  "); DebugPrintLine(hourCount);

    // Reset ISR variable and start new measurement.
    isReady = false;
    if(!sunrise.StartSingleMeasurement())
    {
        DebugPrintLineF(ERRMSG_SENSOR_START_MEASRUEENT);
        logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_START_MEASRUEENT);
        return false;
    }

    // Wait for the sensor to complate the measurement and signal the ISR (or timeout).
    unsigned long int measurementStartTime = millis();
    if(!awaitISR(estimatedMeasurementDuration))
    {
        DebugPrintLineF(ERRMSG_SENSOR_ISR_TIMEOUT);
        logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_ISR_TIMEOUT);
        return false;
    }

    unsigned long int measurementDuration = millis() - measurementStartTime;
    DebugPrintF("Duration:          "); DebugPrintLine(measurementDuration);

    // Read and print measurement values.
    if(!sunrise.ReadMeasurement())
    {
        DebugPrintLineF(ERRMSG_SENSOR_READ_MEASUREMENT);
        logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_READ_MEASUREMENT);
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
    #ifdef SD_ENABLE
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
    #endif
    return true;
}

bool writeBinaryFile(const char* filepath, const payload_t& content)
{
    #ifdef SD_ENABLE
    File file = sd.open(filepath, FILE_WRITE);
    if(!file)
    {
        return false;
    }

    file.write(&content, sizeof(content));
    file.flush();
    file.close();
    #endif
    return true;
}

bool readBinaryFile(const char* filepath, payload_t* result, size_t* resultCount, size_t count, size_t index)
{
    #ifdef SD_ENABLE
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
    #endif
    return true;
}

bool logError(logtype_t type, const char* msg)
{
    #ifdef SD_ENABLE
    File file = sd.open(ERRORFILE, FILE_WRITE);
    if(!file)
    {
        return false;
    }

    switch(type)
    {
        case LT_ERROR:
            DebugPrintF("ERROR: ");
            break;
        case LT_WARNING:
            DebugPrintF("WARNING: ");
            break;
        default:
            break;
    }

    file.print(msg);
    file.println();
    file.flush();

    file.close();
    #endif
    return true;
}

void setState(bool status, bool halt)
{
    if(status)
    {
        digitalWrite(PIN_ERRORLED, LOW);
        digitalWrite(PIN_OKLED, HIGH);
    }
    else
    {
        digitalWrite(PIN_ERRORLED, HIGH);
        digitalWrite(PIN_OKLED, LOW);

        while(halt)
        {
            yield();
        }
    }
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
            DebugPrintLine(ERRMSG_SENSOR_GET_MEASUREMENT_MODE);
            logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_GET_MEASUREMENT_MODE);
            setState(false);
        }

        if(measurementMode == mode)
        {
            break;
        }

        DebugPrintLineF("Attempting to switch measurement mode...");
        if(!sunrise.SetMeasurementModeEE(mode))
        {
            DebugPrintLine(ERRMSG_SENSOR_SET_MEASUREMENT_MODE);
            logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_SET_MEASUREMENT_MODE);
            setState(false);
        }

        if(!sunrise.HardRestart())
        {
            DebugPrintLine(ERRMSG_SENSOR_RESTART);
            logError(logtype_t::LT_ERROR, ERRMSG_SENSOR_RESTART);
            setState(false);
        }
    }
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

void fileTimestamp(uint16_t* date, uint16_t* time)
{
    #ifdef SD_ENABLE
    now = rtc.now();
    *date = FAT_DATE(now.year(), now.month(), now.day());
    *time = FAT_TIME(now.hour(), now.minute(), now.second());
    #endif
}
