#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

inline uint16_t toUint16BE(const uint8_t *arr)
{
    uint16_t num = arr[1];
    num |= ((arr[0] << 8) & 0xFF00);
    return num;
}

inline int16_t toSint16BE(const uint8_t *arr)
{
    int16_t num = *reinterpret_cast<const int8_t *>(&arr[0]);
    num *= 1 << 8;
    num |= arr[1];
    return num;
}

inline uint16_t toUint16LE(const uint8_t *arr)
{
    uint16_t num = arr[0];
    num |= ((arr[1] << 8) & 0xFF00);
    return num;
}

#endif // COMMON_H
