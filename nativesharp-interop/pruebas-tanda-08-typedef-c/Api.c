#include "Api.h"
CCountAlias c_double(CCountAlias value) { return value * 2; }
int c_matrix_sum(int first, int second) { return first + second; }
int c_point_sum(const CPoint* point) { return point->x + point->y; }
CColor c_color(void) { return COLOR_BLUE; }
static int increment(int x) { return x + 1; }
COperation c_operation(void) { return increment; }
