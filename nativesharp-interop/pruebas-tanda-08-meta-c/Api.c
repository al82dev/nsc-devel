#include "Api.h"
static CRecord record = { 17, CGlobalChoice };
CRecord* get_c_record(void) { return &record; }
CRecord get_c_record_value(void) { return record; }
CBig get_c_big(void) { CBig value = { 11, 22, 33 }; return value; }
CBig echo_c_big(CBig value) { value.second += 5; return value; }
enum CGlobalKind get_c_kind(void) { return record.kind; }
int get_c_number(void) { return record.value; }
int shared_c_number = 17;
int get_shared_c_number(void) { return shared_c_number; }
