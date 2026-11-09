/* T08-D20: C control for a DLL whose TLS directory has SizeOfZeroFill 5 (prepare_zero_fill.py).
 * The Windows loader copies the raw TLS template but leaves the zero-fill bytes of each thread's block as they come
 * from the heap (observed: a heap pointer where tls_zero lives), so their initial value is not checked. The control
 * checks that the DLL loads and that its own check sees the raw-data values, not the ones NativeSharp writes.
 * T08-D20: el cargador de Windows no pone en cero los bytes de relleno; su valor inicial no se comprueba. */
__declspec(dllimport) int tls_scalar_check(void);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void) { ExitProcess(tls_scalar_check() == 1 ? 0 : 1); }
