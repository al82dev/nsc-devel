enum StaticMood { StaticSad = -3, StaticHappy = 7 };
static char plain = 10;
static unsigned char byte_value = 11;
static short short_value = -12;
static long long32 = -13;
static unsigned long ulong32 = 14;
static _Bool truth = 1;
static enum StaticMood mood = StaticSad;
static _Atomic(int) atomic_value = 15;
static _Atomic(enum StaticMood) atomic_mood = StaticSad;
static int values[3] = { 16, 17, 18 };
static int grid[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
static inline enum StaticMood happy(void) { return StaticHappy; }
static inline int inspect(void) {
    return plain == 20 && byte_value == 21 && short_value == -22 && long32 == -23 &&
        ulong32 == 24 && !truth && mood == StaticHappy && atomic_value == 25 && atomic_mood == StaticHappy &&
        values[2] == 26 && grid[1][2] == 27;
}
