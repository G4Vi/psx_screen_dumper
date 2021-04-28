
#pragma once

#include <stdint.h>

uint32_t crc32(const void * data, uint32_t len);

uint32_t crc32_frame(const uint16_t startindex, const uint16_t endindex, const uint16_t size, const void * data);