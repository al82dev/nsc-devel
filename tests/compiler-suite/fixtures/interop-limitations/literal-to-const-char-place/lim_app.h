/* Vulkan-like C struct with `const char*` fields (VkApplicationInfo). */
#ifndef LIM_APP_H
#define LIM_APP_H
typedef struct LimAppInfo { const char* pApplicationName; const char* pEngineName; int version; } LimAppInfo;
int lim_name_length(const LimAppInfo* info);
#ifdef LIM_APP_IMPLEMENTATION
int lim_name_length(const LimAppInfo* info)
{
    int n = 0;
    while (info->pApplicationName[n] != 0) n = n + 1;
    int m = 0;
    while (info->pEngineName[m] != 0) m = m + 1;
    return n + m;
}
#endif
#endif
