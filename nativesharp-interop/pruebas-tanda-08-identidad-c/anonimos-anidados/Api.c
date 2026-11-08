#include "Api.h"
static AnonOuter instance;
AnonOuter* get_anon(void) { return &instance; }
int read_anon(void) { return (int)instance.mode + instance.value; }
