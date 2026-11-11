#include "api.h"
int _fltused = 0;
int main(void) {
    int value = 123;
    if (sysv_varargs(3, (signed char)-2, (float)2.25f, -5000000000LL) != 1) return 1;
    if (sysv_many(9, (signed char)-7, (unsigned char)250, (short)-1000,
                  (unsigned short)65000, -5000000000LL, 4000000000U, &value, 2.25f, 3.5) != 1) return 2;
    short s = -1234; T09VarRecord record = {42, 0}; signed char b = -12;
    if (sysv_ptrs(3, &s, &record, &b) != 1 || s != 1234 || record.x != 43 || b != 12) return 3;
    if (sysv_empty(0) != 1) return 4;
    if (sysv_floatspill(9, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 3.5) != 1) return 5;
    s = -1234; record.x = 42;
    if (sysv_fixed_ptrs(&record, &s, (signed char)-7, 2.25f) != 1 || s != 1234 || record.x != 43) return 6;
    if (sysv_return_ptr(&record, 7) != &record || record.x != 44) return 7;
    T09VarRecord small = {1, 2}; T09VarMixed mixed = {2.25, 7}; T09VarLarge large = {11, 13, 17};
    if (sysv_record_small(1, small) != 1 || small.x != 1 || small.y != 2) return 8;
    if (sysv_record_mixed(1, mixed) != 1 || mixed.x != 2.25 || mixed.y != 7) return 9;
    if (sysv_record_two(2, small, mixed) != 1 || small.x != 1 || mixed.y != 7) return 13;
    if (sysv_record_pressure(6, 1, 2, 3, 4, 5, mixed) != 1 || mixed.y != 7) return 10;
    if (sysv_record_sse_pressure(9, 1., 2., 3., 4., 5., 6., 7., 8., mixed) != 1 || mixed.y != 7) return 12;
    if (sysv_record_large(1, large) != 1 || large.x != 11 || large.y != 13 || large.z != 17) return 11;
    int fixedValue = 41; short optionalValue = 12;
    if (sysv_record_ptrs(&fixedValue, mixed, &optionalValue) != 1 || fixedValue != 42 ||
        optionalValue != 13 || mixed.y != 7) return 14;
    fixedValue = 41; optionalValue = 12;
    if (sysv_record_return_ptr(&fixedValue, mixed, &optionalValue) != &fixedValue || fixedValue != 42 ||
        optionalValue != 13 || mixed.y != 7) return 15;
    if (sysv_record_rptr(2, mixed, &small) != 1 || small.x != 3 || small.y != 2 || mixed.y != 7) return 16;
    small.x = 1; optionalValue = 12;
    if (sysv_record_fixed_rptr(&small, mixed, &optionalValue) != &small || small.x != 3 ||
        optionalValue != 13 || mixed.y != 7) return 17;
    T09VarPackedValue packedValue = {7, 2.25};
    if (sysv_record_packed(1, packedValue) != 1 || packedValue.tag != 7 || packedValue.value != 2.25) return 18;
    if (sysv_record_packed_ptr(2, mixed, &packedValue) != 1 || packedValue.tag != 8 || mixed.y != 7) return 19;
    T09VarArray array = {{2.25, 3.5}};
    if (sysv_record_array(1, array) != 1 || array.values[0] != 2.25 || array.values[1] != 3.5) return 20;
    T09VarNested nested = {2.25, {1, 2}};
    if (sysv_record_nested(1, nested) != 1 || nested.leaf.x != 1 || nested.leaf.y != 2) return 21;
    T09VarAliasFields aliasFields = {2.25, {1, 2}};
    if (sysv_record_nested(1, aliasFields) != 1 || aliasFields.leaf.x != 1 || aliasFields.leaf.y != 2) return 25;
    T09VarRecordArray recordArray = {{{1, 2}, {3, 4}}};
    if (sysv_record_record_array(1, recordArray) != 1 || recordArray.leaves[1].y != 4) return 22;
    T09VarRecordArrayLarge recordArrayLarge = {{{1, 2}, {3, 4}, {5, 6}}};
    if (sysv_record_record_array_large(1, recordArrayLarge) != 1 || recordArrayLarge.leaves[2].y != 6) return 24;
    T09VarUnionOuter unionOuter = {2.25, {.integer = 7}};
    if (sysv_record_union_outer(1, unionOuter) != 1 || unionOuter.choice.integer != 7) return 23;
    return 0;
}
