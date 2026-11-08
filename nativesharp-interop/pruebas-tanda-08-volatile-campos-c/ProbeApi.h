/* Probe only: volatile fields that NSC does not import today. */
typedef enum VMood { VMoodA = 1, VMoodB = 2 } VMood;
typedef struct VInner { _Bool flag; int i; } VInner;
typedef struct VOdd { char tag; volatile VMood mood; volatile VInner inner; int after; } VOdd;
VOdd* v_odd(void);
VMood v_mood_b(void);
int v_mood_value(VMood mood);
VMood acc_get_mood_c(VOdd* p);
_Bool acc_get_innerflag_c(VOdd* p);
int acc_get_inneri_c(VOdd* p);
int v_odd_after(VOdd* p);
typedef struct VPlain { _Bool flag; int i; } VPlain;
extern volatile VPlain v_plain_global;
int acc_get_plain_i(void);
void acc_set_plain_i(int value);
