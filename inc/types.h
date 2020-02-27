#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

typedef float single_t;

typedef struct __attribute__((packed))
{
    uint32_t timestamp;
    uint8_t count : 7;
    uint8_t rts : 1;
} header_t; // 40 bits / 5 bytes

typedef struct __attribute__((packed))
{
    uint16_t type : 6;
    uint16_t value : 10;
} sensordata_t; // 16 bits / 2 bytes

typedef struct __attribute__((packed))
{
    uint8_t type : 6;
    //uint8_t _pad : 2;
    single_t value;
} sensordata2_t; // 40 bits / 5 bytes

typedef struct __attribute__((packed))
{
    header_t header;
    sensordata2_t data[5];
} collection_t;
// 40 + (40 * n) bits / (5 + (5 * n)) bytes (240 bits / 30 bytes)

#endif // __TYPES_H__
