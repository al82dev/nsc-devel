#include "ProbeAnonymous.h"
#include "ProbeNested.h"
#include "ProbeFlex.h"
static LAnonymous anonymous_value;
LAnonymous* probe_anonymous(void) { return &anonymous_value; }
int probe_anonymous_check(void)
{
    return anonymous_value.tag == 1 && anonymous_value.i == 2 && anonymous_value.x == 3 && anonymous_value.y == 4;
}
int probe_anonymous_size(void) { return (int)sizeof(LAnonymous); }
static LGuestHost nested_value;
static struct LGuest guest_value;
LGuestHost* probe_nested(void) { return &nested_value; }
struct LGuest* probe_guest(void) { return &guest_value; }
int probe_nested_check(void)
{
    return nested_value.guest.g == 1 && nested_value.guest.h == 2.5 && nested_value.k == 7 && guest_value.h == 3.5;
}
static int flex_storage[5];
LFlex* probe_flex(void) { return (LFlex*)flex_storage; }
int probe_flex_size(void) { return (int)sizeof(LFlex); }
int probe_flex_check(void) { return probe_flex()->n == 3 && probe_flex()->data[1] == 5; }
