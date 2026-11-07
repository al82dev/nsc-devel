static signed char small = -4;
static unsigned short medium = 23;
static unsigned int word = 34;
static long long wide = -45;
static unsigned long long uwide = 56;
static float fraction = 1.5f;
static double real = 2.5;
static int target = 8;
static int* pointer = &target;
static inline int check_initial(void) {
    return small == -4 && medium == 23 && word == 34 && wide == -45 &&
        uwide == 56 && fraction == 1.5f && real == 2.5 && pointer == &target;
}
static inline int check_modified(void) {
    return small == -5 && medium == 24 && word == 35 && wide == -46 &&
        uwide == 57 && fraction == 3.5f && real == 4.5 && pointer == &target && *pointer == 9;
}
