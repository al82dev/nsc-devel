#pragma once

namespace nativesharp::language
{
enum class ThreadSafetyMemberKind
{
    Value,
    Atomic,
    SharedReference,
    MutableReference,
    RawPointer,
    DynamicTrait
};
}
