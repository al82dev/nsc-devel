typedef struct StaticPoint { int x; int y; } StaticPoint;
typedef union StaticUnion { int integer; unsigned int other; } StaticUnion;
enum StaticState { StateLow = 3, StateHigh = 8 };
static int base = 7;
static int* inner = &base;
static int** outer = &inner;
static StaticPoint points[2] = { { 1, 2 }, { 3, 4 } };
static StaticUnion storage = { .integer = 5 };
static enum StaticState states[2] = { StateLow, StateHigh };
static const int readonly_values[3] = { 10, 11, 12 };
static volatile int volatile_values[3] = { 13, 14, 15 };
static inline int inspect(void) {
    return **outer == 20 && points[1].x == 21 && storage.integer == 22 &&
        states[0] == StateHigh && readonly_values[2] == 12 && volatile_values[1] == 23;
}
