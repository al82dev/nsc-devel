static int plain = 9;
static volatile int sink = 13;
static const unsigned int limit = 8;
static volatile short small = 4;
static const int* readptr = &plain;
static int* const fixedptr = &plain;
static volatile int* volptr = &sink;
static inline int inspect(void) {
    return limit == 8 && small == 5 && *readptr == 10 && *fixedptr == 10 && *volptr == 14;
}
