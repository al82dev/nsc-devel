#include "ProbeApi.h"
static VOdd odd;
VOdd* v_odd(void) { return &odd; }
VMood v_mood_b(void) { return VMoodB; }
int v_mood_value(VMood mood) { return (int)mood; }
VMood acc_get_mood_c(VOdd* p) { return p->mood; }
_Bool acc_get_innerflag_c(VOdd* p) { return p->inner.flag; }
int acc_get_inneri_c(VOdd* p) { return p->inner.i; }
int v_odd_after(VOdd* p) { return p->after; }
volatile VPlain v_plain_global;
int acc_get_plain_i(void) { return v_plain_global.i; }
void acc_set_plain_i(int value) { v_plain_global.i = value; }
