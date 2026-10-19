#pragma once

#include <string>

namespace nativesharp::language
{
struct PromotedField final
{
    std::string name;
    std::string typeKey;
    bool isPublic = false;
    bool isReadonly = false;
};
}
