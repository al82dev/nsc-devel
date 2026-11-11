struct R3 { unsigned char b[3]; };
struct R4 { unsigned char b[4]; };
struct R8 { unsigned char b[8]; };
struct R24 { unsigned char b[24]; };
extern struct R3 f3(struct R3);
extern struct R4 f4(struct R4);
extern struct R8 f8(struct R8);
extern struct R24 f24(struct R24);
struct R3 c3(struct R3 v) { return f3(v); }
struct R4 c4(struct R4 v) { return f4(v); }
struct R8 c8(struct R8 v) { return f8(v); }
struct R24 c24(struct R24 v) { return f24(v); }
