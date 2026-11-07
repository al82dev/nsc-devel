#include "Api.h"
int main(void) {
    if (**outer != 7 || points[1].x != 3 || storage.integer != 5 ||
        states[1] != StateHigh || readonly_values[2] != 12 || volatile_values[1] != 14) return 1;
    **outer = 20; points[1].x = 21; storage.integer = 22;
    states[0] = states[1]; volatile_values[1] = 23;
    return inspect() ? 0 : 2;
}
