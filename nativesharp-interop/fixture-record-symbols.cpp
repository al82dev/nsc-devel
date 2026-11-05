class MyClass {
    int hidden;
public:
    int count;
    static int shared;
    MyClass(int);
    ~MyClass();
    int A(int) const;
    static int B(double);
    virtual int C();
};
int MyClass::shared = 42;
MyClass::MyClass(int value) : hidden(value), count(0) {}
MyClass::~MyClass() {}
int MyClass::A(int value) const { return hidden + value; }
int MyClass::B(double value) { return static_cast<int>(value); }
int MyClass::C() { return count; }
