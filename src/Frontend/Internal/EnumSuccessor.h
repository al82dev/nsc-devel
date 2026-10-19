#pragma once

#include "EnumValue.h"

namespace nativesharp::frontend::internal
{
struct EnumSuccessor
{
    EnumValue value;
    bool hasValue = true;
};
}
