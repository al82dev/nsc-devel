typedef int (*Callback)(int value);
int apply_callback(Callback callback, int value);
int apply_callback_ptr(Callback *callback, int value);
