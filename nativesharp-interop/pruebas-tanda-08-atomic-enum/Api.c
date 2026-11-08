#include "Api.h"
_Atomic(enum State) state = STATE_IDLE;
int state_check(void) { return state == STATE_READY; }
void state_write_reference(void) { state = STATE_READY; }
void state_compound_reference(void) { state += 1; state -= 1; state |= 1; }
