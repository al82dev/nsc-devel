#include "Api.h"
#include <float.h>
_Static_assert(sizeof(long double) == sizeof(double), "long double size differs from double");
_Static_assert(LDBL_MANT_DIG == DBL_MANT_DIG && LDBL_MAX_EXP == DBL_MAX_EXP && LDBL_MIN_EXP == DBL_MIN_EXP,
               "long double format differs from double");
static CFloatRecord record = { 1.25L };
CLongDouble add_long_double(CLongDouble a, CLongDouble b) { return a + b; }
CFloatRecord* get_float_record(void) { return &record; }
int check_float_record(CFloatRecord* item) { return item->value == 3.75L; }
