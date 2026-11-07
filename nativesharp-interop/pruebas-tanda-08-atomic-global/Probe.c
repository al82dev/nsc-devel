#include "Api.h"
int c_probe_read(void) { return atomic_global; }
int c_probe_add(void) { return atomic_global += 2; }
