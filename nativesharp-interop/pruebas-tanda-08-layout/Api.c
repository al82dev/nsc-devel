#include "Api.h"
static Pair sample_pair = { 1, 2 };
static Packed sample_packed = { 1, 2 };
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
