#include "Api.h"
int apply_callback(Callback callback, int value) { return callback(value); }
int apply_callback_ptr(Callback *callback, int value) { return (*callback)(value); }
FloatValue add_float(FloatValue a, FloatValue b) { return a + b; }
DoubleValue add_double(DoubleValue a, DoubleValue b) { return a + b; }
