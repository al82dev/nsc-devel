#include "Api.h"
static Value value;
Value* get_value(void) { return &value; }
int first_byte(Value* item) { return item->bytes[0]; }
