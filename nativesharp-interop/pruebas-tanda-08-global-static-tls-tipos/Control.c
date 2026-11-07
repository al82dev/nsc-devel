#include "Api.h"
int _fltused = 0;
int main(void) {
    if (byte_value != 11 || short_value != -12 || large_value != 13 || float_value != 1.5f ||
        double_value != 2.5 || !truth || tone != ToneLow || atomic_value != 15 ||
        values[2] != 18 || pointer_value) return 1;
    byte_value = 21; short_value = -22; large_value = 23; float_value = 3.5f; double_value = 4.5;
    truth = 0; tone = high_tone(); atomic_value = 25; values[2] = 26;
    pointer_value = target_address(); *pointer_value = 27;
    return inspect() ? 0 : 2;
}
