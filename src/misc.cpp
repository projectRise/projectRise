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

void testReadFromFile(const char* filename, payload_t* const results, const size_t resultCount, const size_t iterCount, const size_t offset)
{
    size_t readCount = 0U;
    const size_t max = offset + (resultCount * iterCount);
    for(size_t i = offset; i < max; i += resultCount)
    {
        if(!readBinaryFile(filename, results, &readCount, resultCount, i))
        {
            DebugPrint("Error: Could not read from file \'"); DebugPrint(filename); DebugPrintLine("\'");
            break;
        }

        for(size_t j = 0U; j < resultCount; j++)
        {
            DebugPrint("VALUE["); DebugPrint(i + j); DebugPrintLine("]: ");
            printSensorValues(results[j]);
        }
    }
}
