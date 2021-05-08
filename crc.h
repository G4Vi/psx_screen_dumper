
#pragma once

#include <stdint.h>

uint32_t crc32(const void * data, uint32_t len);

uint32_t crc32_frame(const uint8_t *startdata, const uint16_t startlen, const void * data, const uint16_t size);

uint32_t crc32_inner(uint32_t crc, const uint8_t *bytes, uint32_t len);