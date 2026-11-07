struct StaticRecord { int value; int extra; };
static struct StaticRecord record_value = { 3, 4 };
static int items[2] = { 5, 6 };
static const int read_only = 7;
static volatile int changing = 8;
static inline int check_initial(void) {
    return record_value.value == 3 && record_value.extra == 4 &&
        items[0] == 5 && items[1] == 6 && read_only == 7 && changing == 8;
}
static inline int check_modified(void) {
    return record_value.value == 10 && record_value.extra == 4 &&
        items[0] == 11 && items[1] == 6 && read_only == 7 && changing == 12;
}
