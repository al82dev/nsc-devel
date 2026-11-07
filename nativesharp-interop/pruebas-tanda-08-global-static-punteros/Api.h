typedef struct StaticPoint { int value; } StaticPoint;
enum StaticMood { MoodLow = 2, MoodHigh = 8 };
typedef int (*StaticCallback)(int);
static StaticPoint point = { 3 };
static enum StaticMood mood = MoodLow;
static int add_seven(int value) { return value + 7; }
static StaticPoint* point_ptr = &point;
static enum StaticMood* mood_ptr = &mood;
static StaticCallback callback_ptr = add_seven;
static inline enum StaticMood high_mood(void) { return MoodHigh; }
static inline int inspect(void) {
    return point.value == 12 && mood == MoodHigh && callback_ptr(5) == 12;
}
