#pragma once

#include "nativesharp/Frontend/NamespaceSymbol.h"

#include <span>
#include <string>

namespace nativesharp::frontend
{
class GlobalNamespace final
{
public:
    GlobalNamespace();

    NamespaceSymbol& root() noexcept;
    const NamespaceSymbol& root() const noexcept;

    NamespaceSymbol& ensureNamespace(std::span<const std::string> segments);
    NamespaceSymbol* findNamespace(std::span<const std::string> segments) noexcept;
    const NamespaceSymbol* findNamespace(std::span<const std::string> segments) const noexcept;

private:
    NamespaceSymbol root_;
};
}
