#pragma once

#include <string>

namespace nativesharp::compilation
{
struct SdkHeaderMapping final
{
    std::string include;
    std::string targetNamespace;
};
}
