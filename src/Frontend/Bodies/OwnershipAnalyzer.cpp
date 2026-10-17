#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/TraitSemantics.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
using method_body_detail::pointerContract;

namespace
{
bool declaredLifetimeOutlives(
    const ClassMemberSymbol& callable,
    std::string_view longer,
    std::string_view shorter)
{
    if (longer == shorter || longer == "static") return true;
    std::unordered_set<std::string> visited;
    std::vector<std::string> pending{std::string(longer)};
    while (!pending.empty())
    {
        std::string current = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(current).second) continue;
        for (const auto& [declaredLonger, declaredShorter] :
             callable.lifetimeOutlivesConstraints)
        {
            if (declaredLonger != current) continue;
            if (declaredShorter == shorter) return true;
            pending.push_back(declaredShorter);
        }
    }
    return false;
}
}
std::optional<std::string> ProjectMethodBodyModel::viewRootLocalOfExpression(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    // Alias chains are data, not control flow. Keep them on the heap-backed worklist so a
    // generated or adversarial source cannot turn the semantic query into native-stack growth.
    // Reverse insertion preserves the former recursive preorder exactly.
    std::vector<const ast::AstNode*> work;
    std::unordered_set<const ast::AstNode*> visited;
    if (expression != nullptr) work.push_back(expression);
    while (!work.empty())
    {
        const ast::AstNode* current = work.back();
        work.pop_back();
        if (current == nullptr || !visited.insert(current).second) continue;

        if (current->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, current->text());
            if (name.has_value() && name->kind == ScopeNameKind::Local &&
                name->semanticOrdinal < locals_.size())
            {
                const MethodLocalSymbol& local = locals_[name->semanticOrdinal];
                if (typeKeyIsViewLike(local.typeKey))
                {
                    if (!local.viewRootLocalName.empty()) return local.viewRootLocalName;
                }
                else if (local.typeKey.ends_with("*"))
                {
                    // A raw pointer local is a VALUE slot, not the storage it points at. Follow
                    // its initializer instead of declaring every pointer local frame-bound.
                    if (local.variableLengthArrayRootLocalOrdinal.has_value() &&
                        *local.variableLengthArrayRootLocalOrdinal < locals_.size())
                        return locals_[*local.variableLengthArrayRootLocalOrdinal].name;
                    const ast::AstNode* initializer = local.syntax != nullptr
                        ? local.syntax->first() : nullptr;
                    if (initializer == nullptr) continue;
                    if (initializer->kind() == ast::AstNodeKind::StackallocExpression)
                        return local.name;
                    if (initializer->kind() == ast::AstNodeKind::UnaryExpression &&
                        initializer->tokenKind() == lexer::TokenKind::Amp &&
                        initializer->first() != nullptr &&
                        initializer->first()->kind() == ast::AstNodeKind::IdentifierExpression)
                    {
                        const std::optional<ScopeName> pointee = lookupScopeName(
                            scopeOrdinal, initializer->first()->text());
                        if (pointee.has_value() && pointee->kind == ScopeNameKind::Local &&
                            pointee->semanticOrdinal < locals_.size())
                            return locals_[pointee->semanticOrdinal].name;
                    }
                    work.push_back(initializer);
                }
                else if (typeKeyCarriesLifetime(local.typeKey))
                {
                    return local.name;
                }
            }
            continue;
        }

        const std::span<ast::AstNode* const> children = current->children();
        for (auto child = children.rbegin(); child != children.rend(); ++child)
            work.push_back(*child);
        work.push_back(current->third());
        work.push_back(current->second());
        work.push_back(current->first());
    }
    return std::nullopt;
}

std::optional<std::size_t> ProjectMethodBodyModel::variableArrayRootLocalOfExpression(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    std::vector<const ast::AstNode*> work;
    if (expression != nullptr) work.push_back(expression);
    while (!work.empty())
    {
        const ast::AstNode* current = work.back();
        work.pop_back();
        if (current == nullptr) continue;
        if (current->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, current->text());
            if (name.has_value() && name->kind == ScopeNameKind::Local &&
                name->semanticOrdinal < locals_.size())
            {
                const std::optional<std::size_t> root =
                    locals_[name->semanticOrdinal].variableLengthArrayRootLocalOrdinal;
                if (root.has_value()) return root;
            }
        }

        const std::span<ast::AstNode* const> children = current->children();
        for (auto child = children.rbegin(); child != children.rend(); ++child)
            work.push_back(*child);
        work.push_back(current->third());
        work.push_back(current->second());
        work.push_back(current->first());
    }
    return std::nullopt;
}

