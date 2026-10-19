#pragma once

#include <string_view>

namespace nativesharp::frontend::internal
{
struct ComptimeCapabilityCall final
{
    std::string_view name;
    std::string_view kind;
};
}
