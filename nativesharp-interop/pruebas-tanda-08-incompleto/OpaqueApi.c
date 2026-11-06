#include "OpaqueApi.h"
struct Opaque { int value; };
static Opaque object = { 17 };
Opaque *opaque_current = &object;
Opaque **opaque_slot = &opaque_current;
Opaque *opaque_get(void) { return &object; }
int opaque_read(Opaque *value) { return value->value; }
void opaque_set(Opaque *value, int number) { value->value = number; }
union OpaqueUnion { int value; unsigned int raw; };
static OpaqueUnion union_object = { .value = 8 };
OpaqueUnion *union_current = &union_object;
OpaqueUnion *union_get(void) { return &union_object; }
int union_read(OpaqueUnion *value) { return value->value; }
void union_set(OpaqueUnion *value, int number) { value->value = number; }
