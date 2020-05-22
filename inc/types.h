#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint32_t timestamp;
    uint16_t co2;
    uint16_t error;
    uint16_t battery;
} payload_t;

#endif // __TYPES_H__