std::optional<std::string> ProjectMethodBodyModel::partialMovePlaceKey(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal,
    std::vector<std::string>* fieldPath) const
{
    if (fieldPath != nullptr) fieldPath->clear();
    std::vector<std::string> reversePath;
    const ast::AstNode* root = expression;
    while (root != nullptr && root->kind() == ast::AstNodeKind::MemberExpression)
    {
        reversePath.push_back(root->text());
        root = root->first();
    }
    if (root == nullptr || root->kind() != ast::AstNodeKind::IdentifierExpression)
        return std::nullopt;
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, root->text());
    if (!name.has_value()) return std::nullopt;

    bool moveOnlyRoot = false;
    if (name->kind == ScopeNameKind::Local)
    {
        moveOnlyRoot = name->semanticOrdinal < locals_.size() &&
            locals_[name->semanticOrdinal].isMoveOnly;
    }
    else
    {
        const std::size_t callable = scopeOrdinal < scopes_.size()
            ? scopes_[scopeOrdinal].methodMemberOrdinal : 0U;
        const ClassMemberSymbol* method = methodSymbol(callable);
        moveOnlyRoot = method != nullptr && name->semanticOrdinal < method->parameters.size() &&
            method->parameters[name->semanticOrdinal].passKind == ParameterPassKind::Value &&
            memberModel_ != nullptr &&
            memberModel_->typeIsMoveOnly(method->parameters[name->semanticOrdinal].typeKey);
    }
    if (!moveOnlyRoot) return std::nullopt;

    std::ranges::reverse(reversePath);
    if (fieldPath != nullptr) *fieldPath = reversePath;
    std::string key = name->kind == ScopeNameKind::Local ? "L" : "P";
    key += std::to_string(name->semanticOrdinal);
    for (const std::string& field : reversePath)
    {
        key += '/';
        key += field;
    }
    return key;
}

