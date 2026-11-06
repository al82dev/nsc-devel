enum CGlobalKind { CGlobalChoice = 21 };
typedef struct CRecord { int value; enum CGlobalKind kind; } CRecord;
CRecord* get_c_record(void);
CRecord get_c_record_value(void);
enum CGlobalKind get_c_kind(void);
int get_c_number(void);
extern int shared_c_number;
int get_shared_c_number(void);
