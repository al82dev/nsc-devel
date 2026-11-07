#include "Api.h"
int main(void) {
    if (point_ptr->value != 3 || *mood_ptr != MoodLow || callback_ptr(5) != 12) return 1;
    point_ptr->value = 12;
    *mood_ptr = high_mood();
    return inspect() ? 0 : 2;
}
