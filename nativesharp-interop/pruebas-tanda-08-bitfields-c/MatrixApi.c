#include "MatrixApi.h"
static VolatileMatrix matrix;
VolatileMatrix* matrix_get(void) { return &matrix; }
void matrix_init(VolatileMatrix* value) { value->small = 5; value->medium = -4; value->word = 21; value->wide = 300; }
int matrix_check(VolatileMatrix* value) { return value->small == 6 && value->medium == -3 && value->word == 22 && value->wide == 301; }
int matrix_read_small(VolatileMatrix* value) { return value->small; }
int matrix_read_medium(VolatileMatrix* value) { return value->medium; }
int matrix_read_word(VolatileMatrix* value) { return value->word; }
int matrix_read_wide(VolatileMatrix* value) { return (int)value->wide; }
void matrix_increment_small(VolatileMatrix* value) { value->small++; }
void matrix_increment_medium(VolatileMatrix* value) { value->medium++; }
void matrix_increment_word(VolatileMatrix* value) { value->word++; }
void matrix_increment_wide(VolatileMatrix* value) { value->wide++; }
