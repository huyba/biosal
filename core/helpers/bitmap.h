
#ifndef CORE_BITMAP_H
#define CORE_BITMAP_H

#include <stdint.h>

#define CORE_BIT_ZERO 0
#define CORE_BIT_ONE 1

#define CORE_BITS_PER_BYTE 8

int core_bitmap_get_bit_uint8_t(uint8_t *self, int index);
void core_bitmap_set_bit_value_uint8_t(uint8_t *self, int index, int value);

int core_bitmap_get_bit_uint32_t(uint32_t *self, int index);
void core_bitmap_set_bit_value_uint32_t(uint32_t *self, int index, int value);

void core_bitmap_set_bit_uint32_t(uint32_t *self, int index);
void core_bitmap_clear_bit_uint32_t(uint32_t *self, int index);

int core_bitmap_get_bit_uint64_t(uint64_t *self, int index);
void core_bitmap_set_bit_value_uint64_t(uint64_t *self, int index, int value);

#endif
