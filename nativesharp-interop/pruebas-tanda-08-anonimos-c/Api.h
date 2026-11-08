typedef struct LAnonymous {
    int tag;
    union { int i; float f; };
    struct { short x; short y; };
    union { struct { int deep; }; long long storage; };
} LAnonymous;
#pragma pack(push, 1)
typedef struct LPackedAnonymous {
    unsigned char tag;
    union { int packedInt; float packedFloat; };
    struct { short packedShort; };
} LPackedAnonymous;
#pragma pack(pop)
LAnonymous* probe_anonymous(void);
LPackedAnonymous* probe_packed_anonymous(void);
int probe_anonymous_check(void);
int probe_anonymous_size(void);
int probe_packed_anonymous_size(void);
