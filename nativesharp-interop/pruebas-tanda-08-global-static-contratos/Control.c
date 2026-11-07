#include "Api.h"
int main(void) {
    if (text[0] != 'o' || values[3] != 4 || ticks != 5 || local != 6 || atomic_local != 7) return 1;
    values[3] = 14; ticks = 15; local = 16; atomic_local = 17;
    box_ptr = next_box(); bool_ptr = next_bool();
    return inspect() ? 0 : 2;
}
