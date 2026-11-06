#include "UnionApi.h"
static UnionBits bits;
UnionBits* get_union_bits(void) { return &bits; }
void init_union_bits(UnionBits* value) { value->raw = 0x1234561dU; }
int check_union_bits(UnionBits* value) { return value->raw == 0x1234561bU; }
