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
enum Mood* shared_mood_ptr = &shared_mood;
volatile enum Mood volatile_mood = Happy;
int read_volatile_mood(void) { return (int)volatile_mood; }
enum HiddenState hidden_global = HiddenReady;
int read_hidden_raw(void) { return (int)hidden_global; }
static enum PointerState pointer_value = PointerReady;
enum PointerState* pointer_global_only = &pointer_value;
static enum DoublePointerState double_pointer_value = DoublePointerReady;
static enum DoublePointerState* double_pointer_inner = &double_pointer_value;
enum DoublePointerState** double_pointer_global_only = &double_pointer_inner;
int read_pointer_globals(void) { return (int)*pointer_global_only + (int)**double_pointer_global_only; }
enum ArrayState array_states[2] = { ArrayLow, ArrayHigh };
enum MatrixState matrix_states[2][2] = { { MatrixLow, MatrixLow }, { MatrixLow, MatrixHigh } };
int read_state_arrays(void) { return (int)array_states[0] + (int)matrix_states[1][0]; }
