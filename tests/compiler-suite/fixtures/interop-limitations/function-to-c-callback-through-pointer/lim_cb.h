/* GLAD-like: a function-pointer entry point (glDebugMessageCallback) that takes a callback (GLDEBUGPROC). */
#ifndef LIM_CB_H
#define LIM_CB_H
typedef void (*LIMDEBUGPROC)(unsigned int source, unsigned int type, int length, const char* message,
                             const void* userParam);
typedef int (*PFNLIMSETCALLBACKPROC)(LIMDEBUGPROC callback, const void* userParam);
extern PFNLIMSETCALLBACKPROC lim_set_callback;
#define limSetCallback lim_set_callback
#ifdef LIM_CB_IMPLEMENTATION
static int lim_set_callback_impl(LIMDEBUGPROC callback, const void* userParam)
{
    callback(1u, 2u, 3, "msg", userParam);
    callback(10u, 20u, 4, "more", userParam);
    return 7;
}
PFNLIMSETCALLBACKPROC lim_set_callback = lim_set_callback_impl;
#endif
#endif
