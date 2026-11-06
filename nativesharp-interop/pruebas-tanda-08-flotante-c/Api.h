typedef long double CLongDouble;
typedef struct CFloatRecord { long double value; } CFloatRecord;
CLongDouble add_long_double(CLongDouble a, CLongDouble b);
CFloatRecord* get_float_record(void);
int check_float_record(CFloatRecord* record);
