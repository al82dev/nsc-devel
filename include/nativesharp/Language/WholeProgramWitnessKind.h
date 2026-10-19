#pragma once

namespace nativesharp::language
{
enum class WholeProgramWitnessKind
{
    Parser,
    ArenaAllocator,
    JobGraph,
    BinarySerializer,
    DllHostPlugin,
    AsyncServer
};
}
