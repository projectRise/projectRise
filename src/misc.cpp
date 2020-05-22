#include "misc.hpp"
#include <errno.h>
#include <RTClib.h>
#include "debug.hpp"

extern unsigned long sleepDuration;
extern RTC_DS3231 rtc;
extern bool readBinaryFile(const char* filepath, payload_t* result, size_t* resultCount, size_t count, size_t index = 0U);

bool handleCommand(CommandHandler* self, const char* cmd, const char* args)
{
    static bool mode = false;

    if(cmd == nullptr)
    {
        if(!mode)
        {
            return false;
        }
        else if(args != nullptr)
        {
            self->SendLine(CMD_ABORT);
            return false;
        }

        return true;
    }

    if(strcmp(cmd, CMD_INIT) == 0)
    {
        mode = !mode;
        self->SendLine(CMD_OK);
        return mode;
    }

    if(!mode)
    {
        return false;
    }
    else if(strcmp(cmd, CMD_SD) == 0)
    {
        if(args == nullptr)
        {
            self->SendLine(CMD_FAIL);
            return true;
        }

        errno = 0;
        unsigned long tmp = strtoul(args, nullptr, 10);
        if(tmp == 0UL || errno != 0)
        {
            self->SendLine(CMD_FAIL);
            return true;
        }

        sleepDuration = tmp;
    }
    else if(strcmp(cmd, CMD_DT) == 0)
    {
        if(args == nullptr)
        {
            self->SendLine(CMD_FAIL);
            return true;
        }

        errno = 0;
        uint32_t tmp = strtoul(args, nullptr, 10);
        if(tmp == 0UL || errno != 0)
        {
            self->SendLine(CMD_FAIL);
            return true;
        }

        DateTime tmpTime = DateTime(tmp);
        rtc.adjust(tmpTime);
        self->Send(CMD_OK " ");
        self->SendLine(tmpTime.unixtime());
    }
    else
    {
        self->SendLine(CMD_UNKNOWN);
    }

    return true;
}

void printSensorValues(const payload_t& content)
{
    DebugPrint(content.timestamp);  DebugPrint(";");
    DebugPrint(content.co2);        DebugPrint(";");
    DebugPrint(content.battery, 2); //DebugPrint(";");
    DebugPrintLine();
    DebugFlush();
}

#define TEST_READOFFSET     0U                                          // Position in the file to start reading (should be even divisible by size of 'payload_t').
#define TEST_ELEMENTCOUNT   2U                                          // How many 'payload_t' to request per read (a.k.a. 'payload_t' count of the buffer).
#define TEST_READCOUNT      5U                                          // Total number of read operations.
#define TEST_TOTALCOUNT     ((TEST_ELEMENTCOUNT) * (TEST_READCOUNT))    // The total number of 'payload_t' values to read.
void testReadFromFile(const char* filename)
{
    payload_t vals[TEST_ELEMENTCOUNT];
    size_t readCount = 0U;
    const size_t max = TEST_READOFFSET + TEST_TOTALCOUNT;
    for(size_t i = TEST_READOFFSET; i < max; i += TEST_ELEMENTCOUNT)
    {
        if(!readBinaryFile(filename, vals, &readCount, TEST_ELEMENTCOUNT, i))
        {
            DebugPrint("Error: Could not read from file \'"); DebugPrint(filename); DebugPrintLine("\'");
            break;
        }

        for(size_t j = 0U; j < TEST_ELEMENTCOUNT; j++)
        {
            DebugPrint("VALUE["); DebugPrint(i + j); DebugPrintLine("]: ");
            printSensorValues(vals[j]);
        }
    }
}
