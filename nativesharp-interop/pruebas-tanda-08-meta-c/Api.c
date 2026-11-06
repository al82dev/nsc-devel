#include "Api.h"
_Static_assert(_Generic((enum CWideUnsigned)0, unsigned int: 1, default: 0), "enum must be unsigned");
static CRecord record = { 17, CGlobalChoice };
static CUnionShared shared_union = { 42 };
static int add_two(int value) { return value + 2; }
static CCallbackHolder callback_holder = { add_two };
CSharedCallback get_shared_callback(void) { return add_two; }
CCallbackHolder* get_callback_holder(void) { return &callback_holder; }
CUnionShared* get_shared_union(void) { return &shared_union; }
CUnionShared get_shared_union_value(void) { return shared_union; }
int read_shared_union(CUnionShared value) { return value.value; }
CRecord* get_c_record(void) { return &record; }
CRecord get_c_record_value(void) { return record; }
CBig get_c_big(void) { CBig value = { 11, 22, 33 }; return value; }
CBig echo_c_big(CBig value) { value.second += 5; return value; }
enum CGlobalKind get_c_kind(void) { return record.kind; }
int get_c_number(void) { return record.value; }
int shared_c_number = 17;
int get_shared_c_number(void) { return shared_c_number; }
enum CSignedSmall get_small(void) { return CNegative; }
enum CWideUnsigned get_high(void) { return CHigh; }
