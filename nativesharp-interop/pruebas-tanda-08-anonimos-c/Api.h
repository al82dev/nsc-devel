typedef struct LAnonymous {
    int tag;
    union { int i; float f; };
    struct { short x; short y; };
} LAnonymous;
LAnonymous* probe_anonymous(void);
int probe_anonymous_check(void);
int probe_anonymous_size(void);
