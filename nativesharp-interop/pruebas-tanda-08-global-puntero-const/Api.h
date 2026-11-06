extern const int *c_slots[2];
extern const int *c_matrix[2][2];
extern int * const c_fixed_slots[2];
const int *c_other(void);
int c_verify(void);
int c_verify_matrix(void);
int c_verify_fixed(void);
void c_mutate(int **slots);
