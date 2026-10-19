#pragma once

#include "nativesharp/Language/ThrowAbi.h"

#include <string>

namespace nativesharp::language
{
struct ThrowContract final
{
    std::string errorType;
    ThrowAbi abi = ThrowAbi::ResultCarrier;
    bool crossesCAbi = false;
    bool hasAdapter = false;
};
}
