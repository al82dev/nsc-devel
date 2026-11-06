#include "Api.h"
static CBoolFields sample_bool_fields = { 3, 0, { 1, 0 } };
CBoolFields* layout_bool_fields(void) { return &sample_bool_fields; }
int layout_bool_size(void) { return (int)sizeof(CBoolFields); }
int layout_bool_alignment(void) { return (int)_Alignof(CBoolFields); }
int layout_bool_flag_offset(void) { return (int)__builtin_offsetof(CBoolFields, flag); }
int layout_bool_values_offset(void) { return (int)__builtin_offsetof(CBoolFields, values); }
int layout_bool_check(void) {
    return sample_bool_fields.flag && !sample_bool_fields.values[0] && sample_bool_fields.values[1];
}
static Pair sample_pair = { 1, 2 };
static AlignedVolatileAggregates aligned_volatile_aggregates = { { { 2, 3 }, { 4, 5 } }, { PackedSad, PackedSad } };
AlignedVolatileAggregates* layout_aligned_volatile_aggregates(void) { return &aligned_volatile_aggregates; }
int layout_aligned_volatile_update(void) {
    aligned_volatile_aggregates.pairs[1].b = 41;
    aligned_volatile_aggregates.moods[1] = PackedHappy;
    return aligned_volatile_aggregates.pairs[1].b + aligned_volatile_aggregates.moods[1];
}
int layout_aligned_volatile_pointer(AlignedVolatileAggregates* value) {
    value->pairs[1].b = 41;
    value->moods[1] = PackedHappy;
    return value->pairs[1].b + value->moods[1];
}
static PackedConstVolatileAggregates sample_packed_const_volatile_aggregates = { 1, { { 2, 3 }, { 4, 5 } }, { PackedSad, PackedHappy } };
PackedConstVolatileAggregates* layout_packed_const_volatile_aggregates(void) { return &sample_packed_const_volatile_aggregates; }
static PackedVolatileAggregates sample_packed_volatile_aggregates = { 1, { { 2, 3 }, { 4, 5 } }, { PackedSad, PackedSad } };
PackedVolatileAggregates* layout_packed_volatile_aggregates(void) { return &sample_packed_volatile_aggregates; }
int layout_packed_volatile_aggregates_sum(void) { return sample_packed_volatile_aggregates.pairs[1].b + sample_packed_volatile_aggregates.moods[1]; }
static ConstAggregates sample_const_aggregates = { { { 2, 3 }, { 4, 5 } }, { PackedSad, PackedHappy } };
ConstAggregates* layout_const_aggregates(void) { return &sample_const_aggregates; }
static PackedConstAggregates sample_packed_const_aggregates = { 1, { { 2, 3 }, { 4, 5 } }, { PackedSad, PackedHappy } };
PackedConstAggregates* layout_packed_const_aggregates(void) { return &sample_packed_const_aggregates; }
static PackedEnum sample_packed_enum = { 1, PackedSad, { PackedSad, PackedSad } };
PackedEnum* layout_packed_enum(void) { return &sample_packed_enum; }
enum PackedMood layout_packed_enum_new(void) { return PackedHappy; }
int layout_packed_enum_sum(void) { return sample_packed_enum.value + sample_packed_enum.values[1]; }
static PackedRecordMatrix sample_packed_record_matrix = { 1, { { { 1, 2 }, { 3, 4 } }, { { 5, 6 }, { 7, 8 } } } };
PackedRecordMatrix* layout_packed_record_matrix(void) { return &sample_packed_record_matrix; }
int layout_packed_record_matrix_value(void) { return sample_packed_record_matrix.values[1][1].b; }
static PackedRecordArray sample_packed_record_array = { 1, { { 2, 3 }, { 4, 5 } } };
PackedRecordArray* layout_packed_record_array(void) { return &sample_packed_record_array; }
int layout_packed_record_array_value(void) { return sample_packed_record_array.values[1].b; }
static PackedNestedArray sample_packed_nested_array = { 1, { { 2, 3 } } };
PackedNestedArray* layout_packed_nested_array(void) { return &sample_packed_nested_array; }
int layout_packed_nested_array_value(void) { return sample_packed_nested_array.nested.values[1]; }
static PackedNested sample_packed_nested = { 1, { 2, 3 } };
PackedNested* layout_packed_nested(void) { return &sample_packed_nested; }
int layout_packed_nested_value(void) { return sample_packed_nested.nested.b; }
int layout_packed_nested_size(void) { return (int)sizeof(PackedNested); }
int layout_packed_nested_offset(void) { return (int)__builtin_offsetof(PackedNested, nested); }
static Packed sample_packed = { 1, 2 };
static PackedArray sample_packed_array = { 1, { { 1, 2, 3 }, { 4, 5, 6 } } };
static PackedQualified sample_packed_qualified = { 1, { 2, 3 }, { 4, 5 } };
PackedQualified* layout_packed_qualified(void) { return &sample_packed_qualified; }
int layout_packed_qualified_status(void) { return sample_packed_qualified.status[1]; }
PackedArray* layout_packed_array(void) { return &sample_packed_array; }
int layout_packed_array_value(void) { return sample_packed_array.values[1][2]; }
int layout_packed_array_size(void) { return (int)sizeof(PackedArray); }
int layout_packed_array_offset(void) { return (int)__builtin_offsetof(PackedArray, values); }
static PackedTwo sample_packed_two = { 1, 2, 3, 4.0 };
PackedTwo* layout_packed_two(void) { return &sample_packed_two; }
int layout_packed_two_check(void)
{
    return sample_packed_two.aligned == 7 && sample_packed_two.integer == 9 &&
           sample_packed_two.real == 6.5;
}
int layout_packed_two_size(void) { return (int)sizeof(PackedTwo); }
int layout_packed_two_alignment(void) { return (int)_Alignof(PackedTwo); }
int layout_packed_two_integer_offset(void) { return (int)__builtin_offsetof(PackedTwo, integer); }
int layout_packed_two_real_offset(void) { return (int)__builtin_offsetof(PackedTwo, real); }
static int packed_pointer_initial = 3;
static int packed_pointer_target = 4;
static PackedPointers sample_packed_pointers = { 1, { &packed_pointer_initial, &packed_pointer_target } };
PackedPointers* layout_packed_pointers(void) { return &sample_packed_pointers; }
int layout_packed_pointers_value(void) { return *sample_packed_pointers.slots[1]; }
static PackedPointer sample_packed_pointer = { 1, &packed_pointer_initial };
PackedPointer* layout_packed_pointer(void) { return &sample_packed_pointer; }
int* layout_packed_pointer_target(void) { return &packed_pointer_target; }
int layout_packed_pointer_value(void) { return *sample_packed_pointer.value; }
Packed* layout_packed(void) { return &sample_packed; }
int layout_packed_value(void) { return sample_packed.value; }
int layout_packed_size(void) { return (int)sizeof(Packed); }
int layout_packed_alignment(void) { return (int)_Alignof(Packed); }
int layout_packed_offset(void) { return (int)__builtin_offsetof(Packed, value); }
Pair* layout_pair(void) { return &sample_pair; }
int layout_pair_value(void) { return sample_pair.b; }
int pair_size(void) { return (int)sizeof(Pair); }
int pair_alignment(void) { return (int)_Alignof(Pair); }
int pair_b_offset(void) { return (int)__builtin_offsetof(Pair, b); }
int cell_size(void) { return (int)sizeof(Cell); }
int cell_alignment(void) { return (int)_Alignof(Cell); }
int cell_real_offset(void) { return (int)__builtin_offsetof(Cell, real); }
int container_size(void) { return (int)sizeof(Container); }
int container_alignment(void) { return (int)_Alignof(Container); }
int container_pairs_offset(void) { return (int)__builtin_offsetof(Container, pairs); }
int container_cell_offset(void) { return (int)__builtin_offsetof(Container, cell); }
