#include "nativesharp/Frontend/ProjectCallableBindingIndex.h"

namespace nativesharp::frontend
{
void ProjectCallableBindingIndex::clear() noexcept
{
    bindingOrdinalsBySyntaxAndCallable_.clear();
    size_ = 0;
}

void ProjectCallableBindingIndex::record(
    std::size_t callableOrdinal,
    const ast::AstNode* syntax,
    std::size_t bindingOrdinal)
{
    if (syntax == nullptr)
    {
        return;
    }

    auto& bindingsByCallable = bindingOrdinalsBySyntaxAndCallable_[syntax];
    const auto [ignored, inserted] = bindingsByCallable.insert_or_assign(
        callableOrdinal,
        bindingOrdinal);
    static_cast<void>(ignored);
    if (inserted)
    {
        ++size_;
    }
}

std::optional<std::size_t> ProjectCallableBindingIndex::latest(
    const ast::AstNode* syntax,
    std::size_t callableOrdinal) const noexcept
{
    const auto syntaxIterator = bindingOrdinalsBySyntaxAndCallable_.find(syntax);
    if (syntaxIterator == bindingOrdinalsBySyntaxAndCallable_.end())
    {
        return std::nullopt;
    }

    const auto callableIterator = syntaxIterator->second.find(callableOrdinal);
    if (callableIterator == syntaxIterator->second.end())
    {
        return std::nullopt;
    }
    return callableIterator->second;
}

std::size_t ProjectCallableBindingIndex::size() const noexcept
{
    return size_;
}
}
