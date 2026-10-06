#pragma once

#include "nativesharp/AST/AstNode.h"

#include <cstddef>
#include <optional>
#include <unordered_map>

namespace nativesharp::frontend
{
// Provides average O(1) lookup for a binding owned by one callable and syntax node.
//
// Ownership: the index does not own syntax nodes or bindings; it stores stable binding ordinals.
// Thread safety: one builder may mutate the index. Concurrent reads are allowed only after the
// index has been published as immutable.
// Replacement: recording the same composite key again preserves newest-binding-wins semantics.
class ProjectCallableBindingIndex final
{
public:
    void clear() noexcept;

    // Records the ordinal of the newest binding for a syntax/callable pair.
    // A null syntax node is ignored because it cannot be queried unambiguously.
    // Average complexity: O(1). Worst case follows std::unordered_map collision behavior.
    void record(
        std::size_t callableOrdinal,
        const ast::AstNode* syntax,
        std::size_t bindingOrdinal);

    // Returns the newest binding ordinal for the exact composite key.
    // Average complexity: O(1). The method does not allocate or mutate the index.
    [[nodiscard]] std::optional<std::size_t> latest(
        const ast::AstNode* syntax,
        std::size_t callableOrdinal) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::unordered_map<
        const ast::AstNode*,
        std::unordered_map<std::size_t, std::size_t>> bindingOrdinalsBySyntaxAndCallable_;
    std::size_t size_ = 0;
};
}
