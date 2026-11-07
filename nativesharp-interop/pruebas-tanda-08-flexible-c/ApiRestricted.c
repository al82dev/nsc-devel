#include "Restricted.h"
static long long const_storage[2];
static long long packed_storage[2];
FlexConst* flex_const(void) { return (FlexConst*)const_storage; }
FlexPacked* flex_packed(void) { return (FlexPacked*)packed_storage; }
FlexPacked flex_packed_value(void) { FlexPacked value = { 7 }; return value; }
