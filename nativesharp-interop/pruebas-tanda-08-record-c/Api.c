#include "Api.h"
static Pair pair = { 17 };
Pair* make_pair(void) { return &pair; }
int read_pair(Pair* value) { return value->value; }
static struct Parent parent = {{ 1 }, {{ 2 }, { 3 }}};
struct Parent* make_parent(void) { return &parent; }
int read_parent(struct Parent* value) { return value->child.value + value->children[1].value; }
void add_distinct(int* restrict destination, const int* restrict source) { *destination += *source; }
static struct Device device = { 7 };
struct Device* get_device(void) { return &device; }
int read_device(struct Device* value) { return value->counter; }
int shared_value = 7;
int read_shared_value(void) { return shared_value; }
volatile int shared_volatile = 7;
int read_shared_volatile(void) { return shared_volatile; }
int shared_array[3] = { 1, 2, 3 };
int shared_matrix[2][3] = { { 4, 5, 6 }, { 7, 8, 9 } };
int read_shared_arrays(void) { return shared_array[1] + shared_matrix[1][2]; }
const int readonly_array[3] = { 5, 17, 29 };
int read_readonly_array(void) { return readonly_array[1]; }
volatile int volatile_array[3] = { 1, 2, 3 };
volatile int volatile_matrix[2][2] = { { 1, 2 }, { 3, 4 } };
int read_volatile_arrays(void) { return volatile_array[1] + volatile_matrix[1][1]; }
struct GlobalOnly global_record = { 1 };
static struct GlobalOnly global_record_target = { 2 };
struct GlobalOnly* global_record_pointer = &global_record_target;
struct GlobalOnly** global_record_pointer_pointer = &global_record_pointer;
struct GlobalOnly global_record_array[2] = { { 3 }, { 4 } };
struct GlobalOnly global_record_matrix[2][2] = { { { 1 }, { 2 } }, { { 3 }, { 4 } } };
union GlobalUnion global_union = { 0 };
const struct GlobalOnly global_record_const = { 7 };
volatile struct GlobalOnly global_record_volatile = { 8 };
struct GlobalOnly* volatile volatile_record_pointer = &global_record_target;
int read_volatile_record(void) { return global_record_volatile.value; }
int read_global_records(void) { return global_record.value + global_record_pointer->value + global_record_array[1].value + global_record_matrix[1][1].value + global_union.value; }
