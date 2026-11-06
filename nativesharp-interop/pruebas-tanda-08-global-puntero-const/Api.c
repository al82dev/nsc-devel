#include "Api.h"
static const int first = 11;
static const int second = 22;
static const int other = 33;
static int fixed_first = 55;
static int fixed_second = 66;
const int *c_slots[2] = { &first, &second };
const int *c_matrix[2][2] = { { &first, &second }, { &second, &first } };
int * const c_fixed_slots[2] = { &fixed_first, &fixed_second };
int * const c_fixed_matrix[2][2] = { { &fixed_first, &fixed_second }, { &fixed_second, &fixed_first } };
const int * const c_fixed_const[2] = { &first, &second };
const int *c_other(void) { return &other; }
int c_verify(void) { return *c_slots[0] == 33 && *c_slots[1] == 22; }
int c_verify_matrix(void) { return *c_matrix[1][0] == 33 && *c_matrix[0][1] == 22; }
int c_verify_fixed(void) { return fixed_first == 77 && fixed_second == 66; }
int c_verify_fixed_matrix(void) { return fixed_first == 77 && fixed_second == 88; }
void c_mutate(int **slots) { *slots[0] = 44; }
