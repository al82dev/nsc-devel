#include "Api.h"
static int calls;
void reset_calls(void) { calls = 0; }
int read_calls(void) { return calls; }
void* identity_pointer(void* value) { ++calls; return value; }
int no_arguments(void) { return 17; }
int old_style(int value, double fraction) { return value + (int)(fraction * 10); }
int old_style_noargs(void) { return 23; }
int refined(int value) { return value + 11; }
int refined_zero(void) { return 29; }
int refined_reverse(int value) { return value + 7; }
