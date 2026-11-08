enum State { STATE_IDLE = 0, STATE_READY = 1 };
extern _Atomic(enum State) state;
int state_check(void);
