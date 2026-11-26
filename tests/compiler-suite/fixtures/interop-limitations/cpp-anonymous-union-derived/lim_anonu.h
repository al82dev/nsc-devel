// Bullet's btTypedConstraint: an anonymous union in a C++ class derived from another one.
#ifndef LIM_ANONU_H
#define LIM_ANONU_H
struct LimTyped
{
    int objectType;
    explicit LimTyped(int type) : objectType(type) {}
};
class LimConstraint : public LimTyped
{
    int userType;
    union {
        int userId;
        void* userPtr;
    };
    float threshold;
public:
    explicit LimConstraint(int id) : LimTyped(3), userType(1), userId(id), threshold(2.0f) {}
    int GetUserId() const { return userId; }
    int GetType() const { return objectType + userType; }
};
#endif
