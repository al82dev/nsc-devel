#include "Api.h"
static long long ints_storage[4];
static double doubles_storage[5];
static long long records_storage[6];
static int enums_storage[5];
static long long bools_storage[2];
long long flex_layout(int id)
{
    switch (id)
    {
    case 1: return (long long)sizeof(FlexInts);
    case 2: return (long long)_Alignof(FlexInts);
    case 3: return (long long)__builtin_offsetof(FlexInts, data);
    case 4: return (long long)sizeof(FlexDoubles);
    case 5: return (long long)_Alignof(FlexDoubles);
    case 6: return (long long)__builtin_offsetof(FlexDoubles, values);
    case 7: return (long long)sizeof(FlexRecords);
    case 8: return (long long)_Alignof(FlexRecords);
    case 9: return (long long)__builtin_offsetof(FlexRecords, items);
    case 10: return (long long)sizeof(FlexEnums);
    case 11: return (long long)_Alignof(FlexEnums);
    case 12: return (long long)__builtin_offsetof(FlexEnums, moods);
    case 13: return (long long)sizeof(FlexBools);
    case 14: return (long long)_Alignof(FlexBools);
    case 15: return (long long)__builtin_offsetof(FlexBools, flags);
    default: return -1;
    }
}
FlexInts* flex_ints(void) { return (FlexInts*)ints_storage; }
FlexDoubles* flex_doubles(void) { return (FlexDoubles*)doubles_storage; }
FlexRecords* flex_records(void) { return (FlexRecords*)records_storage; }
FlexEnums* flex_enums(void) { return (FlexEnums*)enums_storage; }
FlexBools* flex_bools(void) { return (FlexBools*)bools_storage; }
int flex_ints_check(int index, int value) { return flex_ints()->data[index] == value; }
int flex_doubles_check(int index, double value) { return flex_doubles()->values[index] == value; }
int flex_records_check(int index, int value) { return flex_records()->items[index].b == value; }
int flex_enums_check(int index, enum FlexMood value) { return flex_enums()->moods[index] == value; }
int flex_bools_check(int index, _Bool value) { return flex_bools()->flags[index] == value; }
void flex_ints_store(int index, int value) { flex_ints()->data[index] = value; }
void flex_doubles_store(int index, double value) { flex_doubles()->values[index] = value; }
void flex_records_store(int index, int value) { flex_records()->items[index].b = value; }
void flex_enums_store(int index, enum FlexMood value) { flex_enums()->moods[index] = value; }
void flex_bools_store(int index, _Bool value) { flex_bools()->flags[index] = value; }
FlexInts flex_ints_value(void) { FlexInts value = { 7 }; return value; }
int flex_ints_take(FlexInts value) { return value.n; }
int flex_get_ints(const FlexInts* p, int index) { return p->data[index]; }
void flex_set_ints(FlexInts* p, int index, int value) { p->data[index] = value; }
double flex_get_doubles(const FlexDoubles* p, int index) { return p->values[index]; }
void flex_set_doubles(FlexDoubles* p, int index, double value) { p->values[index] = value; }
int flex_get_records(const FlexRecords* p, int index) { return p->items[index].b; }
void flex_set_records(FlexRecords* p, int index, int value) { p->items[index].b = value; }
enum FlexMood flex_get_enums(const FlexEnums* p, int index) { return p->moods[index]; }
void flex_set_enums(FlexEnums* p, int index, enum FlexMood value) { p->moods[index] = value; }
_Bool flex_get_bools(const FlexBools* p, int index) { return p->flags[index]; }
void flex_set_bools(FlexBools* p, int index, _Bool value) { p->flags[index] = value; }
enum FlexMood flex_happy(void) { return FlexHappy; }
enum FlexMood flex_sad(void) { return FlexSad; }
