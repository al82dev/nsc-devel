#include "MoreQualified.h"
float probe_read_float(void) { return more_float_volatile[1]; }
void probe_write_float(float value) { more_float_volatile[1] = value; }
long probe_read_long(void) { return more_long_volatile[1]; }
void probe_write_long(long value) { more_long_volatile[1] = value; }
unsigned long probe_read_ulong(void) { return more_ulong_volatile[1]; }
void probe_write_ulong(unsigned long value) { more_ulong_volatile[1] = value; }
