#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

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
using lexer::TokenKind;
using method_body_detail::pointerContract;

bool ProjectMethodBodyModel::aggregateIsUnion(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != identity || type.syntax == nullptr) continue;
        return type.syntax->auxiliaryText().find("union") != std::string::npos;
    }
    return false;
}

bool ProjectMethodBodyModel::aggregateRequiresActiveMember(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != identity || type.syntax == nullptr) continue;
        return type.syntax->auxiliaryText().find("active-member-safe") != std::string::npos;
    }
    return false;
}

bool ProjectMethodBodyModel::aggregateAllowsSynthesizedMember(
    std::string_view typeKey,
    std::string_view memberName) const noexcept
{
    // Ordinary 2.x structs and structural tuples retain their existing root members. Only a
    // NativeSharp 3.0 record changes to explicit opt-ins. The declaration marker survives generic
    // specialization but never enters the value layout.
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:"))
    {
        return memberName != "Clone";
    }
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != identity || type.syntax == nullptr) continue;
        const std::string* metadata = &type.syntax->auxiliaryText();
        // Stage 44 replaces a closed generic declaration's auxiliary text with its canonical
        // specialization key. The specialization catalog retains the originating template AST;
        // derive policy belongs to that source declaration and must be read from there.
        constexpr std::string_view metadataDelimiter = ";nsc-meta:";
        const auto sourceMetadata = [&](const std::string& value) -> std::string_view
        {
            const std::size_t delimiter = value.find(metadataDelimiter);
            return delimiter == std::string::npos
                ? std::string_view(value)
                : std::string_view(value).substr(delimiter + metadataDelimiter.size());
        };
        const auto hasMarker = [](std::string_view list, std::string_view marker) noexcept
        {
            std::size_t cursor = 0;
            while (cursor <= list.size())
            {
                const std::size_t separator = list.find(',', cursor);
                const std::string_view item = list.substr(
                    cursor, separator == std::string_view::npos ? std::string_view::npos
                                                                : separator - cursor);
                if (item == marker) return true;
                if (separator == std::string_view::npos) break;
                cursor = separator + 1;
            }
            return false;
        };
        if (!hasMarker(sourceMetadata(*metadata), "record") && genericModel_ != nullptr)
        {
            if (const GenericSpecializationRecord* specialization =
                    genericModel_->specializationForTypeKey(typeKey);
                specialization != nullptr && specialization->templateSyntax != nullptr)
            {
                metadata = &specialization->templateSyntax->auxiliaryText();
            }
        }
        const std::string_view recordMetadata = sourceMetadata(*metadata);
        if (!hasMarker(recordMetadata, "record"))
        {
            return memberName != "Clone";
        }
        const std::string_view derive = memberName == "Equals" ? "Equality"
            : memberName == "GetHashCode" ? "Hash"
            : memberName == "ToString" ? "Print"
            : memberName == "Clone" ? "Clone" : std::string_view{};
        if (derive.empty()) return false;

        // The parser emits one canonical ordered marker: derive=Equality|Hash|Print|Clone.
        // Compare complete list elements so `Hash` can never match a future longer derive name.
        const std::size_t marker = recordMetadata.find("derive=");
        if (marker == std::string::npos) return false;
        const std::size_t begin = marker + std::string_view("derive=").size();
        const std::size_t end = recordMetadata.find(',', begin);
        const std::string_view list = recordMetadata.substr(
            begin, (end == std::string::npos ? recordMetadata.size() : end) - begin);
        std::size_t cursor = 0;
        while (cursor <= list.size())
        {
            const std::size_t separator = list.find('|', cursor);
            const std::string_view item = list.substr(
                cursor, separator == std::string_view::npos ? std::string_view::npos
                                                            : separator - cursor);
            if (item == derive) return true;
            if (separator == std::string_view::npos) break;
            cursor = separator + 1;
        }
        return false;
    }
    return memberName != "Clone";
}

