#include "Api.h"
static CRecord record = { 17, CGlobalChoice };
CRecord* get_c_record(void) { return &record; }
CRecord get_c_record_value(void) { return record; }
enum CGlobalKind get_c_kind(void) { return record.kind; }
int get_c_number(void) { return record.value; }
int shared_c_number = 17;
int get_shared_c_number(void) { return shared_c_number; }
