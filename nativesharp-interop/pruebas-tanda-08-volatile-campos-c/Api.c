#include "Api.h"
#include <stddef.h>
static VFields fields;
static VPacked packed;
static VUnion unionValue;
static int target = 42;
VFields* v_fields(void) { return &fields; }
VPacked* v_packed(void) { return &packed; }
VUnion* v_union(void) { return &unionValue; }
int* v_target(void) { return &target; }
long long v_layout(int id)
{
    switch (id)
    {
    case 1: return (long long)sizeof(VFields);
    case 2: return (long long)offsetof(VFields, flag);
    case 3: return (long long)offsetof(VFields, ll);
    case 4: return (long long)offsetof(VFields, ptr);
    case 5: return (long long)offsetof(VFields, ints);
    case 6: return (long long)offsetof(VFields, ints) + 8;
    case 7: return (long long)sizeof(VPacked);
    case 8: return (long long)offsetof(VPacked, d);
    case 9: return (long long)sizeof(VUnion);
    default: return -1;
    }
}
void v_reset(void)
{
    VFields zeroFields = {0};
    VPacked zeroPacked = {0};
    fields = zeroFields;
    packed = zeroPacked;
    unionValue.i = 0;
    fields.tag = 0x5A;
    fields.after = 0x12345678;
    packed.tag = 0x5A;
}
int v_neighbors_intact(void)
{
    return fields.tag == 0x5A && fields.after == 0x12345678 && packed.tag == 0x5A;
}
_Bool acc_get_flag(VFields* p) { return p->flag; }
void acc_set_flag(VFields* p, _Bool value) { p->flag = value; }
signed char acc_get_c(VFields* p) { return p->c; }
void acc_set_c(VFields* p, signed char value) { p->c = value; }
short acc_get_s(VFields* p) { return p->s; }
void acc_set_s(VFields* p, short value) { p->s = value; }
int acc_get_i(VFields* p) { return p->i; }
void acc_set_i(VFields* p, int value) { p->i = value; }
long long acc_get_ll(VFields* p) { return p->ll; }
void acc_set_ll(VFields* p, long long value) { p->ll = value; }
float acc_get_f(VFields* p) { return p->f; }
void acc_set_f(VFields* p, float value) { p->f = value; }
double acc_get_d(VFields* p) { return p->d; }
void acc_set_d(VFields* p, double value) { p->d = value; }
int* acc_get_ptr(VFields* p) { return p->ptr; }
void acc_set_ptr(VFields* p, int* value) { p->ptr = value; }
_Bool acc_get_flags1(VFields* p) { return p->flags[1]; }
void acc_set_flags1(VFields* p, _Bool value) { p->flags[1] = value; }
int acc_get_ints1(VFields* p) { return p->ints[1]; }
void acc_set_ints1(VFields* p, int value) { p->ints[1] = value; }
int acc_get_pi(VPacked* p) { return p->i; }
void acc_set_pi(VPacked* p, int value) { p->i = value; }
_Bool acc_get_pflag(VPacked* p) { return p->flag; }
void acc_set_pflag(VPacked* p, _Bool value) { p->flag = value; }
double acc_get_pd(VPacked* p) { return p->d; }
void acc_set_pd(VPacked* p, double value) { p->d = value; }
short acc_get_ps(VPacked* p) { return p->s; }
void acc_set_ps(VPacked* p, short value) { p->s = value; }
_Bool acc_get_uflag(VUnion* p) { return p->flag; }
void acc_set_uflag(VUnion* p, _Bool value) { p->flag = value; }
void acc_add_i(VFields* p, int value) { p->i += value; }
void acc_inc_s(VFields* p) { p->s++; }
int acc_get_mode(VFields* p) { return (int)p->mode; }
void acc_set_mode(VFields* p, int value) { p->mode = (VMode)value; }
int acc_get_pmode(VPacked* p) { return (int)p->mode; }
void acc_set_pmode(VPacked* p, int value) { p->mode = (VMode)value; }
int acc_get_umode(VUnion* p) { return (int)p->mode; }
void acc_set_umode(VUnion* p, int value) { p->mode = (VMode)value; }
