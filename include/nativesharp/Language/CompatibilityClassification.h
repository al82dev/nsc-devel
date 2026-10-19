#pragma once

#include "nativesharp/Language/AbiCompatibility.h"
#include "nativesharp/Language/SourceCompatibility.h"

namespace nativesharp::language
{
struct CompatibilityClassification final
{
    SourceCompatibility source;
    AbiCompatibility abi;
    bool requiresConsumerRebuild;
};
}
