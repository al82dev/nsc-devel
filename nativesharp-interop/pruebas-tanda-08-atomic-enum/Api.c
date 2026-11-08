#include "Api.h"
_Atomic(enum State) state = STATE_IDLE;
_Atomic(enum SignedState) signed_state = SIGNED_NEGATIVE;
_Atomic(enum WideState) wide_state = WIDE_LARGE;
int state_check(void) { return state == STATE_READY; }
unsigned long long wide_read(void) { return (unsigned long long)wide_state; }
void wide_compound_reference(void) { wide_state += 5ULL; }
int signed_reference(int stage)
{
    _Atomic(enum SignedState) reference = SIGNED_NEGATIVE;
    reference /= 2;
    if (stage == 1) return (int)reference;
    reference >>= 1;
    if (stage == 2) return (int)reference;
    reference += 9;
    if (stage == 3) return (int)reference;
    reference *= 3;
    if (stage == 4) return (int)reference;
    reference %= 11;
    if (stage == 5) return (int)reference;
    reference ^= 3;
    if (stage == 6) return (int)reference;
    reference &= 7;
    return (int)reference;
}
int signed_mixed_reference(int operation)
{
    _Atomic(enum SignedState) reference = SIGNED_NEGATIVE;
    if (operation == 1) reference /= 4294967296ULL;
    else if (operation == 2) reference %= 4294967296ULL;
    else if (operation == 3) reference += 4294967295ULL;
    else if (operation == 4) reference >>= 1ULL;
    else if (operation == 5) reference /= 4294967296LL;
    return (int)reference;
}
void state_write_reference(void) { state = STATE_READY; }
void state_compound_reference(void) { state += 1; state -= 1; state |= 1; state++; --state; }