bool ProjectMethodBodyModel::aggregateSupportsSynthesizedMember(
    std::string_view typeKey,
    std::string_view memberName,
    std::string& reason) const
{
    reason.clear();
    if (memberName == "ToString") return true;
    if (memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr)
    {
        reason = "record derive validation requires the complete member and type models";
        return false;
    }

    const std::string_view requiredDerive = memberName == "Equals" ? "Equality"
        : memberName == "GetHashCode" ? "Hash"
        : memberName == "Clone" ? "Clone" : std::string_view{};
    if (requiredDerive.empty())
    {
        reason = "unknown synthesized record member '" + std::string(memberName) + "'";
        return false;
    }

    std::unordered_set<std::string> activeTypes;
    std::function<bool(std::string_view, const std::string&, bool)> visit =
        [&](std::string_view currentTypeKey, const std::string& path, bool nested) -> bool
    {
        if (const std::optional<std::string> element =
                ProjectMemberModel::fixedArrayElementTypeKey(currentTypeKey);
            element.has_value())
        {
            return visit(*element, path + "[]", true);
        }
        if (const std::optional<std::vector<std::string>> elements =
                ProjectTypeConversion::tupleElementTypeKeys(currentTypeKey);
            elements.has_value())
        {
            for (std::size_t index = 0; index < elements->size(); ++index)
            {
                if (!visit((*elements)[index], path + "." + std::to_string(index), true))
                    return false;
            }
            return true;
        }
        if (!typeKeyIsStruct(currentTypeKey)) return true;

        if (aggregateIsUnion(currentTypeKey))
        {
            reason = "field '" + (path.empty() ? std::string("<self>") : path) +
                "' has union type '" + std::string(currentTypeKey) +
                "'; a generated '" + std::string(memberName) +
                "' cannot determine which union member is live";
            return false;
        }
        if (nested && memberName != "Clone" &&
            !aggregateAllowsSynthesizedMember(currentTypeKey, memberName))
        {
            reason = "field '" + path + "' has record type '" + std::string(currentTypeKey) +
                "' that does not derive " + std::string(requiredDerive) +
                "; the outer record cannot synthesize '" + std::string(memberName) + "'";
            return false;
        }

        const std::string stableKey(currentTypeKey);
        if (!activeTypes.insert(stableKey).second) return true;
        const std::optional<std::size_t> typeOrdinal =
            inheritanceModel_->typeOrdinalForTypeKey(currentTypeKey);
        if (!typeOrdinal.has_value())
        {
            activeTypes.erase(stableKey);
            reason = "record derive validation cannot resolve value type '" + stableKey + "'";
            return false;
        }

        // Destructors do not participate in ordinary name lookup and therefore are not required
        // to appear in the per-type lookup bucket. Walk the authoritative symbol vector and
        // filter by owner so Clone cannot miss lifetime-only members.
        for (const ClassMemberSymbol& member : memberModel_->members())
        {
            if (member.declaringTypeOrdinal != *typeOrdinal) continue;
            if (member.kind == ClassMemberKind::Destructor && memberName == "Clone")
            {
                reason = "field '" + (path.empty() ? std::string("<self>") : path) +
                    "' has a destructor; generated Clone is a plain value copy and would "
                    "duplicate a non-trivial lifetime";
                activeTypes.erase(stableKey);
                return false;
            }
            if (member.kind != ClassMemberKind::Field || member.isStatic) continue;
            const std::string fieldPath = path.empty() ? member.name : path + "." + member.name;
            if (memberName == "Clone" &&
                member.declaredOwnership == sema::OwnershipKind::Owned)
            {
                reason = "field '" + fieldPath +
                    "' owns storage; generated Clone is a plain value copy and would create "
                    "two owners";
                activeTypes.erase(stableKey);
                return false;
            }
            if (!visit(member.declaredTypeKey, fieldPath, true))
            {
                activeTypes.erase(stableKey);
                return false;
            }
        }
        activeTypes.erase(stableKey);
        return true;
    };

    return visit(typeKey, {}, false);
}

