#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
class NamespaceSymbol final
{
public:
    NamespaceSymbol(std::string name, std::string fullName, NamespaceSymbol* parent);

    std::string_view name() const noexcept;
    std::string_view fullName() const noexcept;
    NamespaceSymbol* parent() const noexcept;

    NamespaceSymbol& ensureChild(std::string_view name);
    NamespaceSymbol* findChild(std::string_view name) noexcept;
    const NamespaceSymbol* findChild(std::string_view name) const noexcept;

    void addTypeOrdinal(std::size_t ordinal);
    std::span<const std::size_t> typeOrdinals() const noexcept;

    void addFunctionOrdinal(std::size_t ordinal);
    std::span<const std::size_t> functionOrdinals() const noexcept;
    std::span<const std::unique_ptr<NamespaceSymbol>> children() const noexcept;

private:
    std::string name_;
    std::string fullName_;
    NamespaceSymbol* parent_ = nullptr;
    std::vector<std::unique_ptr<NamespaceSymbol>> children_;
    std::vector<std::size_t> typeOrdinals_;
    std::vector<std::size_t> functionOrdinals_;
};

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
