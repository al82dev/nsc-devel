__declspec(thread) int plain_tls = 42;
int plain_c(void) { return plain_tls; }
