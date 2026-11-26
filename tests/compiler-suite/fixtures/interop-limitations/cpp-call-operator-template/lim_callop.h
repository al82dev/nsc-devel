// The shape of std::uniform_*_distribution: `operator()` is a member function template that takes the engine by
// non-const reference and advances it.
#ifndef LIM_CALLOP_H
#define LIM_CALLOP_H
struct LimEngine
{
    unsigned state;
    LimEngine() : state(1u) {}
    unsigned operator()() { state = state * 3u; return state; }
};
struct LimDistribution
{
    unsigned offset;
    explicit LimDistribution(unsigned value) : offset(value) {}
    template <class Engine>
    unsigned operator()(Engine& engine) const { return engine() + offset; }
};
#endif
