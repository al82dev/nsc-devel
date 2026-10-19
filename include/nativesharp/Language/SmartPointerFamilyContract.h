#pragma once

#include "nativesharp/Language/SmartPointerFamily.h"
#include "nativesharp/Language/SmartPointerThreadModel.h"

#include <string_view>

namespace nativesharp::language
{
struct SmartPointerFamilyContract final
{
    SmartPointerFamily family;
    std::string_view name;
    SmartPointerThreadModel threadModel;
    bool nullable;
    bool implicitCopy;
    bool explicitClone;
    bool weak;
};
}
