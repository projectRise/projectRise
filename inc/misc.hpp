#ifndef __MISC_HPP__
#define __MISC_HPP__

#include "CommandHandler.hpp"
#include "types.h"

#define CMD_INIT    "$$$"
#define CMD_OK      "OK"
#define CMD_FAIL    "FAIL"
#define CMD_UNKNOWN "UNKNOWN"
#define CMD_ABORT   "ABORT"
#define CMD_SD      "SD"
#define CMD_DT      "DT"

bool handleCommand(CommandHandler* self, const char* cmd, const char* args);
void testReadFromFile(const char* filename);
void printSensorValues(const payload_t& content);

#endif  // __MISC_HPP__
