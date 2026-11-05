#include "Api.h"
enum Mood current_mood(void) { return Sad; }
int read_mood(enum Mood value) { return (int)value; }
