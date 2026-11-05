enum Mood { Sad = -3, Happy = 7 };
enum Mood current_mood(void);
int read_mood(enum Mood value);
typedef enum Mood (*MoodCallback)(enum Mood value);
enum Mood apply_mood(MoodCallback callback, enum Mood value);
