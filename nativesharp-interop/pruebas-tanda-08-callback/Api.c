#include "Api.h"
int apply_callback(Callback callback, int value) { return callback(value); }
int apply_callback_ptr(Callback *callback, int value) { return (*callback)(value); }
