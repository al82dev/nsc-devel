#pragma once

#include <string>

namespace nativesharp::compilation
{
struct PrecompiledProductSidecar final
{
    std::string publicApiSource;
    std::string publicTemplateSource;
    std::string publicApiSyntax;
    std::string publicTemplateSyntax;
};
}
