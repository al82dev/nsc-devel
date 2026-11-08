#include "fixture.h"
static struct COnlyRecord record = { 42 };
struct COnlyRecord *fixture_record(void) { return &record; }
