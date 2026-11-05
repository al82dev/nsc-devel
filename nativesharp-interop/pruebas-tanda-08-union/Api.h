typedef union Value { int integer; unsigned char bytes[4]; } Value;
Value* get_value(void);
int first_byte(Value* value);
