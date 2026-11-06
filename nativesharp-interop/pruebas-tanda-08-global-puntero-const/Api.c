#include "Api.h"
static const int first = 11;
static const int second = 22;
static const int other = 33;
const int *c_slots[2] = { &first, &second };
const int *c_matrix[2][2] = { { &first, &second }, { &second, &first } };
const int *c_other(void) { return &other; }
int c_verify(void) { return *c_slots[0] == 33 && *c_slots[1] == 22; }
int c_verify_matrix(void) { return *c_matrix[1][0] == 33 && *c_matrix[0][1] == 22; }
void c_mutate(int **slots) { *slots[0] = 44; }
