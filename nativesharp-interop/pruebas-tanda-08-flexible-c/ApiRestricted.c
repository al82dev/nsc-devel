#include "Restricted.h"
static struct { int n; int data[2]; } const_storage = { 2, { 123, 456 } };
static struct { int n; volatile int data[2]; } volatile_storage = { 2, { 0, 0 } };
static long long packed_storage[2];
FlexConst* flex_const(void) { return (FlexConst*)&const_storage; }
FlexVolatile* flex_volatile(void) { return (FlexVolatile*)&volatile_storage; }
int flex_volatile_get(const FlexVolatile* value, int index) { return value->data[index]; }
void flex_volatile_set(FlexVolatile* value, int index, int element) { value->data[index] = element; }
FlexPacked* flex_packed(void) { return (FlexPacked*)packed_storage; }
FlexPacked flex_packed_value(void) { FlexPacked value = { 7 }; return value; }
int flex_packed_take(FlexPacked value) { return value.tag; }
int flex_packed_get(const FlexPacked* value, int index) { return value->data[index]; }
void flex_packed_set(FlexPacked* value, int index, int element) { value->data[index] = element; }
