#include "OpaqueApi.h"
struct Opaque { int value; };
static Opaque object = { 17 };
Opaque *opaque_current = &object;
Opaque *opaque_get(void) { return &object; }
int opaque_read(Opaque *value) { return value->value; }
void opaque_set(Opaque *value, int number) { value->value = number; }