std::optional<std::string> ProjectMethodBodyModel::storagePlaceKey(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal,
    bool* precise) const
{
    if (precise != nullptr) *precise = true;
    if (expression == nullptr) return std::nullopt;
    const std::size_t callableOrdinal = scopeOrdinal < scopes_.size()
        ? scopes_[scopeOrdinal].methodMemberOrdinal
        : std::numeric_limits<std::size_t>::max();

    const auto integerConstant = [&](const ast::AstNode* syntax) -> std::optional<std::int64_t>
    {
        if (syntax == nullptr) return std::nullopt;
        if (syntax->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, syntax->text());
            if (name.has_value() && name->kind == ScopeNameKind::Local &&
                name->semanticOrdinal < locals_.size() &&
                locals_[name->semanticOrdinal].constantIntegerValue.has_value())
            {
                return locals_[name->semanticOrdinal].constantIntegerValue;
            }
        }
        const MethodExpressionTypeBinding* binding =
            expressionTypeBinding(syntax, callableOrdinal);
        if (binding != nullptr && binding->integerConstant.has_value())
        {
            const ProjectIntegerConstant& value = *binding->integerConstant;
            if (value.isNegative)
            {
                if (value.value > (std::uint64_t{1} << 63U)) return std::nullopt;
                if (value.value == (std::uint64_t{1} << 63U))
                    return std::numeric_limits<std::int64_t>::min();
                return -static_cast<std::int64_t>(value.value);
            }
            if (value.value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                return std::nullopt;
            return static_cast<std::int64_t>(value.value);
        }
        return std::nullopt;
    };
    const auto recordedTypeKey = [&](const ast::AstNode* syntax) -> std::string_view
    {
        const MethodExpressionTypeBinding* binding =
            expressionTypeBinding(syntax, callableOrdinal);
        return binding != nullptr ? std::string_view(binding->typeKey) : std::string_view{};
    };

    // F04: a reference-returning call borrows the place named by its lifetime contract. Keep the
    // old conservative unknown-place fallback when several eligible arguments name different
    // places; a single string key must never pretend that a conditional return has one origin.
    if (expression->kind() == ast::AstNodeKind::CallExpression ||
        expression->kind() == ast::AstNodeKind::IndexExpression)
    {
        const MethodCallBinding* binding = callBinding(expression, callableOrdinal);
        if (binding != nullptr &&
            (binding->resolution != MethodCallResolutionKind::Resolved ||
             !binding->selectedMemberOrdinal.has_value()))
        {
            binding = nullptr;
        }
        if (binding != nullptr && memberModel_ != nullptr &&
            *binding->selectedMemberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& callee =
                memberModel_->members()[*binding->selectedMemberOrdinal];
            if (callee.isReferenceReturn)
            {
                if (callee.returnLifetimeName == "this")
                {
                    const std::optional<std::string> receiver =
                        storagePlaceKey(binding->receiverSyntax, scopeOrdinal, precise);
                    if (!receiver.has_value() && precise != nullptr) *precise = false;
                    return receiver;
                }
                if (callee.returnLifetimeName == "static")
                {
                    if (precise != nullptr) *precise = false;
                    return std::nullopt;
                }

                std::optional<std::string> result;
                bool resultPrecise = true;
                for (std::size_t index = 0; index < expression->children().size(); ++index)
                {
                    const std::size_t parameterIndex =
                        index < binding->selectedArgumentParameterOrdinals.size()
                        ? binding->selectedArgumentParameterOrdinals[index] : index;
                    if (parameterIndex >= callee.parameters.size() ||
                        !declaredLifetimeOutlives(
                            callee,
                            callee.parameters[parameterIndex].lifetimeName,
                            callee.returnLifetimeName))
                        continue;

                    bool argumentPrecise = true;
                    const std::optional<std::string> argument = storagePlaceKey(
                        expression->children()[index], scopeOrdinal, &argumentPrecise);
                    if (!argument.has_value() || !argumentPrecise ||
                        (result.has_value() && *result != *argument))
                    {
                        if (precise != nullptr) *precise = false;
                        return std::nullopt;
                    }
                    result = *argument;
                    resultPrecise = resultPrecise && argumentPrecise;
                }
                if (result.has_value())
                {
                    if (precise != nullptr) *precise = resultPrecise;
                    return result;
                }
                if (precise != nullptr) *precise = false;
                return std::nullopt;
            }
        }
    }

    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
        {
            const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, expression->text());
            if (!name.has_value()) return std::nullopt;
            if (name->kind == ScopeNameKind::Local)
            {
                if (name->semanticOrdinal >= locals_.size()) return std::nullopt;
                const MethodLocalSymbol& local = locals_[name->semanticOrdinal];
                if (local.isReference)
                {
                    if (local.referencePlaceKey.empty())
                    {
                        if (precise != nullptr) *precise = false;
                        return std::nullopt;
                    }
                    return local.referencePlaceKey;
                }
                return "L" + std::to_string(name->semanticOrdinal);
            }
            return "P" + std::to_string(name->semanticOrdinal);
        }
        case ast::AstNodeKind::MemberExpression:
        {
            bool receiverPrecise = true;
            std::optional<std::string> key = storagePlaceKey(
                expression->first(), scopeOrdinal, &receiverPrecise);
            if (!key.has_value())
            {
                if (precise != nullptr) *precise = false;
                return std::nullopt;
            }
            const std::string_view receiverTypeKey = recordedTypeKey(expression->first());
            bool overlappingLayout = aggregateIsUnion(receiverTypeKey);
            if (!overlappingLayout && memberModel_ != nullptr)
            {
                const std::optional<std::size_t> typeOrdinal =
                    namedTypeOrdinalForTypeKey(receiverTypeKey);
                if (typeOrdinal.has_value())
                {
                    overlappingLayout = std::ranges::any_of(
                        memberModel_->members(), [&](const ClassMemberSymbol& member)
                        {
                            return member.declaringTypeOrdinal == *typeOrdinal &&
                                member.kind == ClassMemberKind::Field && !member.isStatic &&
                                member.explicitFieldOffset.has_value();
                        });
                }
            }
            *key += overlappingLayout ? "/F:*" : "/F:" + expression->text();
            if (precise != nullptr) *precise = receiverPrecise && !overlappingLayout;
            return key;
        }
        case ast::AstNodeKind::IndexExpression:
        {
            bool basePrecise = true;
            std::optional<std::string> key = storagePlaceKey(
                expression->first(), scopeOrdinal, &basePrecise);
            if (!key.has_value())
            {
                if (precise != nullptr) *precise = false;
                return std::nullopt;
            }
            bool indicesPrecise = true;
            const std::string_view baseTypeKey = recordedTypeKey(expression->first());
            const bool structurallySplittable =
                ProjectTypeConversion::isNativeArray(baseTypeKey) ||
                ProjectTypeConversion::isPointer(baseTypeKey) ||
                ProjectMemberModel::fixedArrayElementTypeKey(baseTypeKey).has_value() ||
                ProjectMemberModel::flexibleArrayElementTypeKey(baseTypeKey).has_value();
            const auto appendIndex = [&](const ast::AstNode* index)
            {
                // An arbitrary user indexer may map every written index to the same storage.
                // Only builtin/raw/fixed array projections have a structural disjointness
                // guarantee; all protocol/indexer cases deliberately collapse to a wildcard.
                if (!structurallySplittable)
                {
                    *key += "/I:*";
                    indicesPrecise = false;
                    return;
                }
                if (index != nullptr && index->kind() == ast::AstNodeKind::RangeExpression)
                {
                    const std::optional<std::int64_t> start = index->first() != nullptr
                        ? integerConstant(index->first()) : std::optional<std::int64_t>{};
                    const std::optional<std::int64_t> end = index->second() != nullptr
                        ? integerConstant(index->second()) : std::optional<std::int64_t>{};
                    const bool exact = (index->first() == nullptr || start.has_value()) &&
                        (index->second() == nullptr || end.has_value());
                    if (!exact)
                    {
                        *key += "/S:*";
                        indicesPrecise = false;
                        return;
                    }
                    *key += "/S:";
                    *key += start.has_value() ? std::to_string(*start) : "_";
                    *key += ':';
                    *key += end.has_value() ? std::to_string(*end) : "_";
                    return;
                }
                const std::optional<std::int64_t> value = integerConstant(index);
                if (!value.has_value())
                {
                    *key += "/I:*";
                    indicesPrecise = false;
                    return;
                }
                *key += "/I:" + std::to_string(*value);
            };
            if (!expression->children().empty())
            {
                for (const ast::AstNode* index : expression->children()) appendIndex(index);
            }
            else
            {
                appendIndex(expression->second());
            }
            if (precise != nullptr) *precise = basePrecise && indicesPrecise;
            return key;
        }
        default:
            break;
    }

    // Parentheses do not survive as a distinct AST node. Calls, dereferences and arbitrary
    // expressions have no statically comparable place root here; inventing one from first()
    // would incorrectly equate a returned reference with the call receiver or first argument.
    if (precise != nullptr) *precise = false;
    return std::nullopt;
}

