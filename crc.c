
#include "crc.h"

// Adapted from https://stackoverflow.com/a/15031244/4454028

uint32_t crc32(const void * data, uint32_t len) {
	const uint8_t * bytes = (const uint8_t *) data;
	uint32_t crc = 0xFFFFFFFF;

    while (len) {
        crc ^= *bytes;

        for (int k = 0; k < 8; k++) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xEDB88320;
			} else {
				crc = (crc >> 1);
			}
        }

		bytes++;
		len--;
    }

    return ~crc;
}

static inline uint32_t crc32_inner(uint32_t crc, const uint8_t *bytes, uint32_t len)
{
	 while (len) {
        crc ^= *bytes;

        for (int k = 0; k < 8; k++) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xEDB88320;
			} else {
				crc = (crc >> 1);
			}
        }

		bytes++;
		len--;
    }
	return crc;
}

uint32_t crc32_frame(const uint16_t startindex, const uint16_t endindex, const uint16_t size, const void * data)
{
	uint32_t crc = 0xFFFFFFFF;
    uint8_t startdata[6];
	startdata[0] = startindex;
	startdata[1] = startindex >> 8;
	startdata[2] = endindex;
	startdata[3] = endindex >> 8;
	startdata[4] = size;
	startdata[5] = size >> 8;
	crc = crc32_inner(crc, startdata, sizeof(startdata));
	crc = crc32_inner(crc, data, size);
	return ~crc;
}