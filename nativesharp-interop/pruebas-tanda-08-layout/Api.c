#include "Api.h"
static Pair sample_pair = { 1, 2 };
static Packed sample_packed = { 1, 2 };
static PackedArray sample_packed_array = { 1, { { 1, 2, 3 }, { 4, 5, 6 } } };
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