bool ProjectMethodBodyModel::placeContains(
    std::string_view ancestor, std::string_view descendant) noexcept
{
    return ancestor == descendant ||
        (descendant.size() > ancestor.size() && descendant.starts_with(ancestor) &&
         descendant[ancestor.size()] == '/');
}

bool ProjectMethodBodyModel::placesOverlap(
    std::string_view left, std::string_view right) noexcept
{
    const auto nextSegment = [](std::string_view& value) noexcept
    {
        const std::size_t separator = value.find('/');
        const std::string_view segment = value.substr(0, separator);
        value = separator == std::string_view::npos
            ? std::string_view{} : value.substr(separator + 1U);
        return segment;
    };
    const auto parseNumber = [](std::string_view text) noexcept -> std::optional<std::int64_t>
    {
        std::int64_t value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return std::nullopt;
        return value;
    };
    const auto parseSlice = [&](std::string_view segment,
                                std::optional<std::int64_t>& start,
                                std::optional<std::int64_t>& end) noexcept -> bool
    {
        if (!segment.starts_with("S:") || segment == "S:*") return false;
        segment.remove_prefix(2U);
        const std::size_t separator = segment.find(':');
        if (separator == std::string_view::npos) return false;
        const std::string_view startText = segment.substr(0, separator);
        const std::string_view endText = segment.substr(separator + 1U);
        if (startText != "_")
        {
            start = parseNumber(startText);
            if (!start.has_value()) return false;
        }
        if (endText != "_")
        {
            end = parseNumber(endText);
            if (!end.has_value()) return false;
        }
        return true;
    };

    const std::string_view leftRoot = nextSegment(left);
    const std::string_view rightRoot = nextSegment(right);
    // Unknown reference-return/dereference provenance is one global conservative region.  It may
    // alias any named place, so it never establishes disjointness.
    if (leftRoot == "U" || rightRoot == "U") return true;
    if (leftRoot != rightRoot) return false;
    while (!left.empty() && !right.empty())
    {
        const std::string_view leftSegment = nextSegment(left);
        const std::string_view rightSegment = nextSegment(right);
        if (leftSegment == rightSegment) continue;
        if (leftSegment.ends_with(":*") || rightSegment.ends_with(":*")) return true;
        if (leftSegment.starts_with("F:") && rightSegment.starts_with("F:")) return false;
        if (leftSegment.starts_with("I:") && rightSegment.starts_with("I:")) return false;

        const auto indexAgainstSlice = [&](std::string_view indexSegment,
                                           std::string_view sliceSegment) noexcept
        {
            if (!indexSegment.starts_with("I:")) return true;
            const std::optional<std::int64_t> index = parseNumber(indexSegment.substr(2U));
            std::optional<std::int64_t> start;
            std::optional<std::int64_t> end;
            if (!index.has_value() || !parseSlice(sliceSegment, start, end)) return true;
            return (!start.has_value() || *index >= *start) &&
                (!end.has_value() || *index < *end);
        };
        if (leftSegment.starts_with("I:") && rightSegment.starts_with("S:"))
            return indexAgainstSlice(leftSegment, rightSegment);
        if (leftSegment.starts_with("S:") && rightSegment.starts_with("I:"))
            return indexAgainstSlice(rightSegment, leftSegment);
        if (leftSegment.starts_with("S:") && rightSegment.starts_with("S:"))
        {
            std::optional<std::int64_t> leftStart;
            std::optional<std::int64_t> leftEnd;
            std::optional<std::int64_t> rightStart;
            std::optional<std::int64_t> rightEnd;
            if (!parseSlice(leftSegment, leftStart, leftEnd) ||
                !parseSlice(rightSegment, rightStart, rightEnd)) return true;
            if (leftEnd.has_value() && rightStart.has_value() && *leftEnd <= *rightStart)
                return false;
            if (rightEnd.has_value() && leftStart.has_value() && *rightEnd <= *leftStart)
                return false;
            return true;
        }
        return true;
    }
    // Equal paths and ancestor/descendant paths overlap.
    return true;
}

