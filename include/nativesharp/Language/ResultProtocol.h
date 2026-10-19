#pragma once

#include <string>

namespace nativesharp::language
{
struct ResultProtocol final
{
    std::string successType;
    std::string errorType;
    bool hasIsSuccess = true;
    bool hasTakeSuccess = true;
    bool hasTakeError = true;
};
}
