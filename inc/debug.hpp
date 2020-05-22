#ifndef __DEBUG_HPP__
#define __DEBUG_HPP__

#include <Arduino.h>

#define NOP ((void)0)

#ifdef __DEBUG__
#define DebugPrint(...)         Serial.print(__VA_ARGS__)
#define DebugPrintLine(...)     Serial.println(__VA_ARGS__)
#define DebugPrintF(...)        Serial.print(F(__VA_ARGS__))
#define DebugPrintLineF(...)    Serial.println(F(__VA_ARGS__))
#define DebugWrite(...)         Serial.write(__VA_ARGS__)
#define DebugFlush()            Serial.flush()
#else
#define DebugPrint(...)         NOP
#define DebugPrintLine(...)     NOP
#define DebugPrintF(...)        NOP
#define DebugPrintLineF(...)    NOP
#define DebugWrite(...)         NOP
#define DebugFlush()            NOP
#endif

#endif // __DEBUG_HPP__
