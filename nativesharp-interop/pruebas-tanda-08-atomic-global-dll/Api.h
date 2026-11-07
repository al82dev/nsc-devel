#ifdef BUILD_ATOMIC_DLL
#define ATOMIC_API __declspec(dllexport)
#else
#define ATOMIC_API __declspec(dllimport)
#endif
extern ATOMIC_API _Atomic(int) atomic_global;
ATOMIC_API int c_read_atomic_global(void);
