int HostCallback(int value);
static inline int call_host(int value) { return HostCallback(value); }
