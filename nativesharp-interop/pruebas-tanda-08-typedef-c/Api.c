#include "Api.h"
CCountAlias c_double(CCountAlias value) { return value * 2; }
int c_matrix_sum(int first, int second) { return first + second; }
int c_matrix_ptr(const CMatrix* matrix) { return (*matrix)[0][1] + (*matrix)[1][2]; }
void c_matrix_mutate(CMatrix* matrix) { (*matrix)[0][1] = 13; }
const CMatrix* c_matrix_get(void) { static const CMatrix value = {{1, 2, 3}, {4, 5, 6}}; return &value; }
int c_points_sum(const CPointArray* points) { return (*points)[0].x + (*points)[1].y; }
void c_points_mutate(CPointArray* points) { (*points)[0].x = 12; }
const CPointArray* c_points_get(void) { static const CPointArray value = {{1, 2}, {3, 4}}; return &value; }
int c_colors_sum(const CColorArray* colors) { return (*colors)[0] + (*colors)[1]; }
void c_colors_mutate(CColorArray* colors) { (*colors)[0] = COLOR_BLUE; }
void c_colors_init(CColorArray* colors) { (*colors)[0] = COLOR_RED; (*colors)[1] = COLOR_BLUE; }
const CColorArray* c_colors_get(void) { static const CColorArray value = { COLOR_RED, COLOR_BLUE }; return &value; }
int c_point_sum(const CPoint* point) { return point->x + point->y; }
CColor c_color(void) { return COLOR_BLUE; }
CColor c_echo_color(CColor color) { return color; }
int c_unsigned_verify(CUnsigned value) { return value == U_HIGH; }
static int increment(int x) { return x + 1; }
COperation c_operation(void) { return increment; }
