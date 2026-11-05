#include "Api.h"
enum Mood current_mood(void) { return Sad; }
int read_mood(enum Mood value) { return (int)value; }
enum Mood apply_mood(MoodCallback callback, enum Mood value) { return callback(value); }
static struct MoodData data = { Happy, { Happy, Happy } };
struct MoodData* get_mood_data(void) { return &data; }
int read_mood_data(struct MoodData* value) { return (int)value->value + (int)value->values[1]; }
enum Mood shared_mood = Happy;
int read_shared_mood(void) { return (int)shared_mood; }
const enum Mood readonly_mood = Sad;
enum HiddenState hidden_global = HiddenReady;
int read_hidden_raw(void) { return (int)hidden_global; }
