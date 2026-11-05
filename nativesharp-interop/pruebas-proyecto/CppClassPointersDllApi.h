#ifdef BUILD_POINTER_DLL
#define POINTER_API __declspec(dllexport)
#else
#define POINTER_API __declspec(dllimport)
#endif
namespace Demo {
struct Hidden { int value; };
struct POINTER_API Payload { int value; int* ref; const int* viewRef; int* const fixedRef; Payload* next; const Payload* viewNext; Hidden* hidden; int select(); int select() const; void mutateOnly();
    int mix(int*); int mix(const int*); int mix(int*) const; int mix(const int*) const;
    int cross(int*) const; int cross(const int*);
};
struct POINTER_API Widget {
    static Hidden* sharedHidden;
    static Payload* sharedPayload;
    static const Payload* sharedViewPayload;
    static int* sharedRef;
    static const int* sharedView;
    static int* const sharedFixed;
    int* forward(int* value) const;
    const int* view(const int* value) const;
    static const int* viewStatic(const int* value);
    int classify(int* value) const;
    int classify(const int* value) const;
    static int classifyStatic(int* value);
    static int classifyStatic(const int* value);
    static int* forwardStatic(int* value);
    Payload* forwardPayload(Payload* value) const;
    Payload* getPayload() const;
    const Payload* viewPayload(const Payload* value) const;
    static const Payload* viewPayloadStatic(const Payload* value);
    static Payload* forwardPayloadStatic(Payload* value);
};
POINTER_API Widget* make() noexcept;
POINTER_API int* number() noexcept;
}
