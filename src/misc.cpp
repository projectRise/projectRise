#include "misc.hpp"
#include <errno.h>
#include <RTClib.h>

extern unsigned long sleepDuration;
extern RTC_DS3231 rtc;

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
