#include "Api.h"
static LAnonymous value;
static LPackedAnonymous packed;
LAnonymous* probe_anonymous(void) { return &value; }
LPackedAnonymous* probe_packed_anonymous(void) { return &packed; }
int probe_anonymous_size(void) { return (int)sizeof(LAnonymous); }
int probe_packed_anonymous_size(void) { return (int)sizeof(LPackedAnonymous); }
int probe_anonymous_check(void) {
    return value.tag == 1 && value.i == 2 && value.x == 3 && value.y == 4 && value.deep == 5 &&
        value.low == 3 && value.high == 17 && packed.tag == 6 && packed.packedInt == 7 &&
        packed.packedShort == 8 && packed.packedBits == 5 && packed.packedMore == 19;
}
void probe_anonymous_set_bits(void) { value.low = 2; value.high = 11; packed.packedBits = 6; packed.packedMore = 23; }
