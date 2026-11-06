#include "VolatileApi.h"
static VolatileBits bits;
static PackedVolatileBits packed;
static VolatileUnionBits union_bits;
VolatileBits* get_volatile_bits(void) { return &bits; }
PackedVolatileBits* get_packed_volatile_bits(void) { return &packed; }
void init_volatile_bits(VolatileBits* value, PackedVolatileBits* p) {
    value->first = 5; value->delta = -3;
    p->prefix = 11; p->first = 12; p->delta = -4;
}
int check_volatile_bits(VolatileBits* value, PackedVolatileBits* p) {
    return value->first == 6 && value->delta == -7 && p->prefix == 11 &&
        p->first == 7 && p->delta == -2;
}
int read_volatile_first(VolatileBits* value) { return value->first; }
void write_volatile_first(VolatileBits* value, unsigned int number) { value->first = number; }
int read_packed_volatile_first(PackedVolatileBits* value) { return value->first; }
void write_packed_volatile_first(PackedVolatileBits* value, unsigned int number) { value->first = number; }
VolatileUnionBits* get_volatile_union_bits(void) { return &union_bits; }
void init_volatile_union_bits(VolatileUnionBits* value) { value->raw = 0x1234561dU; }
int check_volatile_union_bits(VolatileUnionBits* value) { return value->raw == 0x1234561bU; }
int read_union_volatile_first(VolatileUnionBits* value) { return value->first; }
void write_union_volatile_first(VolatileUnionBits* value, unsigned int number) { value->first = number; }
