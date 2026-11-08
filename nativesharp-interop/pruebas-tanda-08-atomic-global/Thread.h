int start_atomic_worker(void);
int wait_atomic_worker(void);
extern _Atomic(int) atomic_turn;
void yield_atomic_worker(void);
