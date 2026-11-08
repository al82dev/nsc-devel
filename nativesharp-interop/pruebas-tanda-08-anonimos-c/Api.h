typedef struct LAnonymous {
    int tag;
    union { int i; float f; };
    struct { short x; short y; };
    union { struct { int deep; }; long long storage; };
    struct { unsigned low : 3; unsigned high : 5; };
    struct { volatile int vNatural; };
    struct { int items[3]; };
} LAnonymous;
#pragma pack(push, 1)
typedef struct LPackedAnonymous {
    unsigned char tag;
    union { int packedInt; float packedFloat; };
    struct { short packedShort; };
    struct { unsigned packedBits : 3; unsigned packedMore : 5; };
    struct { volatile int vPacked; };
    struct { short packedItems[2]; };
} LPackedAnonymous;
#pragma pack(pop)
LAnonymous* probe_anonymous(void);
LPackedAnonymous* probe_packed_anonymous(void);
int probe_anonymous_check(void);
int probe_anonymous_size(void);
int probe_packed_anonymous_size(void);
void probe_anonymous_set_bits(void);
void probe_anonymous_set_items(void);
enum { AnonExtra = 12 };
