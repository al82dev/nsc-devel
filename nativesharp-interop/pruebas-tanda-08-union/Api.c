#include "Api.h"
static Value value;
Value* get_value(void) { return &value; }
int first_byte(Value* item) { return item->bytes[0]; }
void set_integer(Value* item, int number) { item->integer = number; }
int get_integer(Value* item) { return item->integer; }
