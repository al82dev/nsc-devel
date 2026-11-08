#include "Restricted.h"
static long long const_storage[2];
static long long packed_storage[2];
FlexConst* flex_const(void) { return (FlexConst*)const_storage; }
FlexPacked* flex_packed(void) { return (FlexPacked*)packed_storage; }
FlexPacked flex_packed_value(void) { FlexPacked value = { 7 }; return value; }
int flex_packed_take(FlexPacked value) { return value.tag; }
int flex_packed_get(const FlexPacked* value, int index) { return value->data[index]; }
void flex_packed_set(FlexPacked* value, int index, int element) { value->data[index] = element; }
