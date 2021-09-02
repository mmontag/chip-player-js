#pragma once
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

inline int16_t toSint16LE(const uint8_t *arr)
{
    int16_t num = *reinterpret_cast<const int8_t *>(&arr[1]);
    num *= 1 << 8;
    num |= static_cast<int16_t>(arr[0]) & 0x00FF;
    return num;
}

inline uint16_t toUint16LE(const uint8_t *arr)
{
    uint16_t num = arr[0];
    num |= ((arr[1] << 8) & 0xFF00);
    return num;
}

inline uint32_t toUint32LE(const uint8_t *arr)
{
    uint32_t num = arr[0];
    num |= (static_cast<uint32_t>(arr[1] << 8)  & 0x0000FF00);
    num |= (static_cast<uint32_t>(arr[2] << 16) & 0x00FF0000);
    num |= (static_cast<uint32_t>(arr[3] << 24) & 0xFF000000);
    return num;
}

inline int32_t toSint32LE(const uint8_t *arr)
{
    int32_t num = *reinterpret_cast<const int8_t *>(&arr[3]);
    num *= 1 << 24;
    num |= (static_cast<int32_t>(arr[2]) << 16) & 0x00FF0000;
    num |= (static_cast<int32_t>(arr[1]) << 8) & 0x0000FF00;
    num |= static_cast<int32_t>(arr[0]) & 0x000000FF;
    return num;
}

#endif // COMMON_H
