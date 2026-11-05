#include "Api.h"
static Pair pair = { 17 };
Pair* make_pair(void) { return &pair; }
int read_pair(Pair* value) { return value->value; }
