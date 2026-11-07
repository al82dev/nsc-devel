typedef struct { int raw; } CAnonRecord;
typedef enum { CAnonKindFirst = 5, CAnonKindSecond = 6 } CAnonKind;
enum { CAnonLooseFirst = 7, CAnonLooseSecond = 8 };
typedef struct { int raw; enum { CAnonLevelLow = 1, CAnonLevelHigh = 2 } level; } CAnonLevelRecord;
CAnonRecord* get_anon_record(void);
CAnonKind get_anon_kind(void);
CAnonLevelRecord* get_anon_level_record(void);
