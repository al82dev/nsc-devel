#pragma once

#include "nativesharp/Driver/DeprecationState.h"

namespace nativesharp::driver
{
struct DeprecatedFeature;

struct DeprecationDecision final
{
    const DeprecatedFeature *feature = nullptr;
    DeprecationState state = DeprecationState::Warning;
};
} // namespace nativesharp::driver