std::optional<std::size_t> ProjectMethodBodyModel::referenceRootLocalOrdinal(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const noexcept
{
    const ast::AstNode* root = expression;
    while (root != nullptr && root->kind() != ast::AstNodeKind::IdentifierExpression)
        root = root->first();
    if (root == nullptr) return std::nullopt;
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, root->text());
    if (!name.has_value() || name->kind != ScopeNameKind::Local ||
        name->semanticOrdinal >= locals_.size() || !locals_[name->semanticOrdinal].isReference)
        return std::nullopt;
    return name->semanticOrdinal;
}

bool ProjectMethodBodyModel::referenceLocalDescendsFrom(
    std::size_t localOrdinal,
    std::size_t possibleAncestorOrdinal) const noexcept
{
    std::size_t cursor = localOrdinal;
    std::size_t remaining = locals_.size();
    while (cursor < locals_.size() && remaining-- != 0U)
    {
        const std::optional<std::size_t> parent =
            locals_[cursor].referenceReborrowSourceLocalOrdinal;
        if (!parent.has_value()) return false;
        if (*parent == possibleAncestorOrdinal) return true;
        cursor = *parent;
    }
    return false;
}

std::optional<std::size_t> ProjectMethodBodyModel::conflictingMutableReference(
    std::string_view place,
    std::size_t scopeOrdinal,
    std::optional<std::size_t> accessThroughReference,
    std::optional<std::size_t> reborrowSource) const noexcept
{
    if (place.empty()) return std::nullopt;
    for (std::size_t ordinal = 0; ordinal < locals_.size(); ++ordinal)
    {
        const MethodLocalSymbol& candidate = locals_[ordinal];
        if (!candidate.isReference || candidate.referencePlaceKey.empty() ||
            !scopeContains(candidate.scopeOrdinal, scopeOrdinal) ||
            !placesOverlap(place, candidate.referencePlaceKey)) continue;

        if (accessThroughReference.has_value())
        {
            if (ordinal == *accessThroughReference) continue;
            // Access through the most-derived live reborrow is valid.  Its ancestors are
            // suspended, not competing aliases.
            if (referenceLocalDescendsFrom(*accessThroughReference, ordinal)) continue;
        }
        if (reborrowSource.has_value())
        {
            if (ordinal == *reborrowSource ||
                referenceLocalDescendsFrom(*reborrowSource, ordinal)) continue;
        }
        return ordinal;
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::placeOrAncestorIsPartiallyMoved(std::string_view place) const
{
    return std::ranges::any_of(partiallyMovedPlaces_, [&](const std::string& moved)
    {
        return placeContains(moved, place);
    });
}

bool ProjectMethodBodyModel::placeOrAncestorIsMaybePartiallyMoved(std::string_view place) const
{
    return std::ranges::any_of(maybePartiallyMovedPlaces_, [&](const std::string& moved)
    {
        return placeContains(moved, place);
    });
}

bool ProjectMethodBodyModel::placeHasMovedDescendant(std::string_view place) const
{
    return std::ranges::any_of(partiallyMovedPlaces_, [&](const std::string& moved)
    {
        return moved != place && placeContains(place, moved);
    });
}

bool ProjectMethodBodyModel::placeHasMaybeMovedDescendant(std::string_view place) const
{
    return std::ranges::any_of(maybePartiallyMovedPlaces_, [&](const std::string& moved)
    {
        return moved != place && placeContains(place, moved);
    });
}

void ProjectMethodBodyModel::reinitializePartialPlace(std::string_view place)
{
    std::erase_if(partiallyMovedPlaces_, [&](const std::string& moved)
    {
        return placeContains(place, moved);
    });
    std::erase_if(maybePartiallyMovedPlaces_, [&](const std::string& moved)
    {
        return placeContains(place, moved);
    });
}

MethodBorrowState ProjectMethodBodyModel::localBorrowFlowState(std::size_t borrowOrdinal) const noexcept
{
    if (invalidLocalBorrows_.contains(borrowOrdinal))
    {
        return MethodBorrowState::Invalidated;
    }
    if (maybeInvalidLocalBorrows_.contains(borrowOrdinal))
    {
        return MethodBorrowState::MaybeInvalid;
    }
    return MethodBorrowState::Valid;
}

void ProjectMethodBodyModel::invalidateHistoricalBorrowsOfOwnerLocal(std::size_t localOrdinal)
{
    for (std::size_t ordinal = 0; ordinal < localBorrowBindings_.size(); ++ordinal)
    {
        const MethodLocalBorrowBinding& borrow = localBorrowBindings_[ordinal];
        if (borrow.sourceOwnerLocalOrdinal == localOrdinal ||
            std::find(
                borrow.possibleSourceOwnerLocalOrdinals.begin(),
                borrow.possibleSourceOwnerLocalOrdinals.end(),
                localOrdinal) != borrow.possibleSourceOwnerLocalOrdinals.end())
        {
            maybeInvalidLocalBorrows_.erase(ordinal);
            invalidLocalBorrows_.insert(ordinal);
        }
    }
}

void ProjectMethodBodyModel::invalidateHistoricalBorrowsOfOwnedParameter(std::size_t parameterOrdinal)
{
    for (std::size_t ordinal = 0; ordinal < localBorrowBindings_.size(); ++ordinal)
    {
        const MethodLocalBorrowBinding& borrow = localBorrowBindings_[ordinal];
        if (borrow.sourceOwnerParameterOrdinal == parameterOrdinal ||
            std::find(
                borrow.possibleSourceOwnerParameterOrdinals.begin(),
                borrow.possibleSourceOwnerParameterOrdinals.end(),
                parameterOrdinal) != borrow.possibleSourceOwnerParameterOrdinals.end())
        {
            maybeInvalidLocalBorrows_.erase(ordinal);
            invalidLocalBorrows_.insert(ordinal);
        }
    }
}

bool ProjectMethodBodyModel::hasActiveBorrowOfOwnerLocal(
    std::size_t localOrdinal,
    std::size_t scopeOrdinal,
    std::optional<std::string_view> accessedPlace) const noexcept
{
    for (std::size_t ordinal = 0; ordinal < localBorrowBindings_.size(); ++ordinal)
    {
        const MethodLocalBorrowBinding& borrow = localBorrowBindings_[ordinal];
        const bool rootedHere = borrow.sourceOwnerLocalOrdinal == localOrdinal ||
            std::find(
                borrow.possibleSourceOwnerLocalOrdinals.begin(),
                borrow.possibleSourceOwnerLocalOrdinals.end(),
                localOrdinal) != borrow.possibleSourceOwnerLocalOrdinals.end();
        if (rootedHere &&
            scopeContains(borrow.creationScopeOrdinal, scopeOrdinal) &&
            localBorrowFlowState(ordinal) != MethodBorrowState::Invalidated)
        {
            return true;
        }
    }
    const std::size_t methodMemberOrdinal = scopeOrdinal < scopes_.size()
        ? scopes_[scopeOrdinal].methodMemberOrdinal : std::numeric_limits<std::size_t>::max();
    for (const MethodBorrowedParameterReassignmentBinding& borrow :
         borrowedParameterReassignmentBindings_)
    {
        if (borrow.methodMemberOrdinal == methodMemberOrdinal &&
            std::find(
                borrow.possibleSourceOwnerLocalOrdinals.begin(),
                borrow.possibleSourceOwnerLocalOrdinals.end(),
                localOrdinal) != borrow.possibleSourceOwnerLocalOrdinals.end())
        {
            return true;
        }
    }
    // T31 view values (`borrowed struct`, including Span<T> and BorrowedReference<T>) carry
    // their owner provenance in MethodLocalSymbol::viewRootLocalName rather than in the older
    // explicit `borrowed T` binding table above. Both are real borrows and must block the same
    // move while the view's lexical scope is active.
    if (localOrdinal < locals_.size())
    {
        const std::string rootPlace = "L" + std::to_string(localOrdinal);
        const std::string_view operationPlace = accessedPlace.value_or(rootPlace);
        if (conflictingMutableReference(operationPlace, scopeOrdinal).has_value()) return true;
        const std::string& ownerName = locals_[localOrdinal].name;
        for (const MethodLocalSymbol& local : locals_)
        {
            if (local.viewRootLocalName == ownerName &&
                scopeContains(local.scopeOrdinal, scopeOrdinal))
            {
                return true;
            }
        }
    }
    return false;
}

bool ProjectMethodBodyModel::hasActiveBorrowOfOwnedParameter(
    std::size_t parameterOrdinal,
    std::size_t scopeOrdinal,
    std::optional<std::string_view> accessedPlace) const noexcept
{
    for (std::size_t ordinal = 0; ordinal < localBorrowBindings_.size(); ++ordinal)
    {
        const MethodLocalBorrowBinding& borrow = localBorrowBindings_[ordinal];
        const bool rootedHere = borrow.sourceOwnerParameterOrdinal == parameterOrdinal ||
            std::find(
                borrow.possibleSourceOwnerParameterOrdinals.begin(),
                borrow.possibleSourceOwnerParameterOrdinals.end(),
                parameterOrdinal) != borrow.possibleSourceOwnerParameterOrdinals.end();
        if (rootedHere &&
            scopeContains(borrow.creationScopeOrdinal, scopeOrdinal) &&
            localBorrowFlowState(ordinal) != MethodBorrowState::Invalidated)
        {
            return true;
        }
    }
    const std::size_t methodMemberOrdinal = scopeOrdinal < scopes_.size()
        ? scopes_[scopeOrdinal].methodMemberOrdinal : std::numeric_limits<std::size_t>::max();
    for (const MethodBorrowedParameterReassignmentBinding& borrow :
         borrowedParameterReassignmentBindings_)
    {
        if (borrow.methodMemberOrdinal == methodMemberOrdinal &&
            std::find(
                borrow.possibleSourceOwnerParameterOrdinals.begin(),
                borrow.possibleSourceOwnerParameterOrdinals.end(),
                parameterOrdinal) != borrow.possibleSourceOwnerParameterOrdinals.end())
        {
            return true;
        }
    }
    const std::string rootPlace = "P" + std::to_string(parameterOrdinal);
    if (conflictingMutableReference(
            accessedPlace.value_or(std::string_view(rootPlace)), scopeOrdinal).has_value())
        return true;
    return false;
}
}
