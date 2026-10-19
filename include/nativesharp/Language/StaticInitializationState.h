#pragma once

namespace nativesharp::language
{
enum class StaticInitializationState
{
    Uninitialized,
    Initializing,
    Initialized,
    Failed
};
}
