extern _Atomic(signed char*) width_i8;
extern _Atomic(unsigned char*) width_u8;
extern _Atomic(unsigned short*) width_u16;
extern _Atomic(unsigned long long*) width_u64;
signed char* width_i8_expected(void);
unsigned char* width_u8_expected(void);
unsigned short* width_u16_expected(void);
unsigned long long* width_u64_expected(void);
int widths_check(void);
