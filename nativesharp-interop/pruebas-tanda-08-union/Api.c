#include "Api.h"
static Value value;
Value* get_value(void) { return &value; }
int first_byte(Value* item) { return item->bytes[0]; }
void set_integer(Value* item, int number) { item->integer = number; }
int get_integer(Value* item) { return item->integer; }
Value make_value(int number) { Value result; result.integer = number; return result; }
int value_integer(Value item) { return item.integer; }
BigValue make_big(void) { BigValue result = { { 7, 11, 13 } }; return result; }
int big_sum(BigValue item) { return item.words[0] + item.words[1] + item.words[2]; }
FloatValue make_float_value(void) { FloatValue result; result.number = 1.5; return result; }
int check_float_value(FloatValue item) { return item.number == 1.5; }
PointValue make_point_value(void) { PointValue result; result.point.x = 17; result.point.y = 25; return result; }
int point_sum(PointValue item) { return item.point.x + item.point.y; }
