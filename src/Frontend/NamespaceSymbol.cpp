#include "nativesharp/Frontend/NamespaceSymbol.h"

#include <utility>

namespace nativesharp::frontend
{
NamespaceSymbol::NamespaceSymbol(std::string name, std::string fullName, NamespaceSymbol* parent)
    : name_(std::move(name)),
      fullName_(std::move(fullName)),
      parent_(parent)
{
}

std::string_view NamespaceSymbol::name() const noexcept
{
    return name_;
}

std::string_view NamespaceSymbol::fullName() const noexcept
{
    return fullName_;
}

NamespaceSymbol* NamespaceSymbol::parent() const noexcept
{
    return parent_;
}

NamespaceSymbol& NamespaceSymbol::ensureChild(std::string_view name)
{
    if (NamespaceSymbol* existing = findChild(name))
    {
        return *existing;
    }

    std::string fullName = fullName_;
    if (!fullName.empty())
    {
        fullName.push_back('.');
    }
    fullName += name;

    children_.push_back(std::make_unique<NamespaceSymbol>(
        std::string(name),
        std::move(fullName),
        this));
    return *children_.back();
}

NamespaceSymbol* NamespaceSymbol::findChild(std::string_view name) noexcept
{
    for (const std::unique_ptr<NamespaceSymbol>& child : children_)
    {
        if (child->name() == name)
        {
            return child.get();
        }
    }
    return nullptr;
}

const NamespaceSymbol* NamespaceSymbol::findChild(std::string_view name) const noexcept
{
    for (const std::unique_ptr<NamespaceSymbol>& child : children_)
    {
        if (child->name() == name)
        {
            return child.get();
        }
    }
    return nullptr;
}

void NamespaceSymbol::addTypeOrdinal(std::size_t ordinal)
{
    typeOrdinals_.push_back(ordinal);
}

std::span<const std::size_t> NamespaceSymbol::typeOrdinals() const noexcept
{
    return typeOrdinals_;
}

void NamespaceSymbol::addFunctionOrdinal(std::size_t ordinal)
{
    functionOrdinals_.push_back(ordinal);
}

std::span<const std::size_t> NamespaceSymbol::functionOrdinals() const noexcept
{
    return functionOrdinals_;
}

std::span<const std::unique_ptr<NamespaceSymbol>> NamespaceSymbol::children() const noexcept
{
    return children_;
}

GlobalNamespace::GlobalNamespace()
    : root_("", "", nullptr)
{
}

NamespaceSymbol& GlobalNamespace::root() noexcept
{
    return root_;
}

const NamespaceSymbol& GlobalNamespace::root() const noexcept
{
    return root_;
}

NamespaceSymbol& GlobalNamespace::ensureNamespace(std::span<const std::string> segments)
{
    NamespaceSymbol* current = &root_;
    for (const std::string& segment : segments)
    {
        current = &current->ensureChild(segment);
    }
    return *current;
}

NamespaceSymbol* GlobalNamespace::findNamespace(std::span<const std::string> segments) noexcept
{
    NamespaceSymbol* current = &root_;
    for (const std::string& segment : segments)
    {
        current = current->findChild(segment);
        if (current == nullptr)
        {
            return nullptr;
        }
    }
    return current;
}

const NamespaceSymbol* GlobalNamespace::findNamespace(std::span<const std::string> segments) const noexcept
{
    const NamespaceSymbol* current = &root_;
    for (const std::string& segment : segments)
    {
        current = current->findChild(segment);
        if (current == nullptr)
        {
            return nullptr;
        }
    }
    return current;
}
}
