#include "Api.h"
static CRecord record = { 17 };
CRecord* get_c_record(void) { return &record; }
int get_c_number(void) { return record.value; }
int shared_c_number = 17;
int get_shared_c_number(void) { return shared_c_number; }
