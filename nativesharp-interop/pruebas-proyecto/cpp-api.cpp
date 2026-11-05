#include "cpp-api.h"
namespace tools {
int shared = 0;
volatile int flagged = 0;
const int limit = 11;
const volatile int status = 3;
static int pointer_storage = 10;
int* pointer = &pointer_storage;
int* const fixed_pointer = &pointer_storage;
int* volatile volatile_pointer = &pointer_storage;
static const int readonly_storage = 21;
const int* readonly_pointee = &readonly_storage;
int read_const(const int* value) { return *value; }
int read_mutable(int* value) { return *value; }
int read_pointer_value() { return pointer_storage; }
int read_shared() { return shared; }
int read_flagged() { return flagged; }
int blend(int value) { return value * 11; }
double blend(double value) { return value * 2.0; }
}
