typedef struct LNatural { char c; double d; short s; } LNatural;
#pragma pack(push, 4)
typedef struct LPack4 { char c; double d; short s; } LPack4;
#pragma pack(pop)
#pragma pack(push, 2)
typedef struct LPack2 { char c; double d; short s; } LPack2;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct LPack1 { char c; double d; short s; } LPack1;
#pragma pack(pop)
typedef struct LInner { int a; double b; } LInner;
#pragma pack(push, 1)
typedef struct LMid { char t; LInner inner; } LMid;
#pragma pack(pop)
typedef struct LOuter { char t; LMid m; } LOuter;
#pragma pack(push, 1)
typedef struct LPackedInner { char c; int v; } LPackedInner;
#pragma pack(pop)
typedef struct LHost { char t; LPackedInner pi; int z; } LHost;
#pragma pack(push, 2)
typedef struct LPack2Array { char t; LInner items[2]; } LPack2Array;
#pragma pack(pop)
typedef struct LAlignedField { char c; _Alignas(16) int x; } LAlignedField;
typedef int __attribute__((aligned(8))) LInt8;
typedef struct LAlignedTypedef { char c; LInt8 x; } LAlignedTypedef;
typedef int __attribute__((aligned(1))) LInt1;
typedef struct LUnderTypedef { char c; LInt1 x; } LUnderTypedef;
typedef union LUnion { char c; double d; int i; } LUnion;
#pragma pack(push, 1)
typedef union LPackedUnion { char c; double d; } LPackedUnion;
typedef struct LPackedHoldsUnion { char t; LUnion u; } LPackedHoldsUnion;
#pragma pack(pop)
typedef union LUnionHoldsStruct { LInner inner; long long ll; } LUnionHoldsStruct;
typedef struct LBits { unsigned a : 3; unsigned : 0; unsigned b : 5; unsigned : 2; unsigned c : 4; } LBits;
#pragma pack(push, 1)
typedef struct LPackedBits { char t; unsigned short a : 3; unsigned b : 9; } LPackedBits;
#pragma pack(pop)
typedef union LBitsUnion { unsigned a : 5; unsigned short b : 12; } LBitsUnion;
long long l_layout(int id);
void l_reset(int id);
int l_same(int id);
LNatural* l_obj_lnatural(void);
LNatural* l_shadow_lnatural(void);
LPack4* l_obj_lpack4(void);
LPack4* l_shadow_lpack4(void);
LPack2* l_obj_lpack2(void);
LPack2* l_shadow_lpack2(void);
LPack1* l_obj_lpack1(void);
LPack1* l_shadow_lpack1(void);
LOuter* l_obj_louter(void);
LOuter* l_shadow_louter(void);
LHost* l_obj_lhost(void);
LHost* l_shadow_lhost(void);
LPack2Array* l_obj_lpack2array(void);
LPack2Array* l_shadow_lpack2array(void);
LAlignedField* l_obj_lalignedfield(void);
LAlignedField* l_shadow_lalignedfield(void);
LAlignedTypedef* l_obj_lalignedtypedef(void);
LAlignedTypedef* l_shadow_lalignedtypedef(void);
LUnderTypedef* l_obj_lundertypedef(void);
LUnderTypedef* l_shadow_lundertypedef(void);
LUnion* l_obj_lunion(void);
LUnion* l_shadow_lunion(void);
LPackedUnion* l_obj_lpackedunion(void);
LPackedUnion* l_shadow_lpackedunion(void);
LPackedHoldsUnion* l_obj_lpackedholdsunion(void);
LPackedHoldsUnion* l_shadow_lpackedholdsunion(void);
LUnionHoldsStruct* l_obj_lunionholdsstruct(void);
LUnionHoldsStruct* l_shadow_lunionholdsstruct(void);
LBits* l_obj_lbits(void);
LBits* l_shadow_lbits(void);
LPackedBits* l_obj_lpackedbits(void);
LPackedBits* l_shadow_lpackedbits(void);
LBitsUnion* l_obj_lbitsunion(void);
LBitsUnion* l_shadow_lbitsunion(void);
double acc_get_lnatural_d(const LNatural* p);
void acc_set_lnatural_d(LNatural* p, double v);
short acc_get_lnatural_s(const LNatural* p);
void acc_set_lnatural_s(LNatural* p, short v);
double acc_get_lpack4_d(const LPack4* p);
void acc_set_lpack4_d(LPack4* p, double v);
short acc_get_lpack4_s(const LPack4* p);
void acc_set_lpack4_s(LPack4* p, short v);
double acc_get_lpack2_d(const LPack2* p);
void acc_set_lpack2_d(LPack2* p, double v);
short acc_get_lpack2_s(const LPack2* p);
void acc_set_lpack2_s(LPack2* p, short v);
double acc_get_lpack1_d(const LPack1* p);
void acc_set_lpack1_d(LPack1* p, double v);
short acc_get_lpack1_s(const LPack1* p);
void acc_set_lpack1_s(LPack1* p, short v);
int acc_get_louter_a(const LOuter* p);
void acc_set_louter_a(LOuter* p, int v);
double acc_get_louter_b(const LOuter* p);
void acc_set_louter_b(LOuter* p, double v);
int acc_get_lhost_v(const LHost* p);
void acc_set_lhost_v(LHost* p, int v);
int acc_get_lhost_z(const LHost* p);
void acc_set_lhost_z(LHost* p, int v);
int acc_get_lpack2array_a(const LPack2Array* p);
void acc_set_lpack2array_a(LPack2Array* p, int v);
double acc_get_lpack2array_b(const LPack2Array* p);
void acc_set_lpack2array_b(LPack2Array* p, double v);
int acc_get_lalignedfield_x(const LAlignedField* p);
void acc_set_lalignedfield_x(LAlignedField* p, int v);
int acc_get_lalignedtypedef_x(const LAlignedTypedef* p);
void acc_set_lalignedtypedef_x(LAlignedTypedef* p, int v);
int acc_get_lundertypedef_x(const LUnderTypedef* p);
void acc_set_lundertypedef_x(LUnderTypedef* p, int v);
double acc_get_lunion_d(const LUnion* p);
void acc_set_lunion_d(LUnion* p, double v);
int acc_get_lunion_i(const LUnion* p);
void acc_set_lunion_i(LUnion* p, int v);
double acc_get_lpackedunion_d(const LPackedUnion* p);
void acc_set_lpackedunion_d(LPackedUnion* p, double v);
double acc_get_lpackedholdsunion_d(const LPackedHoldsUnion* p);
void acc_set_lpackedholdsunion_d(LPackedHoldsUnion* p, double v);
int acc_get_lpackedholdsunion_i(const LPackedHoldsUnion* p);
void acc_set_lpackedholdsunion_i(LPackedHoldsUnion* p, int v);
double acc_get_lunionholdsstruct_b(const LUnionHoldsStruct* p);
void acc_set_lunionholdsstruct_b(LUnionHoldsStruct* p, double v);
long long acc_get_lunionholdsstruct_ll(const LUnionHoldsStruct* p);
void acc_set_lunionholdsstruct_ll(LUnionHoldsStruct* p, long long v);
unsigned acc_get_lbits_a(const LBits* p);
void acc_set_lbits_a(LBits* p, unsigned v);
unsigned acc_get_lbits_b(const LBits* p);
void acc_set_lbits_b(LBits* p, unsigned v);
unsigned acc_get_lbits_c(const LBits* p);
void acc_set_lbits_c(LBits* p, unsigned v);
unsigned short acc_get_lpackedbits_a(const LPackedBits* p);
void acc_set_lpackedbits_a(LPackedBits* p, unsigned short v);
unsigned acc_get_lpackedbits_b(const LPackedBits* p);
void acc_set_lpackedbits_b(LPackedBits* p, unsigned v);
unsigned acc_get_lbitsunion_a(const LBitsUnion* p);
void acc_set_lbitsunion_a(LBitsUnion* p, unsigned v);
unsigned short acc_get_lbitsunion_b(const LBitsUnion* p);
void acc_set_lbitsunion_b(LBitsUnion* p, unsigned short v);
