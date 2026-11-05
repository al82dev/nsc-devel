struct VBase { virtual ~VBase(); };
struct Derived : virtual VBase { ~Derived(); };
VBase::~VBase() {}
Derived::~Derived() {}
void destroy(Derived* value) { value->Derived::~Derived(); }
