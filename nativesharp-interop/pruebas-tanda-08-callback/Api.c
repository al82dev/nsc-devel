#include "Api.h"
int apply_callback(Callback callback, int value) { return callback(value); }
int apply_callback_ptr(Callback *callback, int value) { return (*callback)(value); }
static int add_three(int value) { return value + 3; }
Callback get_callback(void) { return add_three; }
Callback echo_callback(Callback callback) { return callback; }
static CallbackHolder holder = { add_three };
CallbackHolder* get_callback_holder(void) { return &holder; }
Callback shared_callback = add_three;
FloatValue add_float(FloatValue a, FloatValue b) { return a + b; }
DoubleValue add_double(DoubleValue a, DoubleValue b) { return a + b; }
int mix_integers(SignedByte a, UnsignedShort b, UnsignedInt c) { return (int)a + (int)b + (int)c; }
