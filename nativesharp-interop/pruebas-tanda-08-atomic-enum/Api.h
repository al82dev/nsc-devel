enum State { STATE_IDLE = 0, STATE_READY = 1 };
enum SignedState { SIGNED_NEGATIVE = -8, SIGNED_ZERO = 0 };
extern _Atomic(enum State) state;
extern _Atomic(enum SignedState) signed_state;
int state_check(void);
int signed_reference(int stage);
int signed_mixed_reference(int operation);
