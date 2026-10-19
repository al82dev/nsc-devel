#pragma once

#include <string>

#include "nativesharp/Language/AttributeRetention.h"

namespace nativesharp::language
{
struct AttributeUse final
{
    std::string identity;
    std::string target;
    bool repeatable = false;
    AttributeRetention retention = AttributeRetention::CompileTime;
};
}
