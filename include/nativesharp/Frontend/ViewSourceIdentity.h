#pragma once

#include "nativesharp/Frontend/ViewSourceIdentityKind.h"

#include <cstddef>
#include <string>

namespace nativesharp::frontend
{
struct ViewSourceIdentity final
{
    std::string name;
    ViewSourceIdentityKind kind = ViewSourceIdentityKind::Unknown;
    std::size_t ordinal = 0;
    bool operator==(const ViewSourceIdentity&) const = default;
};
}
