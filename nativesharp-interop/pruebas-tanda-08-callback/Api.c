#include "Api.h"
int apply_callback(Callback callback, int value) { return callback(value); }
int apply_callback_ptr(Callback *callback, int value) { return (*callback)(value); }
static int add_three(int value) { return value + 3; }
Callback get_callback(void) { return add_three; }
Callback echo_callback(Callback callback) { return callback; }
static CallbackHolder holder = { add_three, add_three };
CallbackHolder* get_callback_holder(void) { return &holder; }
Callback shared_callback = add_three;
Callback const readonly_callback = add_three;
int run_holder(CallbackHolder* item, int value) { return item->run(value); }
int run_shared(int value) { return shared_callback(value); }
static VolatileCallbackHolder volatile_holder = { add_three };
static PackedVolatileCallbackHolder packed_volatile_holder = { 0, add_three };
Callback volatile volatile_callback = add_three;
VolatileCallbackHolder* get_volatile_holder(void) { return &volatile_holder; }
PackedVolatileCallbackHolder* get_packed_volatile_holder(void) { return &packed_volatile_holder; }
int run_volatile_holder(VolatileCallbackHolder* item, int value) { return item->run(value); }
int run_volatile_global(int value) { return volatile_callback(value); }
void set_volatile_holder(VolatileCallbackHolder* item, Callback value) { item->run = value; }
void set_volatile_global(Callback value) { volatile_callback = value; }
int run_packed_volatile(PackedVolatileCallbackHolder* item, int value) { return item->run(value); }
void set_packed_volatile(PackedVolatileCallbackHolder* item, Callback value) { item->run = value; }
FloatValue add_float(FloatValue a, FloatValue b) { return a + b; }
DoubleValue add_double(DoubleValue a, DoubleValue b) { return a + b; }
int mix_integers(SignedByte a, UnsignedShort b, UnsignedInt c) { return (int)a + (int)b + (int)c; }
static int add_payload(CallbackPayload* value) { return value->value + 3; }
static RecordCallbackHolder record_callback_holder = { add_payload };
RecordCallback record_callback_global = add_payload;
RecordCallback get_record_callback(void) { return add_payload; }
RecordCallbackHolder* get_record_callback_holder(void) { return &record_callback_holder; }
