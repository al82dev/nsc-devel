typedef void* (*loadproc)(const char* name);
typedef void (*glproc)(void);
typedef glproc (*getproc_fn)(const char* name);
glproc getproc(const char* name);
int load(loadproc proc);
#ifdef LIM_CAST_IMPLEMENTATION
static void dummy(void) {}
glproc getproc(const char* name) { return name[0] == 'g' ? dummy : 0; }
int load(loadproc proc) { return proc("gl") != 0 ? 42 : 1; }
#endif
