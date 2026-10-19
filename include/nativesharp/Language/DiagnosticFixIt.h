#pragma once

#include <string>

namespace nativesharp::language
{
struct DiagnosticFixIt final
{
    std::string replacement;
    bool preservesSemantics = false;
    bool compiles = false;
};
}
