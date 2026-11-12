#pragma once
typedef struct PairMixed { float x; double y; } PairMixed;
void __attribute__((regcall)) write_pair(PairMixed value, int* out);
typedef void (__attribute__((regcall)) *WriteFn)(PairMixed, int*);
WriteFn get_write(void);
