#include "Anon.h"
static CAnonRecord anon_record = { 41 };
static CAnonLevelRecord anon_level_record = { 43, CAnonLevelHigh };
CAnonRecord* get_anon_record(void) { return &anon_record; }
CAnonKind get_anon_kind(void) { return CAnonKindSecond; }
CAnonLevelRecord* get_anon_level_record(void) { return &anon_level_record; }
