typedef struct {
    int raw;
    enum { H4Low = 1, H4High = 2 } level, other;
    const enum { H4Fixed = 5 } fixed;
    volatile enum { H4Changing = 6, H4Changed = 9 } changing;
} H4Anon;
typedef struct {
    int raw;
    enum H4Named { H4NamedA = -3, H4NamedB = 4 } kind;
} H4NamedRecord;
static H4Anon h4_anon = { 7, H4Low, H4High, H4Fixed, H4Changing };
static H4NamedRecord h4_named = { 8, H4NamedA };
static inline H4Anon* h4_get_anon(void) { return &h4_anon; }
static inline H4NamedRecord* h4_get_named(void) { return &h4_named; }
static inline enum H4Named h4_named_b(void) { return H4NamedB; }
static inline int h4_check(void) {
    return h4_anon.raw == 7 && h4_anon.level == H4High && h4_anon.other == H4Low &&
        h4_anon.fixed == H4Fixed && h4_anon.changing == H4Changed &&
        h4_named.raw == 8 && h4_named.kind == H4NamedB;
}
