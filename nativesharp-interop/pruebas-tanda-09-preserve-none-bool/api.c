#include "api.h"
_Bool __attribute__((preserve_none)) t09_most(_Bool x) { return !x; }
_Bool __attribute__((preserve_none)) t09_all(_Bool x) { return !x; }
T09MostCallback t09_get_most(void) { return t09_most; }
T09AllCallback t09_get_all(void) { return t09_all; }
