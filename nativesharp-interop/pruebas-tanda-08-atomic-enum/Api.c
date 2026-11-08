#include "Api.h"
_Atomic(enum State) state = STATE_IDLE;
int state_check(void) { return state == STATE_READY; }