// SHR-013: la marca `borrowedview` que el parser escribio en el texto auxiliar de la
// declaracion, leida igual que "union" justo arriba. Un solo sitio la escribe y un solo sitio la
// lee, que es la razon de haberla puesto ahi y no en un AstNodeKind nuevo.
bool ProjectMethodBodyModel::aggregateIsBorrowedView(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != identity || type.syntax == nullptr) continue;
        return type.syntax->auxiliaryText().find("borrowedview") != std::string::npos;
    }
    return false;
}

std::optional<std::vector<std::string>> ProjectMethodBodyModel::aggregateElementTypeKeys(
    std::string_view typeKey) const
{
    if (const std::optional<std::vector<std::string>> tuple =
            ProjectTypeConversion::tupleElementTypeKeys(typeKey);
        tuple.has_value()) return tuple;
    if (const std::optional<std::string> element =
            ProjectMemberModel::fixedArrayElementTypeKey(typeKey);
        element.has_value())
    {
        const std::optional<std::uint64_t> count = ProjectMemberModel::fixedArrayCount(typeKey);
        if (!count.has_value()) return std::nullopt;
        return std::vector<std::string>(static_cast<std::size_t>(*count), *element);
    }
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr)
    {
        return std::nullopt;
    }
    if (!typeKeyHasDefinedDefaultStorage(typeKey)) return std::nullopt;
    const std::optional<std::size_t> typeOrdinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
    if (!typeOrdinal.has_value()) return std::nullopt;
    bool isUnion = false;
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != typeKey.substr(5) || type.syntax == nullptr) continue;
        // Same marker ProjectObjectLayoutModel reads (see parseStructLayoutOptions there); the
        // markers are comma-joined, so a substring test is what identifies the union flag.
        isUnion = type.syntax->auxiliaryText().find("union") != std::string::npos;
        break;
    }
    std::vector<std::string> elements;
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(*typeOrdinal))
    {
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Field || member.isStatic || member.isConst ||
            member.isUnnamedBitField) continue;
        elements.push_back(member.declaredTypeKey);
        if (isUnion) break;
    }
    return elements;
}

