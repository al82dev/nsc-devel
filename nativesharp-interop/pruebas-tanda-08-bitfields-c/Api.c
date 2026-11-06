#include "Api.h"
static Bits bits;
static PackedBits packed;
Bits* get_bits(void) { return &bits; }
PackedBits* get_packed_bits(void) { return &packed; }
void init_bits(Bits* value, PackedBits* p) {
    value->first = 5; value->delta = -3; value->second = 37; value->third = 9;
    p->prefix = 11; p->small = 12; p->negative = -4;
}
int check_bits(Bits* value, PackedBits* p) {
    return value->first == 6 && value->delta == -7 && value->second == 31 &&
        value->third == 10 && p->prefix == 11 && p->small == 7 && p->negative == -2;
}
int bits_size(void) { return sizeof(Bits); }
int packed_bits_size(void) { return sizeof(PackedBits); }
