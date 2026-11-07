#include "Nested.h"
static CNestAnonField anon_value = {43, CNestHigh};
static CNestNamedField named_value = {44, CNestNamedB};
CNestAnonField* get_nest_anon(void) { return &anon_value; }
CNestNamedField* get_nest_named(void) { return &named_value; }
