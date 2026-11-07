#ifdef BUILD_BOOL_DLL
#define BOOL_API __declspec(dllexport)
#else
#define BOOL_API __declspec(dllimport)
#endif
extern BOOL_API volatile _Bool flag;
BOOL_API int c_read_flag(void);
BOOL_API void c_write_flag(int value);
