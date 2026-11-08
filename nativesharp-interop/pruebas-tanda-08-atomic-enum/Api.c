#include "Api.h"
_Atomic(enum State) state = STATE_IDLE;
_Atomic(enum SignedState) signed_state = SIGNED_NEGATIVE;
int state_check(void) { return state == STATE_READY; }
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
void state_write_reference(void) { state = STATE_READY; }
void state_compound_reference(void) { state += 1; state -= 1; state |= 1; state++; --state; }
