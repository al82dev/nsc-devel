#include "Api.h"
volatile float volatile_f32 = 0.0f;
volatile double volatile_f64 = 0.0;
int check_volatile_floats(void)
{
    if (volatile_f32 != 1.5f || volatile_f64 != 2.5) return 0;
    volatile_f32 = 3.5f;
    volatile_f64 = 4.5;
    return 1;
}
static Pair pair = { 17 };
Pair* make_pair(void) { return &pair; }
int read_pair(Pair* value) { return value->value; }
static struct Parent parent = {{ 1 }, {{ 2 }, { 3 }}};
struct Parent* make_parent(void) { return &parent; }
int read_parent(struct Parent* value) { return value->child.value + value->children[1].value; }
void add_distinct(int* restrict destination, const int* restrict source) { *destination += *source; }
int bump_volatile(volatile int* value) { *value += 2; return *value; }
int read_const_volatile(const volatile int* value) { return *value; }
int read_volatile_void(volatile void* value) { return *(volatile int*)value; }
static struct Device device = { 7 };
struct Device* get_device(void) { return &device; }
int read_device(struct Device* value) { return value->counter; }
int shared_value = 7;
int read_shared_value(void) { return shared_value; }
volatile int shared_volatile = 7;
volatile signed char volatile_i8 = 0;
volatile unsigned char volatile_u8 = 0;
volatile short volatile_i16 = 0;
volatile unsigned short volatile_u16 = 0;
volatile long long volatile_i64 = 0;
volatile unsigned long long volatile_u64 = 0;
int check_volatile_integers(void)
{
    return volatile_i8 == 12 && volatile_u8 == 13 && volatile_i16 == 1200 &&
        volatile_u16 == 1300 && volatile_i64 == 120000 && volatile_u64 == 130000;
}
static volatile int pointee_storage = 7;
static volatile int pointer_array_storage = 11;
volatile int* volatile_pointer_array[2] = { &pointee_storage, &pointer_array_storage };
volatile int* volatile_pointer_matrix[2][2] = {
    { &pointee_storage, &pointer_array_storage }, { &pointer_array_storage, &pointee_storage }
};
const volatile int* const_volatile_pointer_array[2] = { &pointee_storage, &pointer_array_storage };
volatile int* const fixed_volatile_pointer_array[2] = { &pointer_array_storage, &pointee_storage };
int read_volatile_pointer_array(void) { return *volatile_pointer_array[0]; }
volatile int* volatile_pointee = &pointee_storage;
const volatile int* const_volatile_pointee = &pointee_storage;
volatile int* const fixed_volatile_pointee = &pointee_storage;
int read_volatile_pointee(void) { return pointee_storage; }
volatile int* get_volatile_pointee(void) { return &pointee_storage; }
int read_plain_int(int* value) { return *value; }
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
static volatile struct GlobalOnly volatile_record_storage = { 9 };
volatile struct GlobalOnly* volatile_record_pointer_array[2] = { &volatile_record_storage, &volatile_record_storage };
volatile struct GlobalOnly* volatile_record_target = &volatile_record_storage;
int read_volatile_record_target(void) { return volatile_record_storage.value; }
volatile struct GlobalOnly* get_volatile_record_target(void) { return &volatile_record_storage; }
int bump_volatile_record(volatile struct GlobalOnly* value) { value->value += 2; return value->value; }
int read_const_volatile_record(const volatile struct GlobalOnly* value) { return value->value; }
static struct GlobalOnly global_record_target = { 2 };
struct GlobalOnly* global_record_pointer = &global_record_target;
struct GlobalOnly** global_record_pointer_pointer = &global_record_pointer;
struct GlobalOnly global_record_array[2] = { { 3 }, { 4 } };
struct GlobalOnly global_record_matrix[2][2] = { { { 1 }, { 2 } }, { { 3 }, { 4 } } };
union GlobalUnion global_union = { 0 };
const struct GlobalOnly global_record_const = { 7 };
const struct GlobalOnly* readonly_record_pointer = &global_record_const;
int read_readonly_record_pointer(void) { return readonly_record_pointer->value; }
int read_const_globalonly(const struct GlobalOnly* value) { return value->value; }
const struct GlobalOnly* get_const_globalonly(void) { return &global_record_const; }
static const int const_scalar_value = 27;
const int* get_const_scalar(void) { return &const_scalar_value; }
void write_mutable_globalonly(struct GlobalOnly* value) { value->value = 42; }
volatile struct GlobalOnly global_record_volatile = { 8 };
struct GlobalOnly* volatile volatile_record_pointer = &global_record_target;
int check_volatile_record_pointer(void)
{
    struct GlobalOnly* value = volatile_record_pointer;
    volatile_record_pointer = value;
    return value == global_record_pointer;
}
const struct GlobalOnly readonly_record_matrix[2][2] = { { { 11 }, { 19 } }, { { 23 }, { 31 } } };
volatile struct GlobalOnly volatile_record_matrix[2][2] = { { { 1 }, { 2 } }, { { 3 }, { 4 } } };
int read_volatile_record_matrix(void) { return volatile_record_matrix[1][1].value; }
static int nested_target = 0;
volatile struct GlobalNested volatile_nested_record = { { 0 }, &nested_target };
volatile struct GlobalNested* volatile_nested_pointer = &volatile_nested_record;
int read_volatile_nested_record(void) { return volatile_nested_record.item.value + *volatile_nested_record.target; }
int read_volatile_record(void) { return global_record_volatile.value; }
int read_global_records(void) { return global_record.value + global_record_pointer->value + global_record_array[1].value + global_record_matrix[1][1].value + global_union.value; }
