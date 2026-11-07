#include "Api.h"
int main(void) {
    if (text[0] != 'o' || values[3] != 4 || ticks != 5 || local != 6 || atomic_local != 7) return 1;
    if (box_ptr->value != 31 || *bool_ptr) return 3;
    values[3] = 14; ticks = 15; local = 16; atomic_local = 17;
    box_ptr = next_box(); bool_ptr = next_bool(); mode_ptr = next_mode();
    opaque_ptr = next_opaque(); const_box_ptr = next_const_box();
    if (box_ptr->value != 47 || !*bool_ptr) return 4;
    return inspect() ? 0 : 2;
}
