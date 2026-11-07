#include "Api.h"
int _fltused = 0;
int main(void) {
    if (!check_initial()) return 1;
    small = -5; medium = 24; word = 35; wide = -46; uwide = 57;
    fraction = 3.5f; real = 4.5; *pointer = 9;
    return check_modified() ? 0 : 2;
}
