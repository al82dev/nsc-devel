#include "Api.h"
static LNatural g_lnatural;
static LNatural s_lnatural;
LNatural* l_obj_lnatural(void) { return &g_lnatural; }
LNatural* l_shadow_lnatural(void) { return &s_lnatural; }
static LPack4 g_lpack4;
static LPack4 s_lpack4;
LPack4* l_obj_lpack4(void) { return &g_lpack4; }
LPack4* l_shadow_lpack4(void) { return &s_lpack4; }
static LPack2 g_lpack2;
static LPack2 s_lpack2;
LPack2* l_obj_lpack2(void) { return &g_lpack2; }
LPack2* l_shadow_lpack2(void) { return &s_lpack2; }
static LPack1 g_lpack1;
static LPack1 s_lpack1;
LPack1* l_obj_lpack1(void) { return &g_lpack1; }
LPack1* l_shadow_lpack1(void) { return &s_lpack1; }
static LOuter g_louter;
static LOuter s_louter;
LOuter* l_obj_louter(void) { return &g_louter; }
LOuter* l_shadow_louter(void) { return &s_louter; }
static LHost g_lhost;
static LHost s_lhost;
LHost* l_obj_lhost(void) { return &g_lhost; }
LHost* l_shadow_lhost(void) { return &s_lhost; }
static LPack2Array g_lpack2array;
static LPack2Array s_lpack2array;
LPack2Array* l_obj_lpack2array(void) { return &g_lpack2array; }
LPack2Array* l_shadow_lpack2array(void) { return &s_lpack2array; }
static LAlignedField g_lalignedfield;
static LAlignedField s_lalignedfield;
LAlignedField* l_obj_lalignedfield(void) { return &g_lalignedfield; }
LAlignedField* l_shadow_lalignedfield(void) { return &s_lalignedfield; }
static LAlignedTypedef g_lalignedtypedef;
static LAlignedTypedef s_lalignedtypedef;
LAlignedTypedef* l_obj_lalignedtypedef(void) { return &g_lalignedtypedef; }
LAlignedTypedef* l_shadow_lalignedtypedef(void) { return &s_lalignedtypedef; }
static LUnderTypedef g_lundertypedef;
static LUnderTypedef s_lundertypedef;
LUnderTypedef* l_obj_lundertypedef(void) { return &g_lundertypedef; }
LUnderTypedef* l_shadow_lundertypedef(void) { return &s_lundertypedef; }
static LUnion g_lunion;
static LUnion s_lunion;
LUnion* l_obj_lunion(void) { return &g_lunion; }
LUnion* l_shadow_lunion(void) { return &s_lunion; }
static LPackedUnion g_lpackedunion;
static LPackedUnion s_lpackedunion;
LPackedUnion* l_obj_lpackedunion(void) { return &g_lpackedunion; }
LPackedUnion* l_shadow_lpackedunion(void) { return &s_lpackedunion; }
static LPackedHoldsUnion g_lpackedholdsunion;
static LPackedHoldsUnion s_lpackedholdsunion;
LPackedHoldsUnion* l_obj_lpackedholdsunion(void) { return &g_lpackedholdsunion; }
LPackedHoldsUnion* l_shadow_lpackedholdsunion(void) { return &s_lpackedholdsunion; }
static LUnionHoldsStruct g_lunionholdsstruct;
static LUnionHoldsStruct s_lunionholdsstruct;
LUnionHoldsStruct* l_obj_lunionholdsstruct(void) { return &g_lunionholdsstruct; }
LUnionHoldsStruct* l_shadow_lunionholdsstruct(void) { return &s_lunionholdsstruct; }
static LBits g_lbits;
static LBits s_lbits;
LBits* l_obj_lbits(void) { return &g_lbits; }
LBits* l_shadow_lbits(void) { return &s_lbits; }
static LPackedBits g_lpackedbits;
static LPackedBits s_lpackedbits;
LPackedBits* l_obj_lpackedbits(void) { return &g_lpackedbits; }
LPackedBits* l_shadow_lpackedbits(void) { return &s_lpackedbits; }
static LBitsUnion g_lbitsunion;
static LBitsUnion s_lbitsunion;
LBitsUnion* l_obj_lbitsunion(void) { return &g_lbitsunion; }
LBitsUnion* l_shadow_lbitsunion(void) { return &s_lbitsunion; }
static void fill(unsigned char* target, unsigned long long size)
{
    for (unsigned long long i = 0; i < size; ++i) target[i] = (unsigned char)(0xA5u ^ i);
}
static int equal(const unsigned char* a, const unsigned char* b, unsigned long long size)
{
    for (unsigned long long i = 0; i < size; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
void l_reset(int id)
{
    switch (id)
    {
    case 1:
        fill((unsigned char*)&g_lnatural, sizeof g_lnatural);
        fill((unsigned char*)&s_lnatural, sizeof s_lnatural);
        break;
    case 2:
        fill((unsigned char*)&g_lpack4, sizeof g_lpack4);
        fill((unsigned char*)&s_lpack4, sizeof s_lpack4);
        break;
    case 3:
        fill((unsigned char*)&g_lpack2, sizeof g_lpack2);
        fill((unsigned char*)&s_lpack2, sizeof s_lpack2);
        break;
    case 4:
        fill((unsigned char*)&g_lpack1, sizeof g_lpack1);
        fill((unsigned char*)&s_lpack1, sizeof s_lpack1);
        break;
    case 5:
        fill((unsigned char*)&g_louter, sizeof g_louter);
        fill((unsigned char*)&s_louter, sizeof s_louter);
        break;
    case 6:
        fill((unsigned char*)&g_lhost, sizeof g_lhost);
        fill((unsigned char*)&s_lhost, sizeof s_lhost);
        break;
    case 7:
        fill((unsigned char*)&g_lpack2array, sizeof g_lpack2array);
        fill((unsigned char*)&s_lpack2array, sizeof s_lpack2array);
        break;
    case 8:
        fill((unsigned char*)&g_lalignedfield, sizeof g_lalignedfield);
        fill((unsigned char*)&s_lalignedfield, sizeof s_lalignedfield);
        break;
    case 9:
        fill((unsigned char*)&g_lalignedtypedef, sizeof g_lalignedtypedef);
        fill((unsigned char*)&s_lalignedtypedef, sizeof s_lalignedtypedef);
        break;
    case 10:
        fill((unsigned char*)&g_lundertypedef, sizeof g_lundertypedef);
        fill((unsigned char*)&s_lundertypedef, sizeof s_lundertypedef);
        break;
    case 11:
        fill((unsigned char*)&g_lunion, sizeof g_lunion);
        fill((unsigned char*)&s_lunion, sizeof s_lunion);
        break;
    case 12:
        fill((unsigned char*)&g_lpackedunion, sizeof g_lpackedunion);
        fill((unsigned char*)&s_lpackedunion, sizeof s_lpackedunion);
        break;
    case 13:
        fill((unsigned char*)&g_lpackedholdsunion, sizeof g_lpackedholdsunion);
        fill((unsigned char*)&s_lpackedholdsunion, sizeof s_lpackedholdsunion);
        break;
    case 14:
        fill((unsigned char*)&g_lunionholdsstruct, sizeof g_lunionholdsstruct);
        fill((unsigned char*)&s_lunionholdsstruct, sizeof s_lunionholdsstruct);
        break;
    case 15:
        fill((unsigned char*)&g_lbits, sizeof g_lbits);
        fill((unsigned char*)&s_lbits, sizeof s_lbits);
        break;
    case 16:
        fill((unsigned char*)&g_lpackedbits, sizeof g_lpackedbits);
        fill((unsigned char*)&s_lpackedbits, sizeof s_lpackedbits);
        break;
    case 17:
        fill((unsigned char*)&g_lbitsunion, sizeof g_lbitsunion);
        fill((unsigned char*)&s_lbitsunion, sizeof s_lbitsunion);
        break;
    default: break;
    }
}
int l_same(int id)
{
    switch (id)
    {
    case 1:
        return equal((const unsigned char*)&g_lnatural, (const unsigned char*)&s_lnatural,
                     sizeof g_lnatural);
    case 2:
        return equal((const unsigned char*)&g_lpack4, (const unsigned char*)&s_lpack4,
                     sizeof g_lpack4);
    case 3:
        return equal((const unsigned char*)&g_lpack2, (const unsigned char*)&s_lpack2,
                     sizeof g_lpack2);
    case 4:
        return equal((const unsigned char*)&g_lpack1, (const unsigned char*)&s_lpack1,
                     sizeof g_lpack1);
    case 5:
        return equal((const unsigned char*)&g_louter, (const unsigned char*)&s_louter,
                     sizeof g_louter);
    case 6:
        return equal((const unsigned char*)&g_lhost, (const unsigned char*)&s_lhost,
                     sizeof g_lhost);
    case 7:
        return equal((const unsigned char*)&g_lpack2array, (const unsigned char*)&s_lpack2array,
                     sizeof g_lpack2array);
    case 8:
        return equal((const unsigned char*)&g_lalignedfield, (const unsigned char*)&s_lalignedfield,
                     sizeof g_lalignedfield);
    case 9:
        return equal((const unsigned char*)&g_lalignedtypedef, (const unsigned char*)&s_lalignedtypedef,
                     sizeof g_lalignedtypedef);
    case 10:
        return equal((const unsigned char*)&g_lundertypedef, (const unsigned char*)&s_lundertypedef,
                     sizeof g_lundertypedef);
    case 11:
        return equal((const unsigned char*)&g_lunion, (const unsigned char*)&s_lunion,
                     sizeof g_lunion);
    case 12:
        return equal((const unsigned char*)&g_lpackedunion, (const unsigned char*)&s_lpackedunion,
                     sizeof g_lpackedunion);
    case 13:
        return equal((const unsigned char*)&g_lpackedholdsunion, (const unsigned char*)&s_lpackedholdsunion,
                     sizeof g_lpackedholdsunion);
    case 14:
        return equal((const unsigned char*)&g_lunionholdsstruct, (const unsigned char*)&s_lunionholdsstruct,
                     sizeof g_lunionholdsstruct);
    case 15:
        return equal((const unsigned char*)&g_lbits, (const unsigned char*)&s_lbits,
                     sizeof g_lbits);
    case 16:
        return equal((const unsigned char*)&g_lpackedbits, (const unsigned char*)&s_lpackedbits,
                     sizeof g_lpackedbits);
    case 17:
        return equal((const unsigned char*)&g_lbitsunion, (const unsigned char*)&s_lbitsunion,
                     sizeof g_lbitsunion);
    default: return 0;
    }
}
long long l_layout(int id)
{
    switch (id)
    {
    case 1000: return (long long)sizeof(LNatural);
    case 1001: return (long long)_Alignof(LNatural);
    case 1002: return (long long)__builtin_offsetof(LNatural, d);
    case 1003: return (long long)__builtin_offsetof(LNatural, s);
    case 1004: return (long long)sizeof(LPack4);
    case 1005: return (long long)_Alignof(LPack4);
    case 1006: return (long long)__builtin_offsetof(LPack4, d);
    case 1007: return (long long)__builtin_offsetof(LPack4, s);
    case 1008: return (long long)sizeof(LPack2);
    case 1009: return (long long)_Alignof(LPack2);
    case 1010: return (long long)__builtin_offsetof(LPack2, d);
    case 1011: return (long long)__builtin_offsetof(LPack2, s);
    case 1012: return (long long)sizeof(LPack1);
    case 1013: return (long long)_Alignof(LPack1);
    case 1014: return (long long)__builtin_offsetof(LPack1, d);
    case 1015: return (long long)__builtin_offsetof(LPack1, s);
    case 1016: return (long long)sizeof(LOuter);
    case 1017: return (long long)_Alignof(LOuter);
    case 1018: return (long long)__builtin_offsetof(LOuter, m);
    case 1019: return (long long)sizeof(LHost);
    case 1020: return (long long)_Alignof(LHost);
    case 1021: return (long long)__builtin_offsetof(LHost, pi);
    case 1022: return (long long)__builtin_offsetof(LHost, z);
    case 1023: return (long long)sizeof(LPack2Array);
    case 1024: return (long long)_Alignof(LPack2Array);
    case 1025: return (long long)__builtin_offsetof(LPack2Array, items);
    case 1026: return (long long)sizeof(LAlignedField);
    case 1027: return (long long)_Alignof(LAlignedField);
    case 1028: return (long long)__builtin_offsetof(LAlignedField, x);
    case 1029: return (long long)sizeof(LAlignedTypedef);
    case 1030: return (long long)_Alignof(LAlignedTypedef);
    case 1031: return (long long)__builtin_offsetof(LAlignedTypedef, x);
    case 1032: return (long long)sizeof(LUnderTypedef);
    case 1033: return (long long)_Alignof(LUnderTypedef);
    case 1034: return (long long)__builtin_offsetof(LUnderTypedef, x);
    case 1035: return (long long)sizeof(LUnion);
    case 1036: return (long long)_Alignof(LUnion);
    case 1037: return (long long)__builtin_offsetof(LUnion, d);
    case 1038: return (long long)__builtin_offsetof(LUnion, i);
    case 1039: return (long long)sizeof(LPackedUnion);
    case 1040: return (long long)_Alignof(LPackedUnion);
    case 1041: return (long long)__builtin_offsetof(LPackedUnion, d);
    case 1042: return (long long)sizeof(LPackedHoldsUnion);
    case 1043: return (long long)_Alignof(LPackedHoldsUnion);
    case 1044: return (long long)__builtin_offsetof(LPackedHoldsUnion, u);
    case 1045: return (long long)sizeof(LUnionHoldsStruct);
    case 1046: return (long long)_Alignof(LUnionHoldsStruct);
    case 1047: return (long long)__builtin_offsetof(LUnionHoldsStruct, inner);
    case 1048: return (long long)__builtin_offsetof(LUnionHoldsStruct, ll);
    case 1049: return (long long)sizeof(LBits);
    case 1050: return (long long)_Alignof(LBits);
    case 1051: return (long long)sizeof(LPackedBits);
    case 1052: return (long long)_Alignof(LPackedBits);
    case 1053: return (long long)sizeof(LBitsUnion);
    case 1054: return (long long)_Alignof(LBitsUnion);
    default: return -1;
    }
}
double acc_get_lnatural_d(const LNatural* p) { return p->d; }
void acc_set_lnatural_d(LNatural* p, double v) { p->d = v; }
short acc_get_lnatural_s(const LNatural* p) { return p->s; }
void acc_set_lnatural_s(LNatural* p, short v) { p->s = v; }
double acc_get_lpack4_d(const LPack4* p) { return p->d; }
void acc_set_lpack4_d(LPack4* p, double v) { p->d = v; }
short acc_get_lpack4_s(const LPack4* p) { return p->s; }
void acc_set_lpack4_s(LPack4* p, short v) { p->s = v; }
double acc_get_lpack2_d(const LPack2* p) { return p->d; }
void acc_set_lpack2_d(LPack2* p, double v) { p->d = v; }
short acc_get_lpack2_s(const LPack2* p) { return p->s; }
void acc_set_lpack2_s(LPack2* p, short v) { p->s = v; }
double acc_get_lpack1_d(const LPack1* p) { return p->d; }
void acc_set_lpack1_d(LPack1* p, double v) { p->d = v; }
short acc_get_lpack1_s(const LPack1* p) { return p->s; }
void acc_set_lpack1_s(LPack1* p, short v) { p->s = v; }
int acc_get_louter_a(const LOuter* p) { return p->m.inner.a; }
void acc_set_louter_a(LOuter* p, int v) { p->m.inner.a = v; }
double acc_get_louter_b(const LOuter* p) { return p->m.inner.b; }
void acc_set_louter_b(LOuter* p, double v) { p->m.inner.b = v; }
int acc_get_lhost_v(const LHost* p) { return p->pi.v; }
void acc_set_lhost_v(LHost* p, int v) { p->pi.v = v; }
int acc_get_lhost_z(const LHost* p) { return p->z; }
void acc_set_lhost_z(LHost* p, int v) { p->z = v; }
int acc_get_lpack2array_a(const LPack2Array* p) { return p->items[1].a; }
void acc_set_lpack2array_a(LPack2Array* p, int v) { p->items[1].a = v; }
double acc_get_lpack2array_b(const LPack2Array* p) { return p->items[1].b; }
void acc_set_lpack2array_b(LPack2Array* p, double v) { p->items[1].b = v; }
int acc_get_lalignedfield_x(const LAlignedField* p) { return p->x; }
void acc_set_lalignedfield_x(LAlignedField* p, int v) { p->x = v; }
int acc_get_lalignedtypedef_x(const LAlignedTypedef* p) { return p->x; }
void acc_set_lalignedtypedef_x(LAlignedTypedef* p, int v) { p->x = v; }
int acc_get_lundertypedef_x(const LUnderTypedef* p) { return p->x; }
void acc_set_lundertypedef_x(LUnderTypedef* p, int v) { p->x = v; }
double acc_get_lunion_d(const LUnion* p) { return p->d; }
void acc_set_lunion_d(LUnion* p, double v) { p->d = v; }
int acc_get_lunion_i(const LUnion* p) { return p->i; }
void acc_set_lunion_i(LUnion* p, int v) { p->i = v; }
double acc_get_lpackedunion_d(const LPackedUnion* p) { return p->d; }
void acc_set_lpackedunion_d(LPackedUnion* p, double v) { p->d = v; }
double acc_get_lpackedholdsunion_d(const LPackedHoldsUnion* p) { return p->u.d; }
void acc_set_lpackedholdsunion_d(LPackedHoldsUnion* p, double v) { p->u.d = v; }
int acc_get_lpackedholdsunion_i(const LPackedHoldsUnion* p) { return p->u.i; }
void acc_set_lpackedholdsunion_i(LPackedHoldsUnion* p, int v) { p->u.i = v; }
double acc_get_lunionholdsstruct_b(const LUnionHoldsStruct* p) { return p->inner.b; }
void acc_set_lunionholdsstruct_b(LUnionHoldsStruct* p, double v) { p->inner.b = v; }
long long acc_get_lunionholdsstruct_ll(const LUnionHoldsStruct* p) { return p->ll; }
void acc_set_lunionholdsstruct_ll(LUnionHoldsStruct* p, long long v) { p->ll = v; }
unsigned acc_get_lbits_a(const LBits* p) { return p->a; }
void acc_set_lbits_a(LBits* p, unsigned v) { p->a = v; }
unsigned acc_get_lbits_b(const LBits* p) { return p->b; }
void acc_set_lbits_b(LBits* p, unsigned v) { p->b = v; }
unsigned acc_get_lbits_c(const LBits* p) { return p->c; }
void acc_set_lbits_c(LBits* p, unsigned v) { p->c = v; }
unsigned short acc_get_lpackedbits_a(const LPackedBits* p) { return p->a; }
void acc_set_lpackedbits_a(LPackedBits* p, unsigned short v) { p->a = v; }
unsigned acc_get_lpackedbits_b(const LPackedBits* p) { return p->b; }
void acc_set_lpackedbits_b(LPackedBits* p, unsigned v) { p->b = v; }
unsigned acc_get_lbitsunion_a(const LBitsUnion* p) { return p->a; }
void acc_set_lbitsunion_a(LBitsUnion* p, unsigned v) { p->a = v; }
unsigned short acc_get_lbitsunion_b(const LBitsUnion* p) { return p->b; }
void acc_set_lbitsunion_b(LBitsUnion* p, unsigned short v) { p->b = v; }
