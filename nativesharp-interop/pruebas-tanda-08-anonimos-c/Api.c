#include "Api.h"
static LAnonymous value;
LAnonymous* probe_anonymous(void) { return &value; }
int probe_anonymous_size(void) { return (int)sizeof(LAnonymous); }
int probe_anonymous_check(void) { return value.tag == 1 && value.i == 2 && value.x == 3 && value.y == 4; }