std::optional<std::vector<std::string>> ProjectMethodBodyModel::aggregateElementNames(
    std::string_view typeKey) const
{
    if (const std::optional<std::vector<std::string>> tuple =
            ProjectTypeConversion::tupleElementTypeKeys(typeKey);
        tuple.has_value())
    {
        std::vector<std::string> names;
        names.reserve(tuple->size());
        for (std::size_t index = 0; index < tuple->size(); ++index)
            names.push_back("Item" + std::to_string(index + 1));
        return names;
    }
    if (const std::optional<std::string> element =
            ProjectMemberModel::fixedArrayElementTypeKey(typeKey);
        element.has_value())
    {
        const std::optional<std::uint64_t> count = ProjectMemberModel::fixedArrayCount(typeKey);
        if (!count.has_value()) return std::nullopt;
        return std::vector<std::string>(static_cast<std::size_t>(*count));
    }
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        !typeKey.starts_with("type:"))
        return std::nullopt;
    const std::optional<std::size_t> typeOrdinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
    if (!typeOrdinal.has_value()) return std::nullopt;
    bool isUnion = false;
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != typeKey.substr(5) || type.syntax == nullptr) continue;
        isUnion = type.syntax->auxiliaryText().find("union") != std::string::npos;
        break;
    }
    std::vector<std::string> names;
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(*typeOrdinal))
    {
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Field || member.isStatic || member.isConst ||
            member.isUnnamedBitField) continue;
        names.push_back(member.name);
        if (isUnion) break;
    }
    return names;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::bindAggregateInitializer(
    const ast::AstNode* syntax,
    const std::string& targetTypeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMoveSpread)
{
    ExpressionResult result;
    const std::optional<std::vector<std::string>> elements = aggregateElementTypeKeys(targetTypeKey);
    const std::optional<std::vector<std::string>> names = aggregateElementNames(targetTypeKey);
    if (!elements.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "an aggregate initializer requires a struct or fixed-array target, not '" +
                targetTypeKey + "'");
        return result;
    }
    if (!names.has_value() || names->size() != elements->size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "aggregate initializer could not enumerate the elements of '" + targetTypeKey + "'");
        return result;
    }

    // T02-F06. A spread copies the value as a whole and therefore does not expose private
    // representation, but every explicitly written entry is a direct field initialization and
    // must obey the same accessibility rule as `receiver.Field = value`. Keep the member
    // ordinals parallel to aggregateElementNames/aggregateElementTypeKeys so both named and
    // positional entries are checked; tuples and fixed arrays deliberately have no member gate.
    std::vector<std::optional<std::size_t>> elementMemberOrdinals(elements->size());
    std::optional<std::size_t> targetTypeOrdinal;
    if (inheritanceModel_ != nullptr && targetTypeKey.starts_with("type:"))
        targetTypeOrdinal = inheritanceModel_->typeOrdinalForTypeKey(targetTypeKey);
    if (targetTypeOrdinal.has_value() && memberModel_ != nullptr)
    {
        std::size_t elementOrdinal = 0;
        for (const std::size_t memberOrdinal : memberModel_->membersOfType(*targetTypeOrdinal))
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
            if (member.kind != ClassMemberKind::Field || member.isStatic || member.isConst ||
                member.isUnnamedBitField) continue;
            if (elementOrdinal < elementMemberOrdinals.size())
                elementMemberOrdinals[elementOrdinal] = memberOrdinal;
            ++elementOrdinal;
            if (elementOrdinal >= elementMemberOrdinals.size()) break;
        }
    }

    MethodAggregateInitializerBinding binding;
    binding.syntax = syntax;
    binding.targetTypeKey = targetTypeKey;
    binding.generalized = std::ranges::any_of(syntax->children(), [](const ast::AstNode* element)
    {
        return element != nullptr &&
            (!element->argumentName().empty() ||
             element->kind() == ast::AstNodeKind::SpreadElementExpression);
    });

    bool valid = true;
    const bool nontrivialTarget = !elementTypeHasTrivialLifetime(targetTypeKey);
    bool hasMoveSpread = false;
    std::vector<bool> reservedByName(elements->size(), false);
    std::vector<std::optional<std::size_t>> namedTargets(syntax->children().size());
    for (std::size_t index = 0; index < syntax->children().size(); ++index)
    {
        const ast::AstNode* element = syntax->children()[index];
        if (element == nullptr || element->argumentName().empty()) continue;
        const auto found = std::ranges::find(*names, element->argumentName());
        if (found == names->end())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::UnknownMember,
                diagnostics::DiagnosticSeverity::Error,
                element->range(),
                "aggregate '" + targetTypeKey + "' has no initializable member named '" +
                    element->argumentName() + "'");
            valid = false;
            continue;
        }
        const std::size_t target = static_cast<std::size_t>(std::distance(names->begin(), found));
        if (reservedByName[target])
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                element->range(),
                "initializer member '" + element->argumentName() + "' is assigned more than once");
            valid = false;
            continue;
        }
        reservedByName[target] = true;
        namedTargets[index] = target;
    }

    std::vector<bool> assigned(elements->size(), false);
    std::size_t nextPositional = 0;
    std::optional<std::size_t> spreadEntry;
    for (std::size_t index = 0; index < syntax->children().size(); ++index)
    {
        const ast::AstNode* element = syntax->children()[index];
        if (element == nullptr)
        {
            valid = false;
            continue;
        }
        MethodAggregateInitializerEntryBinding entry;
        entry.syntax = element;
        if (element->kind() == ast::AstNodeKind::SpreadElementExpression)
        {
            entry.spread = true;
            if (spreadEntry.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    element->range(),
                    "an aggregate initializer may contain only one spread entry");
                valid = false;
            }
            const ast::AstNode* source = element->first();
            const ast::AstNode* sourceValue = source;
            if (nontrivialTarget)
            {
                if (!allowMoveSpread || source == nullptr ||
                    source->kind() != ast::AstNodeKind::MoveExpression ||
                    source->first() == nullptr)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        element->range(),
                        "move-only aggregate spread for '" + targetTypeKey +
                            "' requires '..move(source)' in a local initializer");
                    valid = false;
                }
                else
                {
                    sourceValue = source->first();
                    entry.moveSpread = true;
                    hasMoveSpread = true;
                }
            }

            // A move-spread is a set of field projections, not a whole-value read. Reuse the
            // partial-projection guard so a second spread may still move untouched siblings while
            // the ordinary use of an already partial value remains rejected.
            if (entry.moveSpread) ++partialProjectionReceiverDepth_;
            const ExpressionResult sourceResult = analyzeExpression(
                sourceValue, methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (entry.moveSpread) --partialProjectionReceiverDepth_;
            if (!sourceResult.valid || sourceResult.typeKey != targetTypeKey)
            {
                if (sourceResult.valid)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidConversion,
                        diagnostics::DiagnosticSeverity::Error,
                        source != nullptr ? source->range() : element->range(),
                        "spread source for '" + targetTypeKey + "' must have the same type, not '" +
                            sourceResult.typeKey + "'");
                }
                valid = false;
            }
            if (entry.moveSpread)
            {
                const bool namedSource = sourceValue != nullptr &&
                    sourceValue->kind() == ast::AstNodeKind::IdentifierExpression;
                const std::optional<ScopeName> sourceName = namedSource
                    ? lookupScopeName(scopeOrdinal, sourceValue->text())
                    : std::optional<ScopeName>{};
                bool movableRoot = false;
                if (sourceName.has_value() && sourceName->kind == ScopeNameKind::Local &&
                    sourceName->semanticOrdinal < locals_.size())
                {
                    const MethodLocalSymbol& local = locals_[sourceName->semanticOrdinal];
                    movableRoot = local.isMoveOnly && !local.isPinned;
                    if (local.isPinned)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            sourceValue->range(),
                            "cannot move-spread pinned local '" + local.name + "'");
                    }
                    else if (movableRoot && hasActiveBorrowOfOwnerLocal(
                                 sourceName->semanticOrdinal, scopeOrdinal, std::nullopt))
                    {
                        movableRoot = false;
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            sourceValue->range(),
                            "cannot move-spread local '" + local.name +
                                "' while a borrowed local is alive");
                    }
                    if (movableRoot) entry.moveSourceLocalOrdinal = sourceName->semanticOrdinal;
                }
                else if (sourceName.has_value())
                {
                    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
                    if (method != nullptr && sourceName->semanticOrdinal < method->parameters.size())
                    {
                        const MethodParameterSymbol& parameter =
                            method->parameters[sourceName->semanticOrdinal];
                        movableRoot = parameter.passKind == ParameterPassKind::Value &&
                            memberModel_ != nullptr && memberModel_->typeIsMoveOnly(parameter.typeKey);
                        if (movableRoot && hasActiveBorrowOfOwnedParameter(
                                             sourceName->semanticOrdinal, scopeOrdinal, std::nullopt))
                        {
                            movableRoot = false;
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                sourceValue->range(),
                                "cannot move-spread parameter '" + parameter.name +
                                    "' while a borrowed local is alive");
                        }
                        if (movableRoot)
                            entry.moveSourceParameterOrdinal = sourceName->semanticOrdinal;
                    }
                }
                if (!movableRoot)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                        diagnostics::DiagnosticSeverity::Error,
                        sourceValue != nullptr ? sourceValue->range() : element->range(),
                        "move-spread source must be a named move-only local or by-value parameter");
                    valid = false;
                }
                if (memberModel_ != nullptr &&
                    memberModel_->typeTransitivelyHasCustomDestructor(targetTypeKey))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        element->range(),
                        "cannot move-spread value of type '" + targetTypeKey +
                            "' because its custom destructor requires the complete value");
                    valid = false;
                }
            }
            if (!spreadEntry.has_value()) spreadEntry = binding.entries.size();
            binding.entries.push_back(std::move(entry));
            continue;
        }

        std::optional<std::size_t> target = namedTargets[index];
        if (!target.has_value())
        {
            while (nextPositional < elements->size() &&
                   (assigned[nextPositional] || reservedByName[nextPositional]))
                ++nextPositional;
            if (nextPositional < elements->size()) target = nextPositional++;
        }
        if (!target.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                element->range(),
                binding.generalized
                    ? "aggregate initializer for '" + targetTypeKey +
                        "' has more values than elements"
                    : "aggregate initializer for '" + targetTypeKey + "' has " +
                        std::to_string(syntax->children().size()) +
                        " values but the type takes " + std::to_string(elements->size()));
            valid = false;
            binding.entries.push_back(std::move(entry));
            continue;
        }
        if (assigned[*target])
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                element->range(),
                "aggregate element " + std::to_string(*target) + " is assigned more than once");
            valid = false;
            binding.entries.push_back(std::move(entry));
            continue;
        }
        if (*target < elementMemberOrdinals.size() &&
            elementMemberOrdinals[*target].has_value() &&
            memberModel_ != nullptr &&
            methodMemberOrdinal < memberModel_->members().size() &&
            targetTypeOrdinal.has_value() && nameLookup_ != nullptr &&
            inheritanceModel_ != nullptr)
        {
            const ClassMemberSymbol& field =
                memberModel_->members()[*elementMemberOrdinals[*target]];
            const ClassMemberSymbol& caller = memberModel_->members()[methodMemberOrdinal];
            if (!ProjectMemberAccessibility::isAccessible(
                    field,
                    caller.declaringTypeOrdinal,
                    *targetTypeOrdinal,
                    *nameLookup_,
                    *inheritanceModel_))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InaccessibleMember,
                    diagnostics::DiagnosticSeverity::Error,
                    element->range(),
                    "aggregate member '" + field.name +
                        "' is not accessible in this initializer context");
                valid = false;
            }
        }
        assigned[*target] = true;
        entry.targetElementOrdinals.push_back(*target);
        const std::string& elementTypeKey = (*elements)[*target];
        if (element->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            const ExpressionResult nested = bindAggregateInitializer(
                element, elementTypeKey, methodMemberOrdinal, scopeOrdinal, diagnostics, false);
            valid = valid && nested.valid;
            binding.entries.push_back(std::move(entry));
            continue;
        }
        const bool ownedTarget = *target < elementMemberOrdinals.size() &&
            elementMemberOrdinals[*target].has_value() && memberModel_ != nullptr &&
            *elementMemberOrdinals[*target] < memberModel_->members().size() &&
            memberModel_->members()[*elementMemberOrdinals[*target]].declaredOwnership ==
                sema::OwnershipKind::Owned;
        const ExpressionResult value = analyzeExpression(
            element, methodMemberOrdinal, scopeOrdinal, diagnostics, false, false, ownedTarget);
        if (!value.valid)
        {
            valid = false;
            continue;
        }
        const ProjectImplicitConversion conversion = classifyImplicit(
            value.typeKey, elementTypeKey, value.integerConstant);
        if (!conversion.exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                element->range(),
                "cannot initialize element " + std::to_string(*target) + " of '" + targetTypeKey +
                    "' of type '" + elementTypeKey + "' from '" + value.typeKey + "'");
            valid = false;
        }
        if (ownedTarget && value.valid)
        {
            const bool freshObject = element->kind() == ast::AstNodeKind::ObjectCreationExpression;
            const bool nullValue = element->kind() == ast::AstNodeKind::NullLiteralExpression;
            const bool transferred = value.ownership == sema::OwnershipKind::Owned &&
                value.ownershipTransfer;
            if (!freshObject && !nullValue && !transferred)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    element->range(),
                    "initializer for owned aggregate field '" + (*names)[*target] +
                        "' requires move(owner), new T(...), null, or an owned-returning call");
                valid = false;
            }
        }
        binding.entries.push_back(std::move(entry));
    }

    std::vector<std::size_t> missing;
    for (std::size_t index = 0; index < assigned.size(); ++index)
        if (!assigned[index]) missing.push_back(index);
    if (spreadEntry.has_value() && *spreadEntry < binding.entries.size())
    {
        binding.entries[*spreadEntry].targetElementOrdinals = missing;
        MethodAggregateInitializerEntryBinding& spread = binding.entries[*spreadEntry];
        if (spread.moveSpread)
        {
            if (missing.empty())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    spread.syntax != nullptr ? spread.syntax->range() : syntax->range(),
                    "move-spread must transfer at least one aggregate field");
                valid = false;
            }
            const std::string root = spread.moveSourceLocalOrdinal.has_value()
                ? "L" + std::to_string(*spread.moveSourceLocalOrdinal)
                : spread.moveSourceParameterOrdinal.has_value()
                    ? "P" + std::to_string(*spread.moveSourceParameterOrdinal)
                    : std::string{};
            if (!root.empty())
            {
                const bool namedFields = std::ranges::all_of(missing, [&](std::size_t index)
                {
                    return index < names->size() && !(*names)[index].empty();
                });
                if (!namedFields)
                {
                    if (placeOrAncestorIsPartiallyMoved(root) || placeHasMovedDescendant(root) ||
                        placeOrAncestorIsMaybePartiallyMoved(root) ||
                        placeHasMaybeMovedDescendant(root))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                            diagnostics::DiagnosticSeverity::Error,
                            spread.syntax != nullptr ? spread.syntax->range() : syntax->range(),
                            "cannot move-spread an aggregate source that is already partially moved");
                        valid = false;
                    }
                    if (valid)
                    {
                        partiallyMovedPlaces_.insert(root);
                        maybePartiallyMovedPlaces_.erase(root);
                    }
                }
                else
                {
                    std::vector<std::string> movedFields;
                    movedFields.reserve(missing.size());
                    for (const std::size_t index : missing)
                    {
                        const std::string place = root + "/" + (*names)[index];
                        movedFields.push_back(place);
                        if (placeOrAncestorIsPartiallyMoved(place) || placeHasMovedDescendant(place) ||
                            placeOrAncestorIsMaybePartiallyMoved(place) ||
                            placeHasMaybeMovedDescendant(place))
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                                diagnostics::DiagnosticSeverity::Error,
                                spread.syntax != nullptr ? spread.syntax->range() : syntax->range(),
                                "cannot move-spread already moved field path '" + place + "'");
                            valid = false;
                        }
                    }
                    if (valid)
                    {
                        for (const std::string& place : movedFields)
                        {
                            partiallyMovedPlaces_.insert(place);
                            maybePartiallyMovedPlaces_.erase(place);
                        }
                    }
                }
            }
        }
        for (const std::size_t index : missing) assigned[index] = true;
        missing.clear();
    }
    if (binding.generalized && nontrivialTarget && !hasMoveSpread)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "generalized initializer for move-only aggregate '" + targetTypeKey +
                "' requires exactly one '..move(source)' spread");
        valid = false;
    }
    if (binding.generalized && !missing.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "generalized initializer for '" + targetTypeKey + "' leaves " +
                std::to_string(missing.size()) +
                " member(s) uninitialized; name every member or provide one spread source");
        valid = false;
    }

    aggregateInitializerBindingBySyntax_[syntax] = aggregateInitializerBindings_.size();
    aggregateInitializerBindings_.push_back(std::move(binding));
    result.typeKey = targetTypeKey;
    result.valid = valid;
    recordExpressionType(syntax, methodMemberOrdinal, scopeOrdinal, result);
    return result;
}

void ProjectMethodBodyModel::recordConstructionSite(ProjectConstructionSite site)
{
    const std::optional<std::size_t> previous = constructionSiteIndex_.latest(
        site.syntax,
        site.callableOrdinal);
    if (previous.has_value() && *previous < constructionSites_.size())
    {
        site.sequenceWithinCallable = constructionSites_[*previous].sequenceWithinCallable;
        constructionSites_[*previous] = std::move(site);
        return;
    }

    site.sequenceWithinCallable = nextConstructionSequenceByCallable_[site.callableOrdinal]++;
    const std::size_t siteOrdinal = constructionSites_.size();
    const ast::AstNode* syntax = site.syntax;
    const std::size_t callableOrdinal = site.callableOrdinal;
    constructionSites_.push_back(std::move(site));
    constructionSiteIndex_.record(callableOrdinal, syntax, siteOrdinal);
}
}
