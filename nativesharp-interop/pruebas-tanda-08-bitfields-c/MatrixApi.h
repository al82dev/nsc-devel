typedef struct VolatileMatrix {
    volatile unsigned char small : 3;
    volatile signed short medium : 6;
    volatile unsigned int word : 5;
    unsigned int : 0;
    volatile unsigned long long wide : 9;
} VolatileMatrix;
VolatileMatrix* matrix_get(void);
void matrix_init(VolatileMatrix* value);
int matrix_check(VolatileMatrix* value);
int matrix_read_small(VolatileMatrix* value);
int matrix_read_medium(VolatileMatrix* value);
int matrix_read_word(VolatileMatrix* value);
int matrix_read_wide(VolatileMatrix* value);
void matrix_increment_small(VolatileMatrix* value);
void matrix_increment_medium(VolatileMatrix* value);
void matrix_increment_word(VolatileMatrix* value);
void matrix_increment_wide(VolatileMatrix* value);
