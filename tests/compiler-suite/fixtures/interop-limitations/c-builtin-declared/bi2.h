#ifndef BI2_H
#define BI2_H
void __debugbreak(void);
int bi2_one(void);
#ifdef BI2_IMPL
int bi2_one(void) { return 0; }
#endif
#endif
