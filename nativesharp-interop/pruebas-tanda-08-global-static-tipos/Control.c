#include "Api.h"
int main(void) {
    if (plain != 10 || byte_value != 11 || short_value != -12 || long32 != -13 || ulong32 != 14 ||
        !truth || mood != StaticSad || atomic_value != 15 || atomic_mood != StaticSad || values[2] != 18 || grid[1][2] != 6) return 1;
    plain = 20; byte_value = 21; short_value = -22; long32 = -23; ulong32 = 24;
    truth = 0; mood = happy(); atomic_value = 25; atomic_mood = happy(); values[2] = 26; grid[1][2] = 27;
    return inspect() ? 0 : 2;
}
