typedef struct AnonOuter {
    struct { union { struct { enum { AnonFirst = 11, AnonSecond = 23 } mode; int value; }; long long raw; }; };
} AnonOuter;
AnonOuter* get_anon(void);
int read_anon(void);
