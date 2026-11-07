typedef struct { int raw; enum { CNestLow = 1, CNestHigh = 2 } level; } CNestAnonField;
typedef struct { int raw; enum CNestNamed { CNestNamedA = 3, CNestNamedB = 4 } kind; } CNestNamedField;
CNestAnonField* get_nest_anon(void);
CNestNamedField* get_nest_named(void);
