#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"
#include "CppVolatileAliasContract.h"
#include "Internal/NamedArgumentMap.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/CppOverloadRanking.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Frontend/ReadonlyReferenceReturn.h"
#include "nativesharp/Interop/CppInteropSession.h"
#include "nativesharp/Interop/CppRecordDeclaration.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
using lexer::TokenKind;
using method_body_detail::NamedArgumentMap;
using method_body_detail::pointerContract;

namespace
{
// EN: T17-001. A note with the place where the header declares a C/C++ function, when Clang gave it.
// ES: T17-001. Una nota con el lugar donde el encabezado declara una funcion C/C++, si Clang lo dio.
std::vector<std::string> cppDeclarationNotes(const interop::CppFunctionDeclaration& declaration)
{
    if (!declaration.location) return {};
    return {"'" + declaration.qualifiedName + "' is declared at " +
            interop::describeCppSourcePoint(*declaration.location)};
}

// The NativeSharp type key as the program writes it (`builtin:int` is `int`).
// La clave de tipo NativeSharp como la escribe el programa (`builtin:int` es `int`).
std::string readableTypeKey(std::string_view key)
{
    if (key == "string-literal") return "string literal";
    for (const std::string_view prefix : {std::string_view("builtin:"), std::string_view("type:")})
        if (key.starts_with(prefix)) return std::string(key.substr(prefix.size()));
    return std::string(key);
}

bool cppArithmeticParameter(const interop::CppType& parameter)
{
    return parameter.kind == interop::CppTypeKind::SignedInteger ||
        parameter.kind == interop::CppTypeKind::UnsignedInteger ||
        parameter.kind == interop::CppTypeKind::Boolean ||
        parameter.kind == interop::CppTypeKind::Floating;
}

// EN: T19-006. The outcome of ranking C++ overloads by their arithmetic conversions.
//     `applies` is false when some viable candidate is not a C++ function or an argument is not arithmetic in
//     a position where the candidates' parameter types differ; the NativeSharp ranking then decides.
//     Otherwise `best` holds the candidates that no other candidate beats, as indices into the evaluated ones.
// ES: T19-006. El resultado de ordenar sobrecargas C++ por sus conversiones aritmeticas.
//     `applies` es falso si algun candidato viable no es una funcion C++ o un argumento no es aritmetico en una
//     posicion donde los tipos de parametro difieren; entonces decide el orden NativeSharp. Si no, `best` tiene
//     los candidatos que ningun otro supera, como indices de los evaluados.
struct CppArithmeticOverloadRanking final
{
    bool applies = false;
    std::vector<std::size_t> best;
};

CppArithmeticOverloadRanking rankCppArithmeticOverloads(
    const ProjectNamespaceFunctionModel* model,
    std::span<const MethodCallCandidate> evaluated,
    std::span<const std::size_t> viable)
{
    CppArithmeticOverloadRanking ranking;
    if (model == nullptr || viable.size() < 2) return ranking;
    // The C++ parameter type of each candidate for each explicit argument.
    std::vector<std::vector<const interop::CppType*>> parameterTypes;
    std::size_t argumentCount = 0;
    for (const std::size_t index : viable)
    {
        const MethodCallCandidate& candidate = evaluated[index];
        if (candidate.memberOrdinal >= model->functions().size()) return ranking;
        const NamespaceFunctionSymbol& function = model->functions()[candidate.memberOrdinal];
        if (function.cppCanonicalIdentity.empty() ||
            function.cppParameterTypes.size() != function.parameters.size() ||
            candidate.isVariadicCandidate || candidate.usesExpandedParameterPack ||
            candidate.conversions.empty() ||
            candidate.conversions.size() != candidate.argumentParameterOrdinals.size())
            return ranking;
        if (parameterTypes.empty()) argumentCount = candidate.conversions.size();
        if (candidate.conversions.size() != argumentCount) return ranking;
        std::vector<const interop::CppType*> row;
        for (const std::size_t parameterOrdinal : candidate.argumentParameterOrdinals)
        {
            if (parameterOrdinal >= function.cppParameterTypes.size()) return ranking;
            row.push_back(&function.cppParameterTypes[parameterOrdinal]);
        }
        parameterTypes.push_back(std::move(row));
    }
    std::vector<std::vector<int>> ranks(viable.size(), std::vector<int>(argumentCount, 0));
    bool arithmeticPositionDiffers = false;
    for (std::size_t argument = 0; argument < argumentCount; ++argument)
    {
        bool same = true;
        for (std::size_t row = 1; row < parameterTypes.size(); ++row)
        {
            if (parameterTypes[row][argument]->unqualifiedCanonicalSpelling !=
                parameterTypes[0][argument]->unqualifiedCanonicalSpelling)
                same = false;
        }
        // Equal parameter types give every candidate the same conversion. / Tipos iguales dan la misma conversion.
        if (same) continue;
        const std::optional<std::string_view> argumentType = cppArithmeticArgumentType(
            evaluated[viable[0]].conversions[argument].sourceTypeKey);
        if (!argumentType.has_value()) return ranking;
        for (std::size_t row = 0; row < parameterTypes.size(); ++row)
        {
            const interop::CppType& parameter = *parameterTypes[row][argument];
            if (!cppArithmeticParameter(parameter)) return ranking;
            ranks[row][argument] = cppArithmeticConversionRank(*argumentType, parameter.unqualifiedCanonicalSpelling);
        }
        arithmeticPositionDiffers = true;
    }
    if (!arithmeticPositionDiffers) return ranking;
    ranking.applies = true;
    for (std::size_t row = 0; row < viable.size(); ++row)
    {
        bool beaten = false;
        for (std::size_t other = 0; other < viable.size() && !beaten; ++other)
        {
            if (other == row) continue;
            bool noWorse = true;
            bool better = false;
            for (std::size_t argument = 0; argument < argumentCount; ++argument)
            {
                if (ranks[other][argument] > ranks[row][argument]) noWorse = false;
                if (ranks[other][argument] < ranks[row][argument]) better = true;
            }
            beaten = noWorse && better;
        }
        if (!beaten) ranking.best.push_back(viable[row]);
    }
    return ranking;
}

// EN: T17-003. One note per overload candidate of a namespace function: its declaration (the C++ spelling and
//     the header location for a C/C++ function) and why it does not take the call: its arity, or the first
//     argument whose type does not convert. `evaluated` holds the candidates the resolution examined.
// ES: T17-003. Una nota por candidato de una funcion de namespace: su declaracion (el texto C++ y la ubicacion
//     en el encabezado para una funcion C/C++) y por que no acepta la llamada: su aridad, o el primer argumento
//     cuyo tipo no se convierte. `evaluated` tiene los candidatos que examino la resolucion.
std::vector<std::string> namespaceFunctionCandidateNotes(
    const ProjectNamespaceFunctionModel* model,
    std::span<const std::size_t> ordinals,
    std::span<const MethodCallCandidate> evaluated,
    bool explainRejection)
{
    std::vector<std::string> notes;
    if (model == nullptr) return notes;
    constexpr std::size_t limit = 8;
    for (const std::size_t ordinal : ordinals)
    {
        if (ordinal >= model->functions().size()) continue;
        if (notes.size() == limit)
        {
            notes.push_back("and " + std::to_string(ordinals.size() - limit) + " more candidate(s)");
            break;
        }
        const NamespaceFunctionSymbol& function = model->functions()[ordinal];
        std::string note = "candidate: ";
        if (!function.cppDeclarationSpelling.empty())
            note += function.cppDeclarationSpelling;
        else
        {
            note += function.namespaceName.empty() ? function.name : function.namespaceName + "." + function.name;
            note += "(";
            for (std::size_t index = 0; index < function.parameters.size(); ++index)
                note += (index ? ", " : "") + readableTypeKey(function.parameters[index].typeKey);
            note += function.isVariadic ? ", ...)" : ")";
        }
        if (explainRejection)
        {
            const MethodCallCandidate* candidate = nullptr;
            for (const MethodCallCandidate& item : evaluated)
                if (item.memberOrdinal == ordinal) { candidate = &item; break; }
            if (candidate != nullptr && !candidate->viable && candidate->conversions.empty())
                note += " takes " + std::string(function.isVariadic ? "at least " : "") +
                        std::to_string(function.parameters.size()) + " argument(s)";
            else if (candidate != nullptr)
            {
                const auto failed = std::find_if(candidate->conversions.begin(), candidate->conversions.end(),
                    [](const ProjectImplicitConversion& conversion) { return !conversion.exists(); });
                if (failed != candidate->conversions.end())
                    note += ": argument " + std::to_string(failed - candidate->conversions.begin() + 1) +
                            " of type '" + readableTypeKey(failed->sourceTypeKey) + "' does not convert to '" +
                            readableTypeKey(failed->targetTypeKey) + "'" +
                            // Item 3.2b (D2): a literal is never handed over as writable memory.
                            (failed->sourceTypeKey == "string-literal" && failed->targetTypeKey.ends_with('*')
                                 ? std::string(" (a string literal is read-only and this C/C++ parameter may write "
                                               "through the pointer; pass a writable buffer)")
                                 : std::string{});
                else
                    note += " does not accept these arguments";
            }
        }
        if (function.cppLocation)
            note += ", declared at " + interop::describeCppSourcePoint(*function.cppLocation);
        notes.push_back(std::move(note));
    }
    return notes;
}

constexpr const char* kStructRootFacadeTypeKey = "<struct-root-facade>";
constexpr const char* kEnumRootFacadeTypeKey = "<enum-root-facade>";
constexpr const char* kIteratorFacadeTypeKey = "<iterator-facade>";

// Only summarize a source method whose entire body copies its value parameter
// into its ref/out parameter. Other effects require a separate flow summary.
std::optional<std::pair<std::size_t, std::size_t>> simpleDelegateReplacement(
    const ClassMemberSymbol& member)
{
    using K = ast::AstNodeKind;
    if (!member.isStatic || member.isVirtual || member.isOverride || member.isPrecompiled ||
        member.effects.contains(sema::Effect::Async) || member.effects.contains(sema::Effect::Generator) ||
        !member.declaredErrorTypeKey.empty() || member.declaredTypeKey != "builtin:void" || member.parameters.size() != 2 ||
        member.syntax == nullptr || member.syntax->first() == nullptr) return std::nullopt;
    const ast::AstNode* body = member.syntax->first();
    if (body->kind() != K::BlockStatement || body->children().size() != 1) return std::nullopt;
    const ast::AstNode* statement = body->children().front();
    const ast::AstNode* assignment = statement != nullptr && statement->kind() == K::ExpressionStatement
        ? statement->first() : nullptr;
    if (assignment == nullptr || assignment->kind() != K::AssignmentExpression ||
        assignment->tokenKind() != TokenKind::Equal || assignment->first() == nullptr ||
        assignment->second() == nullptr || assignment->first()->kind() != K::IdentifierExpression ||
        assignment->second()->kind() != K::IdentifierExpression) return std::nullopt;
    for (std::size_t target = 0; target < member.parameters.size(); ++target)
    {
        const std::size_t source = 1U - target;
        if (member.parameters[target].name != assignment->first()->text() ||
            member.parameters[source].name != assignment->second()->text() ||
            (member.parameters[target].passKind != ParameterPassKind::Ref &&
             member.parameters[target].passKind != ParameterPassKind::Out) ||
            member.parameters[source].passKind != ParameterPassKind::Value ||
            member.parameters[target].pointeeTypeKey != member.parameters[source].typeKey) continue;
        return std::pair{target, source};
    }
    return std::nullopt;
}

// Recognize only a source body whose callable parameter is used directly and
// linearly. Unknown uses are deliberately left for a future effect summary.
std::optional<unsigned> directCallableParameterInvocations(
    const ClassMemberSymbol& member, std::size_t parameterOrdinal)
{
    if (!member.isStatic || member.isVirtual || member.isOverride || member.isPrecompiled ||
        member.effects.contains(sema::Effect::Async) || member.effects.contains(sema::Effect::Generator) ||
        member.syntax == nullptr || member.syntax->first() == nullptr ||
        parameterOrdinal >= member.parameters.size()) return std::nullopt;
    const std::string& parameter = member.parameters[parameterOrdinal].name;
    using K = ast::AstNodeKind;
    std::vector<std::pair<const ast::AstNode*, const ast::AstNode*>> pending{{member.syntax->first(), nullptr}};
    std::unordered_set<const ast::AstNode*> visited;
    unsigned count = 0;
    unsigned returns = 0;
    while (!pending.empty())
    {
        const auto [node, parent] = pending.back();
        pending.pop_back();
        if (node == nullptr || !visited.insert(node).second) continue;
        const K kind = node->kind();
        if (kind == K::IfStatement || kind == K::WhileStatement || kind == K::DoWhileStatement ||
            kind == K::ForStatement || kind == K::ForeachStatement || kind == K::SwitchStatement ||
            kind == K::TryStatement || kind == K::LambdaExpression || kind == K::ConditionalExpression ||
            kind == K::LoopExpression || kind == K::MatchExpression || kind == K::NullCoalescingExpression ||
            (kind == K::BinaryExpression && (node->tokenKind() == TokenKind::AmpAmp ||
                                             node->tokenKind() == TokenKind::PipePipe))) return std::nullopt;
        if (kind == K::ReturnStatement && ++returns > 1) return std::nullopt;
        if (kind == K::VariableDeclarationStatement && node->text() == parameter) return std::nullopt;
        if (kind == K::IdentifierExpression && node->text() == parameter)
        {
            if (parent == nullptr || parent->kind() != K::CallExpression || parent->first() != node)
                return std::nullopt;
            ++count;
        }
        pending.emplace_back(node->first(), node);
        pending.emplace_back(node->second(), node);
        pending.emplace_back(node->third(), node);
        for (const ast::AstNode* child : node->children()) pending.emplace_back(child, node);
    }
    return count;
}

struct IndirectViewArguments final
{
    ClassMemberSymbol callable;
    MethodCallCandidate candidate;
    std::vector<const ast::AstNode*> syntaxes;
};

template <typename Arguments, typename LocalTypeLookup, typename ExpressionTypeLookup>
IndirectViewArguments prepareIndirectViewArguments(
    const ast::AstNode* expression, const Arguments& arguments, LocalTypeLookup&& localType,
    ExpressionTypeLookup&& expressionType)
{
    IndirectViewArguments result;
    for (std::size_t i = 0; i < arguments.size() && i < expression->children().size(); ++i)
    {
        MethodParameterSymbol parameter;
        parameter.passKind = arguments[i].argumentPassKind;
        parameter.typeKey = arguments[i].typeKey;
        parameter.pointeeTypeKey = arguments[i].typeKey;
        const ast::AstNode* syntax = expression->children()[i];
        if (syntax != nullptr && syntax->kind() == ast::AstNodeKind::UnaryExpression &&
            (syntax->auxiliaryText() == "pass:ref" || syntax->auxiliaryText() == "pass:out"))
        {
            parameter.passKind = syntax->auxiliaryText() == "pass:ref"
                ? ParameterPassKind::Ref : ParameterPassKind::Out;
            if (syntax->first() != nullptr && syntax->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                if (const auto type = localType(syntax->first()->text()); type.has_value())
                    parameter.pointeeTypeKey = *type;
            }
            else if (syntax->first() != nullptr && syntax->first()->kind() == ast::AstNodeKind::MemberExpression)
            {
                if (const auto type = expressionType(syntax->first()); type.has_value())
                    parameter.pointeeTypeKey = *type;
            }
        }
        result.callable.parameters.push_back(std::move(parameter));
        result.candidate.argumentParameterOrdinals.push_back(i);
        result.syntaxes.push_back(syntax);
    }
    return result;
}

// Add every owner recorded for an identifier alias; a reassignment may have
// made any of these owners the source of a field borrowed by the expression.
void appendAliasBorrowOwners(const ProjectMethodBodyModel& model, const ast::AstNode* root,
                             std::size_t callableOrdinal, std::vector<std::size_t>& owners)
{
    if (root == nullptr || root->kind() != ast::AstNodeKind::IdentifierExpression) return;
    const MethodNameBinding* binding = model.nameBinding(root, callableOrdinal);
    if (binding == nullptr || !binding->localOrdinal.has_value() ||
        *binding->localOrdinal >= model.locals().size()) return;
    const MethodLocalSymbol& local = model.locals()[*binding->localOrdinal];
    if (!local.localBorrowBindingOrdinal.has_value() ||
        *local.localBorrowBindingOrdinal >= model.localBorrowBindings().size()) return;
    const MethodLocalBorrowBinding& borrow = model.localBorrowBindings()[*local.localBorrowBindingOrdinal];
    if (borrow.sourceOwnerLocalOrdinal.has_value()) owners.push_back(*borrow.sourceOwnerLocalOrdinal);
    owners.insert(owners.end(), borrow.possibleSourceOwnerLocalOrdinals.begin(),
                  borrow.possibleSourceOwnerLocalOrdinals.end());
}

void retainNestedPrefixBorrowOwners(
    const ProjectMethodBodyModel& model,
    const ast::AstNode* argument,
    std::size_t callableOrdinal,
    std::vector<std::size_t>& activeOwners)
{
    std::vector<const ast::AstNode*> pending{argument};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) continue;
        if (node->kind() == ast::AstNodeKind::UnaryExpression &&
            (node->tokenKind() == TokenKind::PlusPlus || node->tokenKind() == TokenKind::MinusMinus))
        {
            const MethodExpressionTypeBinding* binding = model.expressionTypeBinding(node, callableOrdinal);
            if (binding != nullptr && binding->ownership == sema::OwnershipKind::Borrowed &&
                binding->ownershipSourceLocalOrdinal.has_value())
            {
                activeOwners.push_back(*binding->ownershipSourceLocalOrdinal);
                if (binding->ownsStorageDestination && node->first() != nullptr &&
                    node->first()->kind() == ast::AstNodeKind::MemberExpression)
                    appendAliasBorrowOwners(model, node->first()->first(), callableOrdinal, activeOwners);
            }
        }
        pending.push_back(node->first());
        pending.push_back(node->second());
        pending.push_back(node->third());
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
    }
}

void retainNestedPrefixBorrowFields(
    const ProjectMethodBodyModel& model,
    const ast::AstNode* expression,
    std::size_t callableOrdinal,
    std::vector<std::pair<std::size_t, std::size_t>>& activeFields,
    std::vector<std::pair<std::size_t, std::size_t>>& activeParameterFields)
{
    std::vector<const ast::AstNode*> pending{expression};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) continue;
        if (node->kind() == ast::AstNodeKind::UnaryExpression &&
            (node->tokenKind() == TokenKind::PlusPlus || node->tokenKind() == TokenKind::MinusMinus) &&
            node->first() != nullptr && node->first()->kind() == ast::AstNodeKind::MemberExpression &&
            node->first()->first() != nullptr &&
            node->first()->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const MethodExpressionTypeBinding* value = model.expressionTypeBinding(node, callableOrdinal);
            const MethodNameBinding* field = model.nameBinding(node->first(), callableOrdinal);
            if (value != nullptr && value->ownership == sema::OwnershipKind::Borrowed &&
                value->ownsStorageDestination && field != nullptr &&
                field->kind == MethodNameBindingKind::ExplicitObjectField &&
                field->candidateMemberOrdinals.size() == 1)
            {
                if (value->ownershipSourceLocalOrdinal.has_value())
                {
                    std::vector<std::size_t> owners{*value->ownershipSourceLocalOrdinal};
                    appendAliasBorrowOwners(model, node->first()->first(), callableOrdinal, owners);
                    for (const std::size_t owner : owners)
                        activeFields.emplace_back(owner, field->candidateMemberOrdinals[0]);
                }
                if (value->ownershipSourceParameterOrdinal.has_value())
                    activeParameterFields.emplace_back(*value->ownershipSourceParameterOrdinal,
                                                       field->candidateMemberOrdinals[0]);
            }
        }
        pending.push_back(node->first());
        pending.push_back(node->second());
        pending.push_back(node->third());
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
    }
}

NamedArgumentMap mapPositionalArguments(
    std::span<ast::AstNode* const> arguments,
    const ClassMemberSymbol& candidate,
    std::size_t reservedLeadingParameters,
    NamedArgumentMap result)
{
    const std::size_t fixedEnd = result.parameterPackOrdinal.value_or(candidate.parameters.size());
    const std::size_t fixedAvailable = fixedEnd - reservedLeadingParameters;
    if (arguments.size() > fixedAvailable && !result.parameterPackOrdinal.has_value() &&
        !candidate.isVariadic) return result;

    const std::size_t fixedArguments = std::min(arguments.size(), fixedAvailable);
    result.explicitParameterOrdinals.reserve(arguments.size());
    result.parameterPackElements.reserve(arguments.size());
    for (std::size_t index = 0; index < fixedArguments; ++index)
    {
        if (arguments[index] == nullptr) return {};
        result.explicitParameterOrdinals.push_back(reservedLeadingParameters + index);
        result.parameterPackElements.push_back(false);
    }
    for (std::size_t index = fixedArguments; index < arguments.size(); ++index)
    {
        if (arguments[index] == nullptr) return {};
        if (result.parameterPackOrdinal.has_value())
        {
            result.explicitParameterOrdinals.push_back(*result.parameterPackOrdinal);
            result.parameterPackElements.push_back(true);
            result.usesExpandedParameterPack = true;
        }
        else
        {
            result.explicitParameterOrdinals.push_back(
                candidate.parameters.size() + index - fixedArguments);
            result.parameterPackElements.push_back(false);
        }
    }
    for (std::size_t index = reservedLeadingParameters + fixedArguments; index < fixedEnd; ++index)
    {
        if (candidate.parameters[index].defaultValueSyntax == nullptr) return {};
        result.defaultedParameterOrdinals.push_back(index);
    }
    result.synthesizesEmptyParameterPack =
        result.parameterPackOrdinal.has_value() && !result.usesExpandedParameterPack;
    result.valid = true;
    return result;
}

bool mapNamedArgument(
    const ast::AstNode* argument,
    const ClassMemberSymbol& candidate,
    NamedArgumentMap& result,
    std::vector<bool>& assigned,
    std::size_t reservedLeadingParameters,
    std::size_t& nextPositional,
    std::size_t& variadicOrdinal,
    bool& reachedVariadicArguments,
    bool& reachedExpandedParameterPack,
    bool& assignedParameterPackDirectly)
{
    if (argument == nullptr) return false;
    std::optional<std::size_t> parameterOrdinal;
    bool parameterPackElement = false;
    if (!argument->argumentName().empty())
    {
        // The ellipsis has neither a name nor a parameter symbol. Once a positional
        // argument has crossed the fixed prefix, a later named argument would otherwise
        // make the physical C argument order ambiguous.
        if (reachedVariadicArguments || reachedExpandedParameterPack) return false;
        for (std::size_t index = reservedLeadingParameters; index < candidate.parameters.size(); ++index)
        {
            if (candidate.parameters[index].name == argument->argumentName())
            {
                parameterOrdinal = index;
                break;
            }
        }
        if (parameterOrdinal == result.parameterPackOrdinal)
        {
            if (assignedParameterPackDirectly) return false;
            assignedParameterPackDirectly = true;
        }
    }
    else
    {
        while (nextPositional < assigned.size() && assigned[nextPositional]) ++nextPositional;
        if (nextPositional < assigned.size())
        {
            parameterOrdinal = nextPositional;
            if (parameterOrdinal == result.parameterPackOrdinal)
            {
                if (assignedParameterPackDirectly) return false;
                parameterPackElement = true;
                reachedExpandedParameterPack = true;
            }
        }
        else if (candidate.isVariadic)
        {
            parameterOrdinal = candidate.parameters.size() + variadicOrdinal++;
            reachedVariadicArguments = true;
        }
    }
    if (!parameterOrdinal.has_value()) return false;
    if (*parameterOrdinal < assigned.size())
    {
        if (assigned[*parameterOrdinal] && !parameterPackElement) return false;
        if (!parameterPackElement) assigned[*parameterOrdinal] = true;
    }
    result.explicitParameterOrdinals.push_back(*parameterOrdinal);
    result.parameterPackElements.push_back(parameterPackElement);
    result.usesExpandedParameterPack = result.usesExpandedParameterPack || parameterPackElement;
    while (nextPositional < assigned.size() && assigned[nextPositional]) ++nextPositional;
    return true;
}

bool appendDefaultedParameters(
    const ClassMemberSymbol& candidate,
    std::size_t reservedLeadingParameters,
    const std::vector<bool>& assigned,
    bool assignedParameterPackDirectly,
    NamedArgumentMap& result)
{
    for (std::size_t index = reservedLeadingParameters; index < candidate.parameters.size(); ++index)
    {
        if (index == result.parameterPackOrdinal)
        {
            result.synthesizesEmptyParameterPack =
                !result.usesExpandedParameterPack && !assignedParameterPackDirectly;
            continue;
        }
        if (assigned[index]) continue;
        if (candidate.parameters[index].defaultValueSyntax == nullptr) return false;
        result.defaultedParameterOrdinals.push_back(index);
    }
    return true;
}

NamedArgumentMap mapArgumentsToParameters(
    std::span<ast::AstNode* const> arguments,
    const ClassMemberSymbol& candidate,
    std::size_t reservedLeadingParameters = 0)
{
    NamedArgumentMap result;
    if (reservedLeadingParameters > candidate.parameters.size()) return result;
    if (!candidate.parameters.empty() && candidate.parameters.back().isParameterPack)
    {
        result.parameterPackOrdinal = candidate.parameters.size() - 1U;
        if (*result.parameterPackOrdinal < reservedLeadingParameters) return {};
    }

    // Positional calls are overwhelmingly the common case, especially in generated numeric
    // libraries. Reject impossible arities before allocating the per-candidate assignment map.
    // The general path below remains authoritative for named/reordered arguments.
    const bool hasNamedArguments = std::ranges::any_of(arguments, [](const ast::AstNode* argument)
    {
        return argument != nullptr && !argument->argumentName().empty();
    });
    if (!hasNamedArguments)
    {
        return mapPositionalArguments(arguments, candidate, reservedLeadingParameters, std::move(result));
    }

    std::vector<bool> assigned(candidate.parameters.size(), false);
    for (std::size_t index = 0; index < reservedLeadingParameters; ++index) assigned[index] = true;

    std::size_t nextPositional = reservedLeadingParameters;
    std::size_t variadicOrdinal = 0;
    bool reachedVariadicArguments = false;
    bool reachedExpandedParameterPack = false;
    bool assignedParameterPackDirectly = false;
    for (const ast::AstNode* argument : arguments)
    {
        if (!mapNamedArgument(
                argument,
                candidate,
                result,
                assigned,
                reservedLeadingParameters,
                nextPositional,
                variadicOrdinal,
                reachedVariadicArguments,
                reachedExpandedParameterPack,
                assignedParameterPackDirectly)) return result;
    }

    if (!appendDefaultedParameters(
            candidate, reservedLeadingParameters, assigned, assignedParameterPackDirectly, result)) return result;
    result.valid = true;
    return result;
}

bool isErrorType(std::string_view key) noexcept
{
    return key.empty() || key == "<error>";
}

std::optional<std::size_t> snapshotLocalIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t globalLocalOrdinal) noexcept
{
    if (globalLocalOrdinal < snapshot.localBaseOrdinal ||
        globalLocalOrdinal >= snapshot.localCount)
    {
        return std::nullopt;
    }
    const std::size_t index = globalLocalOrdinal - snapshot.localBaseOrdinal;
    return index < snapshot.localFlags.size() ? std::optional<std::size_t>(index) : std::nullopt;
}

std::optional<std::size_t> snapshotStorageIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept
{
    if ((storageKey & 1U) != 0U)
    {
        const std::size_t parameterOrdinal = storageKey / 2U;
        return parameterOrdinal < snapshot.parameterFlags.size()
            ? std::optional<std::size_t>(parameterOrdinal)
            : std::nullopt;
    }
    const std::optional<std::size_t> localIndex = snapshotLocalIndex(
        snapshot,
        storageKey / 2U);
    return localIndex.has_value()
        ? std::optional<std::size_t>(snapshot.parameterFlags.size() + *localIndex)
        : std::nullopt;
}

OwnershipFlowStorageFacts* mutableSnapshotStorageFacts(
    OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept
{
    const std::optional<std::size_t> index = snapshotStorageIndex(snapshot, storageKey);
    return index.has_value() && *index < snapshot.storageFacts.size()
        ? &snapshot.storageFacts[*index]
        : nullptr;
}
}


ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeNamespaceFunctionGroup(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    ExpressionResult result;
    result.methodGroup = true;
    result.valid = allowMethodGroup;
    result.typeKey = "<method-group>";
    result.methodCandidates.reserve(reference.candidateFunctionOrdinals.size());
    for (const std::size_t functionOrdinal : reference.candidateFunctionOrdinals)
    {
        if (functionOrdinal < namespaceCallableAdapters().size())
            result.methodCandidates.push_back(namespaceCallableBase_ + functionOrdinal);
    }
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::StaticMethodGroup,
        expression,
        callerCallableOrdinal,
        scopeOrdinal,
        reference.writtenName,
        "<method-group>",
        std::nullopt,
        std::nullopt,
        result.methodCandidates,
        nullptr,
        "",
        std::nullopt,
        false});
    if (!allowMethodGroup)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression != nullptr ? expression->range() : SourceRange{},
            "namespace function group '" + reference.writtenName +
                "' can only be used as a call target or converted to a delegate");
    }
    return result;
}

void ProjectMethodBodyModel::storeNamespaceFunctionCall(
    const ast::AstNode* expression,
    const NamespaceFunctionCallBinding& call)
{
    namespaceFunctionCallBySyntax_[expression] = namespaceFunctionCalls_.size();
    namespaceFunctionCalls_.push_back(call);
}

std::vector<ast::AstNode*> ProjectMethodBodyModel::namespaceFunctionArgumentSyntaxes(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    const ExpressionResult* forwardedReceiver,
    const ast::AstNode* extraArgumentSyntax,
    std::size_t explicitArgumentCount) const
{
    std::vector<ast::AstNode*> argumentSyntaxes;
    argumentSyntaxes.reserve(explicitArgumentCount + (forwardedReceiver != nullptr ? 1U : 0U));
    if (forwardedReceiver != nullptr)
    {
        argumentSyntaxes.push_back(const_cast<ast::AstNode*>(reference.receiverSyntax));
    }
    if (extraArgumentSyntax != nullptr)
    {
        argumentSyntaxes.push_back(const_cast<ast::AstNode*>(extraArgumentSyntax));
    }
    else
    {
        argumentSyntaxes.insert(
            argumentSyntaxes.end(), expression->children().begin(), expression->children().end());
    }
    return argumentSyntaxes;
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::bindNamespaceFunctionMethodGroupArgument(
    const ast::AstNode* argument,
    ExpressionResult analyzed,
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::size_t argumentOrdinal,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    bool hasForwardedReceiver,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<std::string> targets;
    for (const std::size_t functionOrdinal : reference.candidateFunctionOrdinals)
    {
        if (functionOrdinal >= namespaceCallableAdapters().size()) continue;
        const ClassMemberSymbol& candidate = namespaceCallableAdapters()[functionOrdinal];
        const NamedArgumentMap mapping = mapArgumentsToParameters(argumentSyntaxes, candidate);
        const std::size_t mappedArgumentOrdinal =
            argumentOrdinal + (hasForwardedReceiver ? 1U : 0U);
        if (!mapping.valid || mappedArgumentOrdinal >= mapping.explicitParameterOrdinals.size())
        {
            continue;
        }
        const std::size_t parameterOrdinal =
            mapping.explicitParameterOrdinals[mappedArgumentOrdinal];
        if (parameterOrdinal >= candidate.parameters.size()) continue;
        const std::string& target = candidate.parameters[parameterOrdinal].typeKey;
        if (methodGroupExactlyMatchesDelegate(analyzed, target) &&
            std::ranges::find(targets, target) == targets.end())
        {
            targets.push_back(target);
        }
    }
    if (targets.size() == 1)
    {
        return bindMethodGroupToDelegate(
            argument, analyzed, targets.front(), callerCallableOrdinal, scopeOrdinal, diagnostics);
    }
    diagnostics.report(
        targets.empty() ? diagnostics::DiagnosticId::InvalidConversion
                        : diagnostics::DiagnosticId::InvalidCall,
        diagnostics::DiagnosticSeverity::Error,
        argument->range(),
        targets.empty()
            ? "method group does not match a delegate parameter of namespace function '" +
                reference.writtenName + "'"
            : "method group argument has more than one compatible delegate target in namespace function '" +
                reference.writtenName + "'");
    return analyzed;
}

void ProjectMethodBodyModel::rememberDirectOwnedFieldArgument(
    const ast::AstNode* expression, const ExpressionResult& argument,
    std::size_t methodMemberOrdinal, std::size_t scopeOrdinal)
{
    if (!argument.valid || expression == nullptr ||
        expression->kind() != ast::AstNodeKind::MemberExpression ||
        expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
        !argument.assignmentMemberOrdinal.has_value() || memberModel_ == nullptr ||
        *argument.assignmentMemberOrdinal >= memberModel_->members().size() ||
        !isReferenceTypeKey(argument.typeKey))
        return;
    const ClassMemberSymbol& field = memberModel_->members()[*argument.assignmentMemberOrdinal];
    if (field.kind != ClassMemberKind::Field || field.isStatic ||
        field.declaredOwnership != sema::OwnershipKind::Owned)
        return;
    const auto root = lookupScopeName(scopeOrdinal, expression->first()->text());
    const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
    if (root.has_value() && root->kind == ScopeNameKind::Parameter && caller != nullptr &&
        root->semanticOrdinal < caller->parameters.size() &&
        caller->parameters[root->semanticOrdinal].ownership == sema::OwnershipKind::Borrowed)
    {
        activeArgumentParameterOwnedFields_.emplace_back(root->semanticOrdinal, *argument.assignmentMemberOrdinal);
        return;
    }
    if (root.has_value() && root->kind == ScopeNameKind::Local && root->semanticOrdinal < locals_.size() &&
        caller != nullptr)
    {
        const MethodLocalSymbol& local = locals_[root->semanticOrdinal];
        if (local.localBorrowBindingOrdinal.has_value() &&
            *local.localBorrowBindingOrdinal < localBorrowBindings_.size())
        {
            const MethodLocalBorrowBinding& borrow = localBorrowBindings_[*local.localBorrowBindingOrdinal];
            const auto parameter = borrow.sourceOwnerParameterOrdinal.has_value()
                ? borrow.sourceOwnerParameterOrdinal
                : (borrow.possibleSourceOwnerParameterOrdinals.empty() ? std::nullopt
                    : std::optional<std::size_t>(borrow.possibleSourceOwnerParameterOrdinals.front()));
            const bool validLocals = (!borrow.sourceOwnerLocalOrdinal.has_value() ||
                (*borrow.sourceOwnerLocalOrdinal < locals_.size() && locals_[*borrow.sourceOwnerLocalOrdinal].isRaii)) &&
                std::ranges::all_of(borrow.possibleSourceOwnerLocalOrdinals,
                    [&](std::size_t candidate) { return candidate < locals_.size() && locals_[candidate].isRaii; });
            if (parameter.has_value() && validLocals && *parameter < caller->parameters.size() &&
                caller->parameters[*parameter].ownership == sema::OwnershipKind::Borrowed &&
                std::ranges::all_of(borrow.possibleSourceOwnerParameterOrdinals,
                    [&](std::size_t candidate)
                    { return candidate < caller->parameters.size() &&
                        caller->parameters[candidate].ownership == sema::OwnershipKind::Borrowed; }))
            {
                activeArgumentParameterOwnedFields_.emplace_back(*parameter, *argument.assignmentMemberOrdinal);
                if (borrow.sourceOwnerLocalOrdinal.has_value())
                    activeArgumentOwnedFields_.emplace_back(*borrow.sourceOwnerLocalOrdinal,
                                                            *argument.assignmentMemberOrdinal);
                for (const std::size_t possible : borrow.possibleSourceOwnerLocalOrdinals)
                    activeArgumentOwnedFields_.emplace_back(possible, *argument.assignmentMemberOrdinal);
                return;
            }
        }
    }
    if (!root.has_value() || root->kind != ScopeNameKind::Local ||
        root->semanticOrdinal >= locals_.size() || !locals_[root->semanticOrdinal].isRaii)
        return;
    activeArgumentOwnedFields_.emplace_back(root->semanticOrdinal, *argument.assignmentMemberOrdinal);
}

std::vector<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeNamespaceFunctionArguments(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::size_t explicitArgumentCount,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    const ExpressionResult* forwardedReceiver,
    const ast::AstNode* extraArgumentSyntax,
    bool& argumentsValid,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<ExpressionResult> arguments;
    arguments.reserve(argumentSyntaxes.size());
    const std::size_t prefixBorrowBase = activePrefixArgumentOwnerLocals_.size();
    bool arrayArgumentEscapes = false;
    const std::size_t fieldBorrowBase = activeArgumentOwnedFields_.size();
    const std::size_t parameterFieldBorrowBase = activeArgumentParameterOwnedFields_.size();
    if (forwardedReceiver != nullptr)
    {
        ExpressionResult receiverArgument = *forwardedReceiver;
        receiverArgument.typeKey = forwardedReceiver->receiverTypeKey;
        receiverArgument.methodGroup = false;
        receiverArgument.extensionMethodGroup = false;
        receiverArgument.valid = true;
        const MethodExpressionTypeBinding* receiverType = reference.receiverSyntax != nullptr
            ? expressionTypeBinding(reference.receiverSyntax, callerCallableOrdinal) : nullptr;
        if (receiverType != nullptr && receiverType->ownershipSourceLocalOrdinal.has_value() &&
            (receiverType->ownership == sema::OwnershipKind::Borrowed ||
             (receiverType->ownership == sema::OwnershipKind::Owned && !receiverType->ownershipTransfer)))
            activePrefixArgumentOwnerLocals_.push_back(*receiverType->ownershipSourceLocalOrdinal);
        else if (receiverType != nullptr && receiverType->ownership == sema::OwnershipKind::Borrowed)
            retainNestedPrefixBorrowOwners(*this, reference.receiverSyntax, callerCallableOrdinal,
                                          activePrefixArgumentOwnerLocals_);
        if (receiverType != nullptr && receiverType->ownership == sema::OwnershipKind::Borrowed)
            retainNestedPrefixBorrowFields(*this, reference.receiverSyntax, callerCallableOrdinal,
                                          activeArgumentOwnedFields_, activeArgumentParameterOwnedFields_);
        if (receiverArgument.readonlyBorrowedObject)
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, expression->range(),
                "borrowed argument requires a readonly-safe parameter; readonly borrowed object "
                "cannot be passed to an unchecked function");
            argumentsValid = false;
        }
        arguments.push_back(std::move(receiverArgument));
    }
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < explicitArgumentCount; ++argumentOrdinal)
    {
        // T19-040: the written arguments without the macro tokens. / Los argumentos escritos sin los tokens de
        //     macro.
        const std::size_t syntaxOrdinal = argumentOrdinal + (forwardedReceiver != nullptr ? 1U : 0U);
        const ast::AstNode* argument = extraArgumentSyntax != nullptr ? extraArgumentSyntax
            : syntaxOrdinal < argumentSyntaxes.size() ? argumentSyntaxes[syntaxOrdinal]
                                                      : expression->children()[argumentOrdinal];
        // EN: C4-01. A lambda written as the argument of an imported C/C++ function (one candidate) in a function
        //     pointer position takes that type: its parameters and result come from the C signature and it reaches
        //     the pointer through its generated thunk, as `binop f = (a, b) => a + b;` does. A captured state stays
        //     rejected by the binding; the C pointer is never the internal delegate representation.
        // ES: C4-01. Una lambda escrita como argumento de una funcion C/C++ importada (un candidato) en una posicion
        //     puntero a funcion toma ese tipo: sus parametros y resultado salen de la firma C y llega al puntero por su
        //     thunk generado, como `binop f = (a, b) => a + b;`. Un estado capturado sigue rechazado por el enlace;
        //     el puntero C nunca es la representacion interna de delegado.
        const NamespaceFunctionSymbol* cTarget = nullptr;
        if (argument != nullptr && argument->kind() == ast::AstNodeKind::LambdaExpression &&
            forwardedReceiver == nullptr && extraArgumentSyntax == nullptr && namespaceFunctionModel_ != nullptr &&
            reference.candidateFunctionOrdinals.size() == 1U &&
            reference.candidateFunctionOrdinals.front() < namespaceFunctionModel_->functions().size())
        {
            const auto& candidate = namespaceFunctionModel_->functions()[reference.candidateFunctionOrdinals.front()];
            if (!candidate.cppCanonicalIdentity.empty() && candidate.definitionSyntax == nullptr &&
                argumentOrdinal < candidate.parameters.size() &&
                candidate.parameters[argumentOrdinal].passKind == ParameterPassKind::Value)
                if (const auto function = language::parseFunctionTypeIdentity(candidate.parameters[argumentOrdinal].typeKey);
                    function && function->carrier == language::FunctionCarrier::Pointer &&
                    function->callingConvention != language::CallingConvention::NativeSharp)
                    cTarget = &candidate;
        }
        ExpressionResult analyzed = cTarget != nullptr
            ? bindLambdaToDelegate(argument, cTarget->parameters[argumentOrdinal].typeKey,
                                   callerCallableOrdinal, scopeOrdinal, diagnostics)
            : analyzeExpression(argument, callerCallableOrdinal, scopeOrdinal, diagnostics, true, false, true);
        if (analyzed.methodGroup)
        {
            analyzed = bindNamespaceFunctionMethodGroupArgument(
                argument, std::move(analyzed), reference, argumentSyntaxes, argumentOrdinal,
                callerCallableOrdinal, scopeOrdinal, forwardedReceiver != nullptr, diagnostics);
        }
        if (analyzed.readonlyBorrowedObject)
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, argument->range(),
                "borrowed argument requires a readonly-safe parameter; readonly borrowed object "
                "cannot be passed to an unchecked function");
            argumentsValid = false;
        }
        argumentsValid = argumentsValid && analyzed.valid && !analyzed.methodGroup;
        if (analyzed.valid && analyzed.ownershipSourceLocalOrdinal.has_value() &&
            (analyzed.ownership == sema::OwnershipKind::Borrowed ||
             (analyzed.ownership == sema::OwnershipKind::Owned && !analyzed.ownershipTransfer)))
            activePrefixArgumentOwnerLocals_.push_back(*analyzed.ownershipSourceLocalOrdinal);
        if (analyzed.valid && analyzed.ownership == sema::OwnershipKind::Borrowed)
            retainNestedPrefixBorrowOwners(*this, argument, callerCallableOrdinal, activePrefixArgumentOwnerLocals_);
        const std::size_t fieldsBefore = activeArgumentOwnedFields_.size();
        const std::size_t parameterFieldsBefore = activeArgumentParameterOwnedFields_.size();
        rememberDirectOwnedFieldArgument(argument, analyzed, callerCallableOrdinal, scopeOrdinal);
        if (activeArgumentOwnedFields_.size() != fieldsBefore ||
            activeArgumentParameterOwnedFields_.size() != parameterFieldsBefore)
        {
            analyzed.ownership = sema::OwnershipKind::Borrowed;
            analyzed.objectLifetimeOwnership = sema::OwnershipKind::Borrowed;
            analyzed.storageOwnership = sema::OwnershipKind::Borrowed;
            if (activeArgumentOwnedFields_.size() != fieldsBefore)
                analyzed.ownershipSourceLocalOrdinal = activeArgumentOwnedFields_.back().first;
            if (activeArgumentParameterOwnedFields_.size() != parameterFieldsBefore)
                analyzed.ownershipSourceParameterOrdinal = activeArgumentParameterOwnedFields_.back().first;
            recordExpressionType(argument, callerCallableOrdinal, scopeOrdinal, analyzed);
        }
        if (analyzed.valid && analyzed.ownership == sema::OwnershipKind::Borrowed)
            retainNestedPrefixBorrowFields(*this, argument, callerCallableOrdinal, activeArgumentOwnedFields_,
                                          activeArgumentParameterOwnedFields_);
        // EN: Applied after this call's view checks: the array escapes during the call, not before it.
        // ES: Se aplica tras las comprobaciones de vistas de esta llamada: escapa durante ella.
        if (analyzed.valid && ProjectTypeConversion::isNativeArray(analyzed.typeKey))
            arrayArgumentEscapes = true;
        arguments.push_back(std::move(analyzed));
    }
    for (const ast::AstNode* argument : argumentSyntaxes)
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression) continue;
        if (forwardedReceiver != nullptr && argument == reference.receiverSyntax) continue;
        if ((activeArgumentOwnedFields_.empty() && activeArgumentParameterOwnedFields_.empty()) ||
            !hasActiveViewOfStorage(argument, callerCallableOrdinal, scopeOrdinal, true)) continue;
        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error, argument->range(),
            "cannot pass object with a borrowed field view to a potentially mutating call");
        argumentsValid = false;
    }
    if (forwardedReceiver != nullptr &&
        (!activeArgumentOwnedFields_.empty() || !activeArgumentParameterOwnedFields_.empty()) &&
        reference.receiverSyntax != nullptr &&
        reference.receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
        hasActiveViewOfStorage(reference.receiverSyntax, callerCallableOrdinal, scopeOrdinal, true))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error, reference.receiverSyntax->range(),
            "cannot call potentially mutating method while a borrowed field view is alive");
        argumentsValid = false;
    }
    // EN: Marking every array local before the checks made a freshly allocated array, passed for
    //     the first time, conflict with an older view of unknown root (NSC-NEW-018).
    // ES: Marcar cada local arreglo antes de las comprobaciones hacía que un arreglo recién creado,
    //     pasado por primera vez, chocara con una vista anterior de raíz desconocida (NSC-NEW-018).
    for (const ast::AstNode* argument : argumentSyntaxes)
    {
        const ast::AstNode* named = argument != nullptr && argument->kind() == ast::AstNodeKind::MoveExpression
            ? argument->first() : argument;
        if (named == nullptr || named->kind() != ast::AstNodeKind::IdentifierExpression) continue;
        const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, named->text());
        if (name.has_value() && name->kind == ScopeNameKind::Local && name->semanticOrdinal < locals_.size())
            locals_[name->semanticOrdinal].passedToCall = true;
    }
    if (arrayArgumentEscapes)
        for (MethodLocalSymbol& local : locals_)
            if (local.methodMemberOrdinal == callerCallableOrdinal &&
                ProjectTypeConversion::isNativeArray(local.typeKey))
                local.mayAliasUnknownReference = true;
    activePrefixArgumentOwnerLocals_.resize(prefixBorrowBase);
    activeArgumentOwnedFields_.resize(fieldBorrowBase);
    activeArgumentParameterOwnedFields_.resize(parameterFieldBorrowBase);
    return arguments;
}

void ProjectMethodBodyModel::normalizeNamespaceFunctionParameterPack(
    const ClassMemberSymbol& candidate,
    std::span<const ExpressionResult> arguments,
    MethodCallCandidate& evaluated) const
{
    if (!evaluated.usesExpandedParameterPack ||
        std::ranges::count(evaluated.parameterPackElements, true) != 1 ||
        !evaluated.parameterPackOrdinal.has_value())
    {
        return;
    }
    const auto packElement = std::ranges::find(evaluated.parameterPackElements, true);
    const std::size_t argumentOrdinal = static_cast<std::size_t>(
        std::distance(evaluated.parameterPackElements.begin(), packElement));
    const MethodParameterSymbol& pack = candidate.parameters[*evaluated.parameterPackOrdinal];
    if (classifyImplicit(
            arguments[argumentOrdinal].typeKey,
            pack.typeKey,
            arguments[argumentOrdinal].integerConstant).exists())
    {
        evaluated.parameterPackElements[argumentOrdinal] = false;
        evaluated.usesExpandedParameterPack = false;
    }
}

std::optional<ProjectImplicitConversion>
ProjectMethodBodyModel::evaluateNamespaceVariadicCandidateArgument(
    std::span<const ExpressionResult> arguments,
    std::size_t argumentOrdinal,
    std::size_t parameterOrdinal,
    const ClassMemberSymbol& candidate,
    MethodCallCandidate& evaluated) const
{
    if (parameterOrdinal < candidate.parameters.size()) return std::nullopt;
    ProjectImplicitConversion conversion =
        classifyCVariadicPromotion(arguments[argumentOrdinal].typeKey,
            namespaceFunctionModel_ && evaluated.memberOrdinal < namespaceFunctionModel_->functions().size() &&
            (namespaceFunctionModel_->functions()[evaluated.memberOrdinal].cppCallingConvention == "cdecl" ||
             namespaceFunctionModel_->functions()[evaluated.memberOrdinal].cppCallingConvention == "sysv_abi") &&
            !namespaceFunctionModel_->functions()[evaluated.memberOrdinal].cppCanonicalIdentity.empty());
    const auto& key = arguments[argumentOrdinal].typeKey;
    if (!conversion.exists() && namespaceFunctionModel_ && nameLookup_ &&
        evaluated.memberOrdinal < namespaceFunctionModel_->functions().size() &&
        (namespaceFunctionModel_->functions()[evaluated.memberOrdinal].cppCallingConvention == "sysv_abi" ||
         namespaceFunctionModel_->functions()[evaluated.memberOrdinal].cppCallingConvention == "cdecl") &&
        !namespaceFunctionModel_->functions()[evaluated.memberOrdinal].cppCanonicalIdentity.empty() &&
        key.starts_with("type:") && !key.ends_with('*'))
    {
        const auto ordinal = nameLookup_->typeOrdinalForIdentity(std::string_view(key).substr(5));
        const auto* imported = ordinal ? nameLookup_->cppImportedType(*ordinal) : nullptr;
        if (imported && imported->origin == CppImportedTypeOrigin::CppRecord &&
            !imported->isIncomplete && imported->deferredImportError.empty() && imported->clangUsr &&
            imported->objectLayout && imported->cppValueTraits &&
            imported->cppValueTraits->triviallyCopyable && imported->cppValueTraits->standardLayout &&
            imported->cppValueTraits->trivialDestructor &&
            !imported->cppValueTraits->hasNonLiteralOrVolatileFieldsOrBases &&
            (namespaceFunctionModel_->functions()[evaluated.memberOrdinal].cppCallingConvention != "cdecl" ||
             (imported->cppValueOperations && interop::cppRecordValueOperationsEligible(
                 *imported->cppValueTraits, *imported->cppValueOperations))))
            conversion = ProjectImplicitConversion{ProjectConversionKind::Identity, 10, key, key, std::nullopt};
    }
    if (!conversion.exists() || arguments[argumentOrdinal].pointerPointeeVolatile ||
        arguments[argumentOrdinal].cppPointerPointeeAtomic ||
        arguments[argumentOrdinal].argumentPassKind != ParameterPassKind::Value ||
        arguments[argumentOrdinal].ownership != sema::OwnershipKind::None)
    {
        evaluated.viable = false;
        evaluated.invalidVariadicArgumentOrdinal = argumentOrdinal;
    }
    return conversion;
}

void ProjectMethodBodyModel::validateNamespaceFunctionCandidateArgumentOwnership(
    const MethodParameterSymbol& parameter,
    std::span<const ExpressionResult> arguments,
    std::size_t argumentOrdinal,
    bool packElement,
    MethodCallCandidate& evaluated) const
{
    const bool freshStringBorrow = !packElement && parameter.typeKey == "builtin:string" &&
        (parameter.ownership == sema::OwnershipKind::None ||
         parameter.ownership == sema::OwnershipKind::Borrowed) &&
        arguments[argumentOrdinal].ownership == sema::OwnershipKind::Owned &&
        arguments[argumentOrdinal].ownershipTransfer &&
        arguments[argumentOrdinal].ownershipFreshOwner;
    const bool postfixBorrow = !packElement && parameter.ownership == sema::OwnershipKind::Borrowed &&
        arguments[argumentOrdinal].ownedPostfixTemporary &&
        arguments[argumentOrdinal].ownership == sema::OwnershipKind::Owned &&
        arguments[argumentOrdinal].ownershipTransfer && arguments[argumentOrdinal].ownershipFreshOwner;
    if (freshStringBorrow || postfixBorrow)
    {
        evaluated.fullExpressionBorrowArgumentOrdinals.push_back(argumentOrdinal);
    }
    else if (classifyOwnershipArgument(
                 packElement ? false : typeKeyIsOwnable(parameter.typeKey),
                 packElement ? sema::OwnershipKind::None : parameter.ownership,
                 arguments[argumentOrdinal].ownership,
                 arguments[argumentOrdinal].ownershipTransfer,
                 arguments[argumentOrdinal].ownershipFreshOwner,
                 arguments[argumentOrdinal].typeKey == "builtin:null") != OwnershipArgumentFit::Ok)
    {
        evaluated.viable = false;
    }
}

ProjectImplicitConversion ProjectMethodBodyModel::evaluateNamespaceFunctionCandidateArgument(
    const ClassMemberSymbol& candidate,
    std::span<const ExpressionResult> arguments,
    std::size_t argumentOrdinal,
    bool hasForwardedReceiver,
    MethodCallCandidate& evaluated) const
{
    const std::size_t parameterOrdinal = evaluated.argumentParameterOrdinals[argumentOrdinal];
    const std::optional<ProjectImplicitConversion> variadic =
        evaluateNamespaceVariadicCandidateArgument(
            arguments, argumentOrdinal, parameterOrdinal, candidate, evaluated);
    if (variadic.has_value()) return *variadic;
    const MethodParameterSymbol& parameter = candidate.parameters[parameterOrdinal];
    // EN: T19-007. A value written without `ref` for a C++ `T&&` parameter converts to the referent type and is
    //     materialized for the call; an lvalue of that very type is rejected, as C++ does not bind it.
    // ES: T19-007. Un valor escrito sin `ref` para un parametro C++ `T&&` se convierte al tipo del referente y se
    //     materializa para la llamada; un lvalue de ese mismo tipo se rechaza, porque C++ no lo enlaza.
    const bool rvalueArgument = parameter.cppRvalueReference &&
        arguments[argumentOrdinal].argumentPassKind == ParameterPassKind::Value;
    std::string conversionTarget = (hasForwardedReceiver && argumentOrdinal == 0 &&
            parameter.passKind != ParameterPassKind::Value) || rvalueArgument
        ? parameter.pointeeTypeKey
        : parameter.typeKey;
    const bool packElement = argumentOrdinal < evaluated.parameterPackElements.size() &&
        evaluated.parameterPackElements[argumentOrdinal];
    if (packElement)
    {
        if (parameter.parameterPackElementTypeKey.empty())
        {
            evaluated.viable = false;
        }
        else
        {
            conversionTarget = parameter.parameterPackElementTypeKey;
        }
    }
    ProjectImplicitConversion conversion =
        classifyImportedFlexibleConversion(arguments[argumentOrdinal], conversionTarget);
    // EN: Items 3.2 and 3.3 (D3): the C conversions only at a C boundary, a function with C linkage (its symbol is
    //     not a decorated C++ name). A C++ function or method keeps NativeSharp's conversions, so a private better
    //     overload still leaves no viable one (pruebas-tanda-10-acceso).
    // ES: Items 3.2 y 3.3 (D3): las conversiones C solo en una frontera C, una funcion con enlace C (su simbolo no
    //     es un nombre C++ decorado). Una funcion o metodo C++ conserva las conversiones de NativeSharp.
    const auto* cFunction = namespaceFunctionModel_ != nullptr &&
            evaluated.memberOrdinal < namespaceFunctionModel_->functions().size()
        ? &namespaceFunctionModel_->functions()[evaluated.memberOrdinal] : nullptr;
    const bool importedFunction = cFunction != nullptr && cFunction->cppReturnType.has_value();
    const bool cLinkage = candidate.isImportedC || (importedFunction && !cFunction->mangledName.starts_with('?'));
    if (!conversion.exists() && !packElement && (candidate.isImportedC || candidate.isCppExternal || importedFunction))
        conversion = classifyCFrontierConversion(arguments[argumentOrdinal], parameter, conversionTarget, cLinkage);
    // EN: Freeze P3: an `in` parameter only reads its argument, so a readonly place (another
    //     `in` parameter, a readonly field, a `ref readonly` result) may be lent to it.
    // ES: Congelamiento P3: un parametro `in` solo lee su argumento, asi que se le puede prestar
    //     un lugar readonly (otro parametro `in`, un campo readonly, un resultado `ref readonly`).
    const bool parameterPointeeReadonly =
        parameter.pointerPointeeReadonly || parameter.passKind == ParameterPassKind::In;
    // EN: T08-D17: a `T**` argument does not bind to a `T const**` parameter.
    // ES: T08-D17: un argumento `T**` no se enlaza a un parametro `T const**`.
    const bool discardsPointeeConst = !packElement &&
        ((arguments[argumentOrdinal].pointerPointeeReadonly &&
          ProjectTypeConversion::isPointer(parameter.typeKey) && !parameterPointeeReadonly) ||
         method_body_detail::addsNestedPointeeConst(arguments[argumentOrdinal].typeKey,
             arguments[argumentOrdinal].pointerPointeeReadonly, parameter.typeKey, parameter.pointerPointeeReadonly) ||
         // C1-04: a `T**` argument does not bind to a `T volatile**` parameter. / Un argumento `T**` no se enlaza
         //     a un parametro `T volatile**`.
         method_body_detail::nestedPointerQualificationError(arguments[argumentOrdinal].typeKey,
             arguments[argumentOrdinal].pointerLevelQualifiers, arguments[argumentOrdinal].pointerPointeeReadonly,
             arguments[argumentOrdinal].pointerPointeeVolatile, parameter.typeKey, parameter.pointerLevelQualifiers,
             parameter.pointerPointeeReadonly, parameter.pointerPointeeVolatile) != nullptr);
    // C1-02 (item 4.7): a decayed fixed array keeps its element qualifiers. / Un arreglo fijo que decae conserva
    //     los calificadores de sus elementos.
    const bool decayDropsQualifier = conversion.exists() &&
        conversion.kind == ProjectConversionKind::CFlexibleArrayToPointer &&
        ProjectMemberModel::fixedArrayElementTypeKey(arguments[argumentOrdinal].typeKey).has_value() &&
        method_body_detail::decayedFixedArrayDropsQualifier(conversionTarget,
            arguments[argumentOrdinal].readonlyReceiver, arguments[argumentOrdinal].volatileStorage,
            arguments[argumentOrdinal].pointerLevelQualifiers, arguments[argumentOrdinal].pointerPointeeReadonly,
            arguments[argumentOrdinal].pointerPointeeVolatile, parameter.typeKey, parameter.pointerLevelQualifiers,
            parameterPointeeReadonly, parameter.pointerPointeeVolatile);
    const bool violatesPointerContract = !packElement &&
        ProjectTypeConversion::isPointer(parameter.typeKey) &&
        !language::pointerContractImplicitlyAssignable(
            pointerContract(
                // Item 8.3: a string literal is never null. / Un literal de cadena nunca es nulo.
                arguments[argumentOrdinal].pointerNullable && arguments[argumentOrdinal].typeKey != "string-literal",
                arguments[argumentOrdinal].pointerPointeeReadonly,
                arguments[argumentOrdinal].pointerHasProvenance,
                arguments[argumentOrdinal].pointerKnownAlignment,
                arguments[argumentOrdinal].pointerAddressSpace),
            pointerContract(
                parameter.pointerNullable,
                parameterPointeeReadonly,
                parameter.pointerHasProvenance,
                parameter.pointerKnownAlignment,
                parameter.pointerAddressSpace));
    const bool implicitExtensionReceiverPass = hasForwardedReceiver && argumentOrdinal == 0;
    const bool boundLvalue = rvalueArgument && conversion.exists() &&
        conversion.kind == ProjectConversionKind::Identity && arguments[argumentOrdinal].addressable;
    if (!conversion.exists() || (!rvalueArgument && (discardsPointeeConst || violatesPointerContract)) ||
        boundLvalue || decayDropsQualifier ||
        (arguments[argumentOrdinal].pointerPointeeVolatile &&
         (packElement || !parameter.pointerPointeeVolatile)) ||
        // T19-002: a pointer to C atomic storage binds only an `_Atomic T*` parameter, and only that parameter
        //     takes one, as in C. / Un puntero a almacenamiento C atomico solo enlaza un parametro `_Atomic T*`,
        //     y solo ese parametro lo admite, como en C.
        (ProjectTypeConversion::isPointer(parameter.typeKey) && parameter.typeKey != "builtin:void*" &&
         arguments[argumentOrdinal].typeKey != "builtin:null" &&
         arguments[argumentOrdinal].cppPointerPointeeAtomic != parameter.cppPointerPointeeAtomic) ||
        (packElement && arguments[argumentOrdinal].cppPointerPointeeAtomic) ||
        // T19-038: no parameter is a pointer to atomic storage holding a pointer to volatile data. / Ningun
        //     parametro es un puntero a almacenamiento atomico que guarda un puntero a dato volatile.
        arguments[argumentOrdinal].cppAtomicPointeeHoldsVolatile ||
        (!implicitExtensionReceiverPass && !rvalueArgument &&
         parameter.passKind != arguments[argumentOrdinal].argumentPassKind))
    {
        evaluated.viable = false;
    }
    else if (packElement &&
        (arguments[argumentOrdinal].ownership != sema::OwnershipKind::None ||
         arguments[argumentOrdinal].ownershipTransfer ||
         arguments[argumentOrdinal].ownershipFreshOwner))
    {
        // The temporary span owns no elements and cannot extend an owner lifetime.
        // Move-only and ownership-qualified values must be passed in a prebuilt span.
        evaluated.viable = false;
        evaluated.invalidPackOwnershipArgumentOrdinal = argumentOrdinal;
    }
    else if (!implicitExtensionReceiverPass && conversion.exists() &&
        parameter.passKind == ParameterPassKind::Value &&
        memberModel_->typeIsMoveOnly(conversionTarget) &&
        arguments[argumentOrdinal].addressable &&
        !arguments[argumentOrdinal].ownershipTransfer)
    {
        evaluated.viable = false;
        evaluated.invalidMoveOnlyArgumentOrdinal = argumentOrdinal;
    }
    validateNamespaceFunctionCandidateArgumentOwnership(
        parameter, arguments, argumentOrdinal, packElement, evaluated);
    return conversion;
}

MethodCallCandidate ProjectMethodBodyModel::evaluateNamespaceFunctionCandidate(
    std::size_t functionOrdinal,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::span<const ExpressionResult> arguments,
    bool hasForwardedReceiver,
    bool& matchingArity) const
{
    const ClassMemberSymbol& candidate = namespaceCallableAdapters()[functionOrdinal];
    MethodCallCandidate evaluated;
    evaluated.memberOrdinal = functionOrdinal;
    evaluated.isVariadicCandidate = candidate.isVariadic;
    const NamedArgumentMap mapping = mapArgumentsToParameters(argumentSyntaxes, candidate);
    if (!mapping.valid) return evaluated;
    matchingArity = true;
    evaluated.viable = true;
    evaluated.argumentParameterOrdinals = mapping.explicitParameterOrdinals;
    evaluated.defaultedParameterOrdinals = mapping.defaultedParameterOrdinals;
    evaluated.parameterPackElements = mapping.parameterPackElements;
    evaluated.parameterPackOrdinal = mapping.parameterPackOrdinal;
    evaluated.usesExpandedParameterPack = mapping.usesExpandedParameterPack;
    evaluated.synthesizesEmptyParameterPack = mapping.synthesizesEmptyParameterPack;
    normalizeNamespaceFunctionParameterPack(candidate, arguments, evaluated);
    evaluated.defaultedParameterCount = mapping.defaultedParameterOrdinals.size();
    for (std::size_t argumentOrdinal = 0; argumentOrdinal < arguments.size(); ++argumentOrdinal)
    {
        evaluated.conversions.push_back(evaluateNamespaceFunctionCandidateArgument(
            candidate, arguments, argumentOrdinal, hasForwardedReceiver, evaluated));
    }
    return evaluated;
}

void ProjectMethodBodyModel::evaluateNamespaceFunctionCandidates(
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::span<const ExpressionResult> arguments,
    bool hasForwardedReceiver,
    NamespaceFunctionCallBinding& call,
    bool& matchingArity) const
{
    for (const std::size_t functionOrdinal : reference.candidateFunctionOrdinals)
    {
        if (functionOrdinal >= namespaceCallableAdapters().size()) continue;
        MethodCallCandidate evaluated = evaluateNamespaceFunctionCandidate(
            functionOrdinal, argumentSyntaxes, arguments, hasForwardedReceiver, matchingArity);
        if (evaluated.viable)
        {
            call.viableCandidateFunctionOrdinals.push_back(functionOrdinal);
        }
        call.evaluatedCandidates.push_back(std::move(evaluated));
    }
}

void ProjectMethodBodyModel::reportNoViableNamespaceFunctionCall(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    const NamespaceFunctionCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const auto packOwnership = std::ranges::find_if(call.evaluatedCandidates,
        [](const MethodCallCandidate& candidate)
        {
            return candidate.invalidPackOwnershipArgumentOrdinal.has_value();
        });
    if (packOwnership != call.evaluatedCandidates.end())
    {
        const std::size_t ordinal = *packOwnership->invalidPackOwnershipArgumentOrdinal;
        const ast::AstNode* argument = ordinal < argumentSyntaxes.size()
            ? argumentSyntaxes[ordinal] : expression;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            argument != nullptr ? argument->range() : expression->range(),
            "an expanded params span only borrows its elements for the call; pass a non-owning reference, "
            "or build a Span/ReadOnlySpan over the owners and pass it as the params argument");
        return;
    }
    for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
    {
        if (!candidate.invalidMoveOnlyArgumentOrdinal.has_value()) continue;
        const std::size_t ordinal = *candidate.invalidMoveOnlyArgumentOrdinal;
        const ast::AstNode* argument = ordinal < argumentSyntaxes.size()
            ? argumentSyntaxes[ordinal] : expression;
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            argument != nullptr ? argument->range() : expression->range(),
            "move-only argument cannot be copied into a by-value namespace-function parameter; use move(argument)");
        return;
    }
    diagnostics.reportStructured(
        diagnostics::DiagnosticId::InvalidConversion,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "no viable overload of namespace function '" + reference.writtenName +
            "' accepts the argument types",
        {}, namespaceFunctionCandidateNotes(namespaceFunctionModel_, reference.candidateFunctionOrdinals,
                                            call.evaluatedCandidates, true));
}

std::vector<std::size_t> ProjectMethodBodyModel::viableNamespaceFunctionCandidates(
    const NamespaceFunctionCallBinding& call) const
{
    std::vector<std::size_t> viable;
    for (std::size_t index = 0; index < call.evaluatedCandidates.size(); ++index)
    {
        if (call.evaluatedCandidates[index].viable) viable.push_back(index);
    }
    return viable;
}

std::vector<std::size_t> ProjectMethodBodyModel::nonDominatedNamespaceFunctionCandidates(
    const NamespaceFunctionCallBinding& call,
    std::span<const std::size_t> viable) const
{
    std::vector<std::size_t> nonDominated;
    const auto betterNamespaceCandidate = [&](const MethodCallCandidate& left, const MethodCallCandidate& right)
    {
        if (betterCandidate(left, right)) return true;
        if (betterCandidate(right, left) || namespaceFunctionModel_ == nullptr ||
            left.memberOrdinal >= namespaceFunctionModel_->functions().size() ||
            right.memberOrdinal >= namespaceFunctionModel_->functions().size() ||
            left.conversions.size() != 1 || right.conversions.size() != 1 ||
            left.argumentParameterOrdinals.size() != 1 || right.argumentParameterOrdinals.size() != 1 ||
            left.defaultedParameterCount != 0 || right.defaultedParameterCount != 0 ||
            left.isVariadicCandidate || right.isVariadicCandidate ||
            left.usesExpandedParameterPack || right.usesExpandedParameterPack)
            return false;
        const auto& leftFunction = namespaceFunctionModel_->functions()[left.memberOrdinal];
        const auto& rightFunction = namespaceFunctionModel_->functions()[right.memberOrdinal];
        if (leftFunction.cppCanonicalIdentity.empty() || rightFunction.cppCanonicalIdentity.empty() ||
            leftFunction.parameters.size() != 1 || rightFunction.parameters.size() != 1 ||
            left.argumentParameterOrdinals[0] != 0 || right.argumentParameterOrdinals[0] != 0)
            return false;
        const auto& lc = left.conversions[0];
        const auto& rc = right.conversions[0];
        return lc.rank == rc.rank && lc.sourceTypeKey == rc.sourceTypeKey &&
            lc.sourceTypeKey == lc.targetTypeKey && lc.targetTypeKey == rc.targetTypeKey &&
            ProjectTypeConversion::isPointer(lc.sourceTypeKey) &&
            !leftFunction.parameters[0].pointerPointeeReadonly &&
            rightFunction.parameters[0].pointerPointeeReadonly;
    };
    for (const std::size_t candidateIndex : viable)
    {
        const bool dominated = std::ranges::any_of(nonDominated, [&](std::size_t other)
        {
            return betterNamespaceCandidate(
                call.evaluatedCandidates[other], call.evaluatedCandidates[candidateIndex]);
        });
        if (dominated) continue;
        std::erase_if(nonDominated, [&](std::size_t other)
        {
            return betterNamespaceCandidate(
                call.evaluatedCandidates[candidateIndex], call.evaluatedCandidates[other]);
        });
        nonDominated.push_back(candidateIndex);
    }
    return nonDominated;
}

std::optional<std::size_t> ProjectMethodBodyModel::selectNamespaceFunctionCandidate(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::size_t explicitArgumentCount,
    bool matchingArity,
    NamespaceFunctionCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!matchingArity)
    {
        diagnostics.reportStructured(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "no overload of namespace function '" + reference.writtenName + "' accepts " +
                std::to_string(explicitArgumentCount) + " argument(s)",
            {}, namespaceFunctionCandidateNotes(namespaceFunctionModel_, reference.candidateFunctionOrdinals,
                                                call.evaluatedCandidates, true));
        storeNamespaceFunctionCall(expression, call);
        return std::nullopt;
    }
    const std::vector<std::size_t> viable = viableNamespaceFunctionCandidates(call);
    if (viable.empty())
    {
        reportNoViableNamespaceFunctionCall(
            expression, reference, argumentSyntaxes, call, diagnostics);
        storeNamespaceFunctionCall(expression, call);
        return std::nullopt;
    }
    // EN: T19-006. When the NativeSharp ranking ties, C++ overloads whose arithmetic parameters differ are ranked
    //     by the C++ conversion of each argument, as Clang ranks them: `g(int)` beats `g(long)` for an int
    //     argument, while `m(int, long)` against `m(long, int)` stays ambiguous. A call the NativeSharp ranking
    //     already resolves keeps its choice (T10-009).
    // ES: T19-006. Si el orden NativeSharp empata, las sobrecargas C++ cuyos parametros aritmeticos difieren se
    //     ordenan por la conversion C++ de cada argumento, como Clang: `g(int)` gana a `g(long)` con un argumento
    //     int, y `m(int, long)` frente a `m(long, int)` sigue ambigua. Una llamada que el orden NativeSharp ya
    //     resuelve conserva su eleccion (T10-009).
    const std::vector<std::size_t> nativeSharpBest = nonDominatedNamespaceFunctionCandidates(call, viable);
    const CppArithmeticOverloadRanking cppRanking = nativeSharpBest.size() > 1
        ? rankCppArithmeticOverloads(namespaceFunctionModel_, call.evaluatedCandidates, nativeSharpBest)
        : CppArithmeticOverloadRanking{};
    std::vector<std::size_t> nonDominated = cppRanking.applies ? cppRanking.best : nativeSharpBest;
    // EN: T19-007. `ref x` names an lvalue, so where it binds a C++ `T&` in one overload and a `T&&` in another, the
    //     `T&` overload is better, as C++ prefers it for an lvalue ([over.ics.rank]).
    // ES: T19-007. `ref x` nombra un lvalue, asi que donde enlaza un `T&` C++ en una sobrecarga y un `T&&` en otra,
    //     la de `T&` es mejor, como C++ la prefiere para un lvalue ([over.ics.rank]).
    if (nonDominated.size() > 1 && namespaceFunctionModel_ != nullptr)
    {
        const auto rvalueBindings = [&](std::size_t index) {
            const MethodCallCandidate& candidate = call.evaluatedCandidates[index];
            if (candidate.memberOrdinal >= namespaceFunctionModel_->functions().size()) return std::size_t{0};
            const auto& parameters = namespaceFunctionModel_->functions()[candidate.memberOrdinal].parameters;
            std::size_t count = 0;
            for (std::size_t argument = 0; argument < candidate.argumentParameterOrdinals.size(); ++argument)
            {
                const std::size_t ordinal = candidate.argumentParameterOrdinals[argument];
                if (ordinal < parameters.size() && parameters[ordinal].cppRvalueReference &&
                    argument < call.evaluatedCandidates[index].conversions.size() &&
                    call.evaluatedCandidates[index].conversions[argument].targetTypeKey == parameters[ordinal].typeKey)
                    ++count;
            }
            return count;
        };
        std::vector<std::size_t> lvalueBound;
        for (const std::size_t index : nonDominated)
            if (rvalueBindings(index) == 0) lvalueBound.push_back(index);
        if (!lvalueBound.empty() && lvalueBound.size() < nonDominated.size()) nonDominated = std::move(lvalueBound);
    }
    if (nonDominated.size() == 1) return nonDominated.front();
    // EN: T13-005. As in C++, when the conversions tie, a function that is not a template specialization is
    //     better than a C++ function template specialization.
    // ES: T13-005. Como en C++, si las conversiones empatan, una funcion que no es especializacion de plantilla
    //     es mejor que una especializacion de plantilla de funcion C++.
    if (namespaceFunctionModel_ != nullptr)
    {
        std::vector<std::size_t> nonTemplates;
        bool anyTemplate = false;
        for (const std::size_t index : nonDominated)
        {
            const std::size_t ordinal = call.evaluatedCandidates[index].memberOrdinal;
            const bool specialization = ordinal < namespaceFunctionModel_->functions().size() &&
                namespaceFunctionModel_->functions()[ordinal].cppTemplateSpecialization;
            anyTemplate = anyTemplate || specialization;
            if (!specialization) nonTemplates.push_back(index);
        }
        if (anyTemplate && nonTemplates.size() == 1) return nonTemplates.front();
    }
    std::vector<std::size_t> tied;
    for (const std::size_t index : nonDominated) tied.push_back(call.evaluatedCandidates[index].memberOrdinal);
    diagnostics.reportStructured(
        diagnostics::DiagnosticId::InvalidCall,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "ambiguous namespace function call to '" + reference.writtenName + "'",
        {}, namespaceFunctionCandidateNotes(namespaceFunctionModel_, tied, call.evaluatedCandidates, false));
    call.resolution = MethodCallResolutionKind::Ambiguous;
    storeNamespaceFunctionCall(expression, call);
    return std::nullopt;
}

// EN: C3-09. C11's `atomic_thread_fence` and `atomic_signal_fence` (`void f(memory_order)` with C linkage, declared by
//     <stdatomic.h>) are functions no library defines: C reaches them through macros that expand to Clang builtins.
//     A call from NativeSharp becomes the fence of NativeSharp.Core.Threading.AtomicFence (Thread or Compiler) with the
//     same order: `consume` is `acquire`, as compilers implement it, and a `relaxed` fence (a no-op in C) is a
//     compiler-only fence, which is conforming. The order must be a constant, as AtomicFence requires; other system
//     macros stay as they are.
// ES: C3-09. `atomic_thread_fence` y `atomic_signal_fence` de C11 (`void f(memory_order)` con enlace C, declaradas por
//     <stdatomic.h>) son funciones que ninguna biblioteca define: C las alcanza por macros que expanden a builtins de
//     Clang. Una llamada desde NativeSharp pasa a ser la barrera de NativeSharp.Core.Threading.AtomicFence (Thread o
//     Compiler) con el mismo orden: `consume` es `acquire`, como lo implementan los compiladores, y una barrera
//     `relaxed` (sin efecto en C) es una barrera solo del compilador, lo que es conforme. El orden debe ser constante,
//     como lo exige AtomicFence; las demas macros del sistema quedan como estan.
bool ProjectMethodBodyModel::recognizeCAtomicFence(
    const ast::AstNode* expression,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    std::size_t functionOrdinal,
    std::span<ast::AstNode* const> argumentSyntaxes,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (namespaceFunctionModel_ == nullptr || functionOrdinal >= namespaceFunctionModel_->functions().size())
        return true;
    const NamespaceFunctionSymbol& function = namespaceFunctionModel_->functions()[functionOrdinal];
    const bool thread = function.name == "atomic_thread_fence";
    if ((!thread && function.name != "atomic_signal_fence") || function.cppCanonicalIdentity.empty() ||
        function.definitionSyntax != nullptr || function.declaredTypeKey != "builtin:void" ||
        function.cppParameterTypes.size() != 1U || argumentSyntaxes.size() != 1U ||
        function.cppParameterTypes[0].kind != interop::CppTypeKind::Enumeration ||
        !function.cppParameterTypes[0].enumIdentity || function.cppParameterTypes[0].enumIdentity->name != "memory_order")
        return true;
    const std::optional<ProjectIntegerValue> folded =
        foldIntegerConstant(argumentSyntaxes[0], std::nullopt, callerCallableOrdinal);
    if (!folded.has_value() || folded->bits > 5U)
    {
        diagnostics.report(diagnostics::DiagnosticId::ConstRequiresConstant, diagnostics::DiagnosticSeverity::Error,
            argumentSyntaxes[0] != nullptr ? argumentSyntaxes[0]->range() : expression->range(),
            "the order of C '" + function.name + "' must be a constant memory_order (it becomes "
            "NativeSharp.Core.Threading.AtomicFence)");
        return false;
    }
    // C: relaxed 0, consume 1, acquire 2, release 3, acq_rel 4, seq_cst 5.
    static constexpr language::MemoryOrder orders[] = {
        language::MemoryOrder::Acquire, language::MemoryOrder::Acquire, language::MemoryOrder::Acquire,
        language::MemoryOrder::Release, language::MemoryOrder::AcquireRelease,
        language::MemoryOrder::SequentiallyConsistent};
    const bool relaxed = folded->bits == 0U;
    MethodCoreBinding binding{
        thread && !relaxed ? MethodCoreOperationKind::AtomicThreadFence : MethodCoreOperationKind::AtomicCompilerFence,
        expression, callerCallableOrdinal, scopeOrdinal, "builtin:void", "", false, std::nullopt, std::nullopt};
    binding.atomicMemoryOrder = static_cast<std::uint8_t>(orders[folded->bits]);
    coreBindings_.push_back(std::move(binding));
    return true;
}

bool ProjectMethodBodyModel::validateNamespaceFunctionDefinition(
    const ast::AstNode* expression,
    std::size_t functionOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (namespaceFunctionModel_ == nullptr ||
        functionOrdinal >= namespaceFunctionModel_->functions().size())
    {
        return true;
    }
    const NamespaceFunctionSymbol& function =
        namespaceFunctionModel_->functions()[functionOrdinal];
    if (function.definitionSyntax != nullptr || function.isExternC || function.isPrecompiled ||
        !function.cppCanonicalIdentity.empty())
    {
        return true;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::MissingFunctionDefinition,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "namespace function '" + function.signatureKey +
            "' is called but has no definition in the project");
    return false;
}

void ProjectMethodBodyModel::checkNamespaceFunctionArgumentPassKinds(
    const ast::AstNode* expression,
    const ClassMemberSymbol& callable,
    std::size_t functionOrdinal,
    const std::vector<ExpressionResult>& arguments,
    const MethodCallCandidate& selected,
    bool hasForwardedReceiver,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!hasForwardedReceiver)
    {
        checkArgumentPassKinds(
            expression, callable.name, namespaceCallableBase_ + functionOrdinal,
            arguments, diagnostics, selected.argumentParameterOrdinals);
        return;
    }
    std::vector<ExpressionResult> explicitArguments(arguments.begin() + 1, arguments.end());
    const std::span<const std::size_t> parameterOrdinals(
        selected.argumentParameterOrdinals.data() + 1,
        selected.argumentParameterOrdinals.size() - 1);
    checkArgumentPassKinds(
        expression, callable.name, namespaceCallableBase_ + functionOrdinal,
        explicitArguments, diagnostics, parameterOrdinals);
}

void ProjectMethodBodyModel::recordNamespaceFunctionBorrows(
    const ast::AstNode* expression,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    std::size_t functionOrdinal,
    std::span<ast::AstNode* const> argumentSyntaxes,
    const std::vector<ExpressionResult>& arguments,
    const MethodCallCandidate& selected)
{
    for (const std::size_t argumentOrdinal : selected.fullExpressionBorrowArgumentOrdinals)
    {
        if (argumentOrdinal >= arguments.size() ||
            argumentOrdinal >= selected.argumentParameterOrdinals.size())
        {
            continue;
        }
        MethodBorrowBinding borrow;
        borrow.kind = MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter;
        borrow.syntax = argumentOrdinal < argumentSyntaxes.size()
            ? argumentSyntaxes[argumentOrdinal] : expression;
        borrow.methodMemberOrdinal = callerCallableOrdinal;
        borrow.scopeOrdinal = scopeOrdinal;
        borrow.targetMemberOrdinal = namespaceCallableBase_ + functionOrdinal;
        borrow.targetParameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        borrow.typeKey = arguments[argumentOrdinal].typeKey;
        borrowBindings_.push_back(std::move(borrow));
    }
}

// EN: Freeze C-01 (plan item 115): `F(new T())` where the parameter of F is not `owned` gives the object
//     no owner: F only observes it, and destroying it after the call would free what F may have kept.
// ES: Congelamiento C-01 (elemento 115 del plan): `F(new T())` con un parámetro de F sin `owned` deja al
//     objeto sin dueño: F sólo lo observa, y destruirlo después de la llamada liberaría lo que F pudo guardar.
void ProjectMethodBodyModel::rejectNamespaceNewToOrdinaryParameters(
    const ClassMemberSymbol& callable,
    std::span<ast::AstNode* const> argumentSyntaxes,
    const std::vector<ExpressionResult>& arguments,
    const MethodCallCandidate& selected,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < arguments.size() && argumentOrdinal < argumentSyntaxes.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= callable.parameters.size()) continue;
        const ExpressionResult& argument = arguments[argumentOrdinal];
        if (newReachesOrdinaryParameter(
                callable.parameters[parameterOrdinal], argumentSyntaxes[argumentOrdinal],
                argument.ownershipFreshOwner, argument.ownershipTransfer, argument.typeKey))
            reportNewToOrdinaryParameter(
                argumentSyntaxes[argumentOrdinal], callable.parameters[parameterOrdinal], callable.name, diagnostics);
    }
}

void ProjectMethodBodyModel::recordNamespaceFunctionDefaults(
    const ClassMemberSymbol& callable,
    const MethodCallCandidate& selected,
    NamespaceFunctionCallBinding& call) const
{
    for (const std::size_t parameterOrdinal : selected.defaultedParameterOrdinals)
    {
        call.selectedDefaultArguments.push_back(
            callable.parameters[parameterOrdinal].defaultValueSyntax);
        call.selectedDefaultArgumentParameterOrdinals.push_back(parameterOrdinal);
    }
}

void ProjectMethodBodyModel::recordViewOutputOrigins(
    std::span<const ast::AstNode* const> argumentSyntaxes,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    const ClassMemberSymbol& selectedMember,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    const ast::AstNode* receiverSyntax)
{
    // A mutable view argument may receive any view supplied by value in the
    // same call. Preserve each known owner on the destination local; escape
    // summaries currently do not encode a relation between two parameters.
    MethodLocalSymbol viewInputs;
    const ast::AstNode* receiverRoot = receiverSyntax;
    while (receiverRoot != nullptr && (receiverRoot->kind() == ast::AstNodeKind::MemberExpression ||
                                       receiverRoot->kind() == ast::AstNodeKind::IndexExpression))
        receiverRoot = receiverRoot->first();
    if (receiverRoot != nullptr && receiverRoot->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const auto root = lookupScopeName(scopeOrdinal, receiverRoot->text());
        if (root.has_value() && (root->kind == ScopeNameKind::Local || root->kind == ScopeNameKind::Parameter))
        {
            viewInputs.viewSourceFields.push_back("@" + receiverRoot->text());
            viewInputs.viewSourceIdentities.push_back({receiverRoot->text(), ViewSourceIdentityKind::Unknown, 0});
        }
    }
    bool unknownViewOutputTarget = false;
    for (std::size_t i = 0; i < resolvedArguments.size() &&
         i < selected.argumentParameterOrdinals.size() && i < argumentSyntaxes.size(); ++i)
    {
        const std::size_t p = selected.argumentParameterOrdinals[i];
        if (p >= selectedMember.parameters.size()) continue;
        const MethodParameterSymbol& parameter = selectedMember.parameters[p];
        // A reference argument can be the owner of a view written to an
        // out/ref parameter, whether the parameter is explicitly borrowed or
        // unqualified. Without a parameter-to-output summary, keep every such
        // owner as a possible source instead of letting the view outlive it.
        if (parameter.passKind == ParameterPassKind::Value &&
            parameter.ownership != sema::OwnershipKind::Owned &&
            isReferenceTypeKey(resolvedArguments[i].typeKey))
        {
            collectViewSourceFields(viewInputs, argumentSyntaxes[i], scopeOrdinal);
            const auto addOwner = [&](std::string_view name)
            {
                const auto root = lookupScopeName(scopeOrdinal, name);
                if (root.has_value() && (root->kind == ScopeNameKind::Local || root->kind == ScopeNameKind::Parameter))
                {
                    const std::string source = "@" + std::string(name);
                    if (std::ranges::find(viewInputs.viewSourceFields, source) == viewInputs.viewSourceFields.end())
                        viewInputs.viewSourceFields.push_back(source);
                    const ViewSourceIdentity identity{std::string(name), ViewSourceIdentityKind::Unknown, 0};
                    if (std::ranges::find(viewInputs.viewSourceIdentities, identity) == viewInputs.viewSourceIdentities.end())
                        viewInputs.viewSourceIdentities.push_back(identity);
                }
            };
            if (resolvedArguments[i].ownershipSourceLocalOrdinal.has_value() &&
                *resolvedArguments[i].ownershipSourceLocalOrdinal < locals_.size())
                addOwner(locals_[*resolvedArguments[i].ownershipSourceLocalOrdinal].name);
            const std::size_t callerOrdinal = scopeOrdinal < scopes_.size()
                ? scopes_[scopeOrdinal].methodMemberOrdinal : std::numeric_limits<std::size_t>::max();
            if (const ClassMemberSymbol* caller = methodSymbol(callerOrdinal);
                caller != nullptr && resolvedArguments[i].ownershipSourceParameterOrdinal.has_value() &&
                *resolvedArguments[i].ownershipSourceParameterOrdinal < caller->parameters.size())
                addOwner(caller->parameters[*resolvedArguments[i].ownershipSourceParameterOrdinal].name);
            std::vector<const ast::AstNode*> pending{argumentSyntaxes[i]};
            while (!pending.empty())
            {
                const ast::AstNode* node = pending.back(); pending.pop_back();
                if (node == nullptr) continue;
                if (node->kind() == ast::AstNodeKind::IdentifierExpression)
                {
                    const MethodNameBinding* binding = nameBinding(node, callerOrdinal);
                    if (binding != nullptr &&
                        (binding->kind == MethodNameBindingKind::Local || binding->kind == MethodNameBindingKind::Parameter) &&
                        isReferenceTypeKey(binding->typeKey)) addOwner(node->text());
                    continue;
                }
                if (node->kind() == ast::AstNodeKind::MemberExpression ||
                    node->kind() == ast::AstNodeKind::IndexExpression ||
                    node->kind() == ast::AstNodeKind::UnaryExpression)
                    pending.push_back(node->first());
                else if (node->kind() == ast::AstNodeKind::ConditionalExpression)
                {
                    pending.push_back(node->second());
                    pending.push_back(node->third());
                }
                else if (node->kind() == ast::AstNodeKind::CallExpression)
                {
                    if (const MethodCallBinding* call = callBinding(node, callerOrdinal);
                        call != nullptr && !call->staticCall) pending.push_back(call->receiverSyntax);
                    for (const ast::AstNode* child : node->children()) pending.push_back(child);
                }
            }
        }
        if ((parameter.passKind == ParameterPassKind::Ref || parameter.passKind == ParameterPassKind::In) &&
            (typeKeyIsViewLike(parameter.pointeeTypeKey) || tupleTypeKeyContainsView(parameter.pointeeTypeKey)))
        {
            const ast::AstNode* argument = argumentSyntaxes[i];
            if (argument != nullptr && argument->kind() == ast::AstNodeKind::UnaryExpression)
                collectViewSourceFields(viewInputs, argument->first(), scopeOrdinal);
        }
        if ((parameter.passKind == ParameterPassKind::Ref || parameter.passKind == ParameterPassKind::In) &&
            memberModel_ != nullptr && memberModel_->typeIsMoveOnly(parameter.pointeeTypeKey))
        {
            const ast::AstNode* argument = argumentSyntaxes[i];
            const ast::AstNode* owner = argument != nullptr &&
                argument->kind() == ast::AstNodeKind::UnaryExpression ? argument->first() : nullptr;
            while (owner != nullptr && (owner->kind() == ast::AstNodeKind::MemberExpression ||
                                        owner->kind() == ast::AstNodeKind::IndexExpression))
                owner = owner->first();
            if (owner != nullptr && owner->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const auto root = lookupScopeName(scopeOrdinal, owner->text());
                if (root.has_value() && (root->kind == ScopeNameKind::Local || root->kind == ScopeNameKind::Parameter))
                {
                    const std::string source = "@" + owner->text();
                    viewInputs.viewSourceIdentities.push_back({owner->text(), ViewSourceIdentityKind::Unknown, 0});
                    if (std::ranges::find(viewInputs.viewSourceFields, source) == viewInputs.viewSourceFields.end())
                        viewInputs.viewSourceFields.push_back(source);
                }
            }
        }
        if (parameter.passKind != ParameterPassKind::Value ||
            (!typeKeyIsViewLike(resolvedArguments[i].typeKey) &&
             !tupleTypeKeyContainsView(resolvedArguments[i].typeKey))) continue;
        collectViewSourceFields(viewInputs,
            argumentSyntaxes[i], scopeOrdinal);
    }
    // Record uncertain call outputs without changing the existing diagnostics.
    const auto recordUnknownOutput = [&](MethodLocalSymbol& target, bool exactDestination)
    {
        // Preserve possible local roots with semantic ordinals. Never recover
        // an ordinal from an Unknown name or replace the Unknown observation.
        const std::size_t callable = scopeOrdinal < scopes_.size()
            ? scopes_[scopeOrdinal].methodMemberOrdinal : std::numeric_limits<std::size_t>::max();
        if (exactDestination && target.methodMemberOrdinal == callable)
            for (const ViewSourceIdentity& source : viewInputs.viewSourceIdentities)
                if (source.kind == ViewSourceIdentityKind::Local && source.ordinal < locals_.size() &&
                    locals_[source.ordinal].methodMemberOrdinal == callable &&
                    std::ranges::find(target.viewSourceIdentities, source) == target.viewSourceIdentities.end())
                    target.viewSourceIdentities.push_back(source);
        for (const ViewUnknownOrigin& source : viewInputs.viewUnknownOrigins)
            appendViewUnknownOrigin(target.viewUnknownOrigins, source);
        const ViewUnknownOrigin callOrigin{
            scopeOrdinal < scopes_.size() ? std::optional<std::size_t>(scopeOrdinal) : std::nullopt,
            locals_.size()};
        appendViewUnknownOrigin(target.viewUnknownOrigins, callOrigin);
    };
    for (std::size_t i = 0; i < resolvedArguments.size() &&
         i < selected.argumentParameterOrdinals.size() && i < argumentSyntaxes.size(); ++i)
    {
        const std::size_t p = selected.argumentParameterOrdinals[i];
        if (p >= selectedMember.parameters.size()) continue;
        const MethodParameterSymbol& parameter = selectedMember.parameters[p];
        if ((parameter.passKind != ParameterPassKind::Ref && parameter.passKind != ParameterPassKind::Out) ||
            (!typeKeyIsViewLike(parameter.pointeeTypeKey) &&
             !tupleTypeKeyContainsView(parameter.pointeeTypeKey))) continue;
        const ast::AstNode* argument = argumentSyntaxes[i];
        const ast::AstNode* destination = argument != nullptr &&
            argument->kind() == ast::AstNodeKind::UnaryExpression ? argument->first() : nullptr;
        if (destination == nullptr || destination->kind() != ast::AstNodeKind::IdentifierExpression)
        {
            unknownViewOutputTarget = true;
            continue;
        }
        const auto name = lookupScopeName(scopeOrdinal, destination->text());
        if (!name.has_value() || name->kind != ScopeNameKind::Local || name->semanticOrdinal >= locals_.size())
        {
            unknownViewOutputTarget = true;
            continue;
        }
        MethodLocalSymbol& target = locals_[name->semanticOrdinal];
        recordUnknownOutput(target, true);
        if (!target.isReference) continue;
        const auto& origin = target.referenceEscapeOrigin;
        if (origin.kind == MethodEscapeOriginKind::CurrentFrameLocal && origin.ordinal.has_value() &&
            *origin.ordinal < locals_.size() &&
            target.referencePlaceKey == "L" + std::to_string(*origin.ordinal))
        {
            recordUnknownOutput(locals_[*origin.ordinal], true);
            continue;
        }
        unknownViewOutputTarget = true;
    }
    if (unknownViewOutputTarget)
    {
        const std::size_t callable = scopeOrdinal < scopes_.size()
            ? scopes_[scopeOrdinal].methodMemberOrdinal : std::numeric_limits<std::size_t>::max();
        for (MethodLocalSymbol& possible : locals_)
            if (possible.methodMemberOrdinal == callable && scopeContains(possible.scopeOrdinal, scopeOrdinal) &&
                (typeKeyIsViewLike(possible.typeKey) || tupleTypeKeyContainsView(possible.typeKey)))
                recordUnknownOutput(possible, false);
    }
    for (std::size_t i = 0; i < resolvedArguments.size() &&
         i < selected.argumentParameterOrdinals.size() && i < argumentSyntaxes.size(); ++i)
    {
        const std::size_t p = selected.argumentParameterOrdinals[i];
        if (p >= selectedMember.parameters.size() ||
            (selectedMember.parameters[p].passKind != ParameterPassKind::Ref &&
             selectedMember.parameters[p].passKind != ParameterPassKind::Out) ||
            !typeKeyIsViewLike(selectedMember.parameters[p].pointeeTypeKey)) continue;
        const ast::AstNode* argument = argumentSyntaxes[i];
        const ast::AstNode* destination = argument != nullptr &&
            argument->kind() == ast::AstNodeKind::UnaryExpression &&
            (argument->auxiliaryText() == "pass:ref" || argument->auxiliaryText() == "pass:out")
            ? argument->first() : nullptr;
        if (destination == nullptr) continue;
        const std::size_t callerOrdinal = scopeOrdinal < scopes_.size()
            ? scopes_[scopeOrdinal].methodMemberOrdinal : std::numeric_limits<std::size_t>::max();
        const auto isStructType = [&](std::string_view typeKey)
        {
            if (nameLookup_ == nullptr) return false;
            const auto type = namedTypeOrdinalForTypeKey(typeKey);
            return type.has_value() && *type < nameLookup_->namedTypes().size() &&
                nameLookup_->namedTypes()[*type].kind == NamedTypeDeclarationKind::Struct;
        };
        const auto valueFieldRoot = [&](const ast::AstNode* field) -> std::optional<std::size_t>
        {
            bool sawField = false;
            while (field != nullptr && field->kind() == ast::AstNodeKind::MemberExpression)
            {
                const MethodNameBinding* binding = nameBinding(field, callerOrdinal);
                if (binding == nullptr || binding->kind != MethodNameBindingKind::ExplicitObjectField ||
                    memberModel_ == nullptr || binding->candidateMemberOrdinals.size() != 1U ||
                    binding->candidateMemberOrdinals.front() >= memberModel_->members().size() ||
                    memberModel_->members()[binding->candidateMemberOrdinals.front()].kind != ClassMemberKind::Field ||
                    memberModel_->members()[binding->candidateMemberOrdinals.front()].isReferenceReturn ||
                    field->first() == nullptr ||
                    !isStructType(recordedExpressionTypeKey(field->first(), callerOrdinal))) return std::nullopt;
                sawField = true;
                field = field->first();
            }
            if (!sawField || field == nullptr || field->kind() != ast::AstNodeKind::IdentifierExpression)
                return std::nullopt;
            const MethodNameBinding* root = nameBinding(field, callerOrdinal);
            if (root == nullptr || root->kind != MethodNameBindingKind::Local || !root->localOrdinal.has_value() ||
                *root->localOrdinal >= locals_.size()) return std::nullopt;
            const MethodLocalSymbol& local = locals_[*root->localOrdinal];
            if (local.isReference || local.isStatic || local.methodMemberOrdinal != callerOrdinal ||
                !isStructType(local.typeKey)) return std::nullopt;
            return *root->localOrdinal;
        };
        const ast::AstNode* targetSyntax = destination;
        std::optional<std::size_t> projectedRoot;
        const auto hasReferenceField = [&](const ast::AstNode* field)
        {
            for (; field != nullptr && field->kind() == ast::AstNodeKind::MemberExpression;
                 field = field->first())
            {
                const MethodNameBinding* binding = nameBinding(field, callerOrdinal);
                if (binding != nullptr && binding->kind == MethodNameBindingKind::ExplicitObjectField &&
                    memberModel_ != nullptr && binding->candidateMemberOrdinals.size() == 1U &&
                    binding->candidateMemberOrdinals.front() < memberModel_->members().size() &&
                    memberModel_->members()[binding->candidateMemberOrdinals.front()].isReferenceReturn)
                    return true;
            }
            return false;
        };
        const auto rejectUnknownReferenceFieldOutput = [&]()
        {
            const ClassMemberSymbol* callable = methodSymbol(callerOrdinal);
            for (const std::string& source : viewInputs.viewSourceFields)
            {
                if (source.size() <= 1 || source.front() != '@') continue;
                const auto owner = lookupScopeName(scopeOrdinal, std::string_view(source).substr(1));
                if (!owner.has_value()) continue;
                const bool localOwner = owner->kind == ScopeNameKind::Local &&
                    owner->semanticOrdinal < locals_.size() && !locals_[owner->semanticOrdinal].isStatic;
                const bool consumedParameter = owner->kind == ScopeNameKind::Parameter && callable != nullptr &&
                    owner->semanticOrdinal < callable->parameters.size() &&
                    callable->parameters[owner->semanticOrdinal].passKind == ParameterPassKind::Value &&
                    (callable->parameters[owner->semanticOrdinal].ownership == sema::OwnershipKind::Owned ||
                     (memberModel_ != nullptr &&
                      memberModel_->typeIsMoveOnly(callable->parameters[owner->semanticOrdinal].typeKey)));
                if (!localOwner && !consumedParameter) continue;
                diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error, destination->range(),
                    "view output cannot outlive its possible owner '" + source.substr(1) +
                        "': ref field destination lifetime is unknown");
            }
        };
        if (destination->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto name = lookupScopeName(scopeOrdinal, destination->text());
            const MethodLocalSymbol* alias = name.has_value() && name->kind == ScopeNameKind::Local &&
                name->semanticOrdinal < locals_.size() ? &locals_[name->semanticOrdinal] : nullptr;
            std::size_t remaining = locals_.size();
            while (alias != nullptr && alias->isReference &&
                   alias->referenceReborrowSourceLocalOrdinal.has_value() && remaining != 0U)
            {
                --remaining;
                const auto parent = *alias->referenceReborrowSourceLocalOrdinal;
                alias = parent < locals_.size() ? &locals_[parent] : nullptr;
            }
            if (alias != nullptr && alias->isReference && !alias->referenceReborrowSourceLocalOrdinal.has_value() &&
                alias->syntax != nullptr && hasReferenceField(alias->syntax->first()))
            {
                rejectUnknownReferenceFieldOutput();
                continue;
            }
        }
        if (destination->kind() == ast::AstNodeKind::MemberExpression)
        {
            projectedRoot = valueFieldRoot(destination);
            bool precise = true;
            if (!projectedRoot.has_value() || !storagePlaceKey(destination, scopeOrdinal, &precise).has_value() ||
                !precise)
            {
                // A ref field's receiver does not identify the lifetime of the
                // storage it points at. A per-member origin is not an instance
                // identity, so local owners cannot safely be written through it.
                if (hasReferenceField(destination)) rejectUnknownReferenceFieldOutput();
                continue;
            }
            while (targetSyntax->kind() == ast::AstNodeKind::MemberExpression) targetSyntax = targetSyntax->first();
        }
        if (targetSyntax->kind() != ast::AstNodeKind::IdentifierExpression) continue;
        const auto targetName = lookupScopeName(scopeOrdinal, targetSyntax->text());
        if (projectedRoot.has_value() && (!targetName.has_value() || targetName->kind != ScopeNameKind::Local ||
            targetName->semanticOrdinal != *projectedRoot)) continue;
        const ClassMemberSymbol* caller = scopeOrdinal < scopes_.size()
            ? methodSymbol(scopes_[scopeOrdinal].methodMemberOrdinal) : nullptr;
        std::optional<std::size_t> outputParameter;
        if (targetName.has_value() && targetName->kind == ScopeNameKind::Parameter)
            outputParameter = targetName->semanticOrdinal;
        else if (targetName.has_value() && targetName->kind == ScopeNameKind::Local &&
                 targetName->semanticOrdinal < locals_.size())
        {
            const MethodLocalSymbol& alias = locals_[targetName->semanticOrdinal];
            const auto& origin = alias.referenceEscapeOrigin;
            if (alias.isReference && origin.kind == MethodEscapeOriginKind::ExternalStorage &&
                origin.ordinal.has_value() && alias.referencePlaceKey == "P" + std::to_string(*origin.ordinal) &&
                caller != nullptr && *origin.ordinal < caller->parameters.size() &&
                (caller->parameters[*origin.ordinal].passKind == ParameterPassKind::Ref ||
                 caller->parameters[*origin.ordinal].passKind == ParameterPassKind::Out) &&
                caller->parameters[*origin.ordinal].pointeeTypeKey == alias.typeKey)
                outputParameter = *origin.ordinal;
        }
        if (outputParameter.has_value())
        {
            if (caller != nullptr && *outputParameter < caller->parameters.size() &&
                (caller->parameters[*outputParameter].passKind == ParameterPassKind::Ref ||
                 caller->parameters[*outputParameter].passKind == ParameterPassKind::Out))
            {
                for (const std::string& source : viewInputs.viewSourceFields)
                {
                    if (source.size() <= 1 || source.front() != '@') continue;
                    const std::string_view rootName = std::string_view(source).substr(1);
                    const auto root = lookupScopeName(scopeOrdinal, rootName);
                    if (!root.has_value()) continue;
                    const bool localOwner = root->kind == ScopeNameKind::Local &&
                        root->semanticOrdinal < locals_.size() && !locals_[root->semanticOrdinal].isStatic;
                    const bool consumedParameter = root->kind == ScopeNameKind::Parameter &&
                        root->semanticOrdinal < caller->parameters.size() &&
                        caller->parameters[root->semanticOrdinal].passKind == ParameterPassKind::Value &&
                        (caller->parameters[root->semanticOrdinal].ownership == sema::OwnershipKind::Owned ||
                         (memberModel_ != nullptr &&
                          memberModel_->typeIsMoveOnly(caller->parameters[root->semanticOrdinal].typeKey)));
                    if (!localOwner && !consumedParameter) continue;
                    diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                        diagnostics::DiagnosticSeverity::Error, destination->range(),
                        "view output cannot outlive its possible owner '" + std::string(rootName) + "'");
                }
            }
            continue;
        }
        if (!targetName.has_value() || targetName->kind != ScopeNameKind::Local ||
            targetName->semanticOrdinal >= locals_.size()) continue;
        MethodLocalSymbol& alias = locals_[targetName->semanticOrdinal];
        MethodLocalSymbol* target = &alias;
        const auto& origin = alias.referenceEscapeOrigin;
        if (alias.isReference && origin.kind == MethodEscapeOriginKind::CurrentFrameLocal &&
            origin.ordinal.has_value() && *origin.ordinal < locals_.size() &&
            locals_[*origin.ordinal].methodMemberOrdinal == alias.methodMemberOrdinal)
        {
            const std::string localPlace = "L" + std::to_string(*origin.ordinal);
            bool exactField = false;
            const MethodLocalSymbol* source = &alias;
            std::size_t remaining = locals_.size();
            while (source->referenceReborrowSourceLocalOrdinal.has_value() && remaining != 0U)
            {
                --remaining;
                const std::size_t parent = *source->referenceReborrowSourceLocalOrdinal;
                if (parent >= locals_.size()) break;
                source = &locals_[parent];
            }
            if (!source->referenceReborrowSourceLocalOrdinal.has_value() &&
                source->syntax != nullptr && source->syntax->first() != nullptr)
            {
                const ast::AstNode* field = source->syntax->first();
                const auto root = valueFieldRoot(field);
                bool precise = true;
                const auto place = storagePlaceKey(field, source->scopeOrdinal, &precise);
                exactField = root.has_value() && *root == *origin.ordinal && precise && place.has_value() &&
                    *place == alias.referencePlaceKey;
            }
            if (alias.referencePlaceKey == localPlace || exactField) target = &locals_[*origin.ordinal];
        }
        for (const std::string& source : viewInputs.viewSourceFields)
        {
            if (source.size() <= 1 || source.front() != '@') continue;
            const std::string_view rootName = std::string_view(source).substr(1);
            const auto root = lookupScopeName(scopeOrdinal, rootName);
            const auto visibleRoot = lookupScopeName(target->scopeOrdinal, rootName);
            if (target->isStatic || !root.has_value() || !visibleRoot.has_value() || root->kind != visibleRoot->kind ||
                root->semanticOrdinal != visibleRoot->semanticOrdinal ||
                (root->kind == ScopeNameKind::Local && root->semanticOrdinal < locals_.size() &&
                 !scopeContains(locals_[root->semanticOrdinal].scopeOrdinal, target->scopeOrdinal)))
            {
                diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error, destination->range(),
                    "view output cannot outlive its possible owner '" + std::string(rootName) + "'");
                continue;
            }
            const auto recordRoot = [&](MethodLocalSymbol& output)
            {
                if (std::ranges::find(output.viewSourceFields, source) == output.viewSourceFields.end())
                    output.viewSourceFields.push_back(source);
                for (const ViewSourceIdentity& identity : viewInputs.viewSourceIdentities)
                    if (identity.kind == ViewSourceIdentityKind::Local && identity.ordinal < locals_.size() &&
                        locals_[identity.ordinal].methodMemberOrdinal == output.methodMemberOrdinal &&
                        std::ranges::find(output.viewSourceIdentities, identity) == output.viewSourceIdentities.end())
                        output.viewSourceIdentities.push_back(identity);
                // The legacy output path resolves by name; retain Unknown.
                const ViewSourceIdentity unknown{std::string(rootName), ViewSourceIdentityKind::Unknown, 0};
                if (std::ranges::find(output.viewSourceIdentities, unknown) == output.viewSourceIdentities.end())
                    output.viewSourceIdentities.push_back(unknown);
                if (output.viewRootLocalName.empty()) output.viewRootLocalName = rootName;
            };
            recordRoot(*target);
            if (target != &alias) recordRoot(alias);
        }
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::completeNamespaceFunctionCall(
    const ast::AstNode* expression,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    std::size_t functionOrdinal,
    std::span<ast::AstNode* const> argumentSyntaxes,
    const std::vector<ExpressionResult>& arguments,
    const MethodCallCandidate& selected,
    NamespaceFunctionCallBinding& call,
    bool hasForwardedReceiver,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ClassMemberSymbol& callable = namespaceCallableAdapters()[functionOrdinal];
    if (!hasForwardedReceiver && namespaceFunctionModel_ != nullptr &&
        functionOrdinal < namespaceFunctionModel_->functions().size())
    {
        const NamespaceFunctionSymbol& function = namespaceFunctionModel_->functions()[functionOrdinal];
        if (!function.isExternC && !function.isPrecompiled && function.definitionSyntax != nullptr)
        {
            for (std::size_t index = 0; index < argumentSyntaxes.size() &&
                 index < selected.argumentParameterOrdinals.size(); ++index)
            {
                const ast::AstNode* argument = argumentSyntaxes[index];
                const MethodNameBinding* name = argument != nullptr
                    ? nameBinding(argument, callerCallableOrdinal) : nullptr;
                if (name == nullptr || name->kind != MethodNameBindingKind::Local ||
                    !name->localOrdinal.has_value()) continue;
                const auto environments = fnOnceEnvironments_.find(*name->localOrdinal);
                if (environments == fnOnceEnvironments_.end()) continue;
                const auto count = directCallableParameterInvocations(
                    callable, selected.argumentParameterOrdinals[index]);
                if (!count.has_value() || *count == 0) continue;
                bool consumed = *count > 1;
                for (const ast::AstNode* environment : environments->second)
                    consumed |= consumedFnOnceEnvironments_.contains(environment);
                if (consumed)
                {
                    diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                        diagnostics::DiagnosticSeverity::Error, argument->range(),
                        "FnOnce closure may be invoked more than once through a namespace function parameter");
                    call.resolution = MethodCallResolutionKind::NoViableCandidate;
                    return {};
                }
                consumedFnOnceEnvironments_.insert(environments->second.begin(), environments->second.end());
            }
        }
    }
    checkNamespaceFunctionArgumentPassKinds(
        expression, callable, functionOrdinal, arguments,
        selected, hasForwardedReceiver, diagnostics);
    if (!hasForwardedReceiver && !recognizeCAtomicFence(expression, callerCallableOrdinal, scopeOrdinal,
                                                        functionOrdinal, argumentSyntaxes, diagnostics))
    {
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        return {};
    }
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedFunctionOrdinal = functionOrdinal;
    call.selectedConversions = selected.conversions;
    call.selectedArgumentParameterOrdinals = selected.argumentParameterOrdinals;
    call.selectedArgumentSyntaxes.assign(argumentSyntaxes.begin(), argumentSyntaxes.end());
    call.selectedParameterPackElements = selected.parameterPackElements;
    call.selectedParameterPackOrdinal = selected.parameterPackOrdinal;
    call.selectedUsesExpandedParameterPack = selected.usesExpandedParameterPack;
    call.selectedSynthesizesEmptyParameterPack = selected.synthesizesEmptyParameterPack;
    recordNamespaceFunctionBorrows(
        expression, callerCallableOrdinal, scopeOrdinal, functionOrdinal,
        argumentSyntaxes, arguments, selected);
    recordViewOutputOrigins(call.selectedArgumentSyntaxes, arguments,
        selected, callable, scopeOrdinal, diagnostics);
    rejectNamespaceNewToOrdinaryParameters(callable, argumentSyntaxes, arguments, selected, diagnostics);
    recordNamespaceFunctionDefaults(callable, selected, call);
    call.resultTypeKey = callable.declaredTypeKey;
    ExpressionResult result;
    result.typeKey = callable.declaredTypeKey;
    result.pointerPointeeReadonly = callable.declaredPointerPointeeReadonly;
    result.pointerPointeeVolatile = callable.declaredPointerPointeeVolatile;
    result.pointerLevelQualifiers = callable.declaredPointerLevelQualifiers;
    result.cppPointerPointeeAtomic = callable.cppDeclaredPointerPointeeAtomic;
    result.pointerNullable = callable.declaredPointerNullable;
    result.pointerHasProvenance = callable.declaredPointerHasProvenance;
    result.pointerKnownAlignment = callable.declaredPointerKnownAlignment;
    result.pointerAddressSpace = callable.declaredPointerAddressSpace;
    // EN: T08-D13b. A `volatile T*` (T a scalar) returned by a C function starts the C alias contract with the
    //     volatile mode of the function's reference and C's natural alignment of the pointee. T08-D11/D12: the
    //     result of a C function is a C operand for the integer divergence diagnostic.
    // ES: T08-D13b. Un `volatile T*` (T escalar) devuelto por una funcion C inicia el contrato de alias C con el
    //     modo volatile de la referencia de la funcion y la alineacion natural del apuntado que supone C.
    //     T08-D11/D12: el resultado de una funcion C es operando C para el diagnostico de divergencia entera.
    if (namespaceFunctionModel_ != nullptr && functionOrdinal < namespaceFunctionModel_->functions().size())
    {
        const NamespaceFunctionSymbol& function = namespaceFunctionModel_->functions()[functionOrdinal];
        const bool cFunction = !function.cppCanonicalIdentity.empty() && function.definitionSyntax == nullptr;
        result.cppCOrigin = cFunction;
        if (cFunction && callable.declaredPointerPointeeVolatile)
            if (const std::size_t width = method_body_detail::cVolatilePointeeScalarWidth(callable.declaredTypeKey))
            {
                result.cppVolatilePointeeContract = true;
                result.cppVolatilePointeeMicrosoft = function.cppMicrosoftVolatileSemantics;
                result.cppVolatilePointeeAlignment = width;
            }
    }
    result.valid = true;
    // EN: T19-007. A read-only reference result (`ref readonly T`, or C++ `const T&`) is not assignable.
    // ES: T19-007. Un resultado por referencia de solo lectura (`ref readonly T`, o C++ `const T&`) no es asignable.
    const bool readonlyReference = isReadonlyReferenceReturn(callable);
    result.assignable = callable.isReferenceReturn && !readonlyReference;
    result.addressable = callable.isReferenceReturn;
    result.readonlyReceiver = result.readonlyReceiver || readonlyReference;
    result.ownership = callable.declaredOwnership;
    result.ownershipTransfer = callable.declaredOwnership == sema::OwnershipKind::Owned;
    result.ownershipFreshOwner = result.ownershipTransfer;
    if (result.ownership != sema::OwnershipKind::None)
    {
        result.objectLifetimeOwnership = result.ownership;
        result.storageOwnership = result.ownership;
    }
    // EN: Freeze (plan item 135a): a forwarded extension receiver is one of the arguments.
    // ES: Congelamiento (elemento 135a del plan): un receptor de extension reenviado es uno de los argumentos.
    markCallTemporaryBorrow(
        result, false, arguments, ownedArgumentParameters(selected.argumentParameterOrdinals, callable));
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeNamespaceFunctionCall(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& writtenReference,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    const ExpressionResult* forwardedReceiver,
    const ast::AstNode* extraArgumentSyntax)
{
    ExpressionResult result;
    // EN: T19-040. A call to a C macro that stringifies or pastes calls the wrapper Clang made for it, without the
    //     arguments the macro takes as tokens.
    // ES: T19-040. Una llamada a una macro C que convierte en cadena o pega llama al envoltorio que Clang hizo para
    //     ella, sin los argumentos que la macro toma como tokens.
    const CppMacroCallWrapper* macroCall = namespaceFunctionModel_ != nullptr && forwardedReceiver == nullptr &&
        extraArgumentSyntax == nullptr ? namespaceFunctionModel_->cppMacroCallWrapper(expression) : nullptr;
    NamespaceFunctionReferenceBinding macroReference;
    if (macroCall != nullptr)
    {
        macroReference = writtenReference;
        std::erase_if(macroReference.candidateFunctionOrdinals, [&](std::size_t ordinal) {
            return ordinal >= namespaceFunctionModel_->functions().size() ||
                namespaceFunctionModel_->functions()[ordinal].cppMacroWrapper != macroCall->wrapper;
        });
    }
    const NamespaceFunctionReferenceBinding& reference = macroCall != nullptr ? macroReference : writtenReference;
    NamespaceFunctionCallBinding call;
    call.syntax = expression;
    call.callerCallableOrdinal = callerCallableOrdinal;
    call.scopeOrdinal = scopeOrdinal;
    call.writtenName = reference.writtenName;
    call.forwardedReceiver = forwardedReceiver != nullptr;
    call.receiverSyntax = forwardedReceiver != nullptr ? reference.receiverSyntax : nullptr;
    call.candidateFunctionOrdinals = reference.candidateFunctionOrdinals;

    // NS304-T08-F02: a synthetic call (e.g. an extension-property assignment lowered to its
    // `set_` accessor) may carry one explicit argument that is NOT a child of `expression`.
    // `extraArgumentSyntax` replaces the expression children as the explicit argument list.
    const bool usesExtraArgument = extraArgumentSyntax != nullptr;
    const std::size_t explicitArgumentCount =
        (usesExtraArgument ? 1U : expression->children().size()) -
        (macroCall != nullptr ? macroCall->tokenArguments.size() : 0U);
    // T19-040: all the written arguments, then without the token ones. / Todos los argumentos escritos, luego sin
    //     los de tokens.
    std::vector<ast::AstNode*> argumentSyntaxes = namespaceFunctionArgumentSyntaxes(
        expression, reference, forwardedReceiver, extraArgumentSyntax,
        explicitArgumentCount + (macroCall != nullptr ? macroCall->tokenArguments.size() : 0U));
    if (macroCall != nullptr)
        for (auto index = macroCall->tokenArguments.rbegin(); index != macroCall->tokenArguments.rend(); ++index)
            if (*index < argumentSyntaxes.size())
                argumentSyntaxes.erase(argumentSyntaxes.begin() + static_cast<std::ptrdiff_t>(*index));
    call.cppMacroTokenArguments = macroCall != nullptr ? macroCall->tokenArguments : std::vector<std::size_t>{};
    bool argumentsValid = true;
    std::vector<ExpressionResult> arguments = analyzeNamespaceFunctionArguments(
        expression, reference, argumentSyntaxes, explicitArgumentCount,
        callerCallableOrdinal, scopeOrdinal, forwardedReceiver,
        extraArgumentSyntax, argumentsValid, diagnostics);
    if (!argumentsValid)
    {
        storeNamespaceFunctionCall(expression, call);
        return result;
    }

    bool matchingArity = false;
    evaluateNamespaceFunctionCandidates(
        reference, argumentSyntaxes, arguments, forwardedReceiver != nullptr,
        call, matchingArity);
    const std::optional<std::size_t> selectedIndex = selectNamespaceFunctionCandidate(
        expression, reference, argumentSyntaxes, explicitArgumentCount,
        matchingArity, call, diagnostics);
    if (!selectedIndex.has_value()) return result;
    const MethodCallCandidate& selected = call.evaluatedCandidates[*selectedIndex];
    const std::size_t functionOrdinal = selected.memberOrdinal;
    if (!validateNamespaceFunctionDefinition(
            expression, functionOrdinal, diagnostics))
    {
        storeNamespaceFunctionCall(expression, call);
        return result;
    }
    result = completeNamespaceFunctionCall(
        expression, callerCallableOrdinal, scopeOrdinal, functionOrdinal,
        argumentSyntaxes, arguments, selected, call,
        forwardedReceiver != nullptr, diagnostics);
    storeNamespaceFunctionCall(expression, call);
    return result;
}

void ProjectMethodBodyModel::recognizeCoreStringOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.String" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> coreKind;
        if (selectedMember.name == "Concat" && selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:string")
        {
            coreKind = MethodCoreOperationKind::StringConcat;
        }
        else if (selectedMember.name == "Equals" && selectedMember.declaredTypeKey == "builtin:bool" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:string")
        {
            coreKind = MethodCoreOperationKind::StringEquals;
        }
        else if (selectedMember.name == "Substring" && selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:int" &&
            selectedMember.parameters[2].typeKey == "builtin:int")
        {
            coreKind = MethodCoreOperationKind::StringSubstring;
        }
        else if (selectedMember.name == "FromCharArray" &&
            selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:char[]")
        {
            coreKind = MethodCoreOperationKind::StringFromCharArray;
        }
        // SHR-010: `String.CharacterPointer(texto, indice)` -> `char*`. El
        // reconocimiento es por firma EXACTA, igual que las cuatro de arriba: clase
        // cualificada, estatico, tipo de retorno y tipos de parametros. Un metodo que se
        // llame igual con otra firma no es esta operacion y se compila como codigo
        // normal.
        else if (selectedMember.name == "CharacterPointer" &&
            selectedMember.declaredTypeKey == "builtin:char*" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:int")
        {
            coreKind = MethodCoreOperationKind::StringCharacterPointer;
        }
        if (coreKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *coreKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreFloat128Operations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Entrega 228: SIMD por familia justificada, reconocida por identidad y firma exactas.
    // Comparar los parametros con el tipo de retorno evita acoplar esta regla a la
    // codificacion interna del type-key del proyecto que contiene NativeSharp.Core.
    const std::string compactFloatVectorName =
        declaringType.identity.fullyQualifiedName();
    if ((compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x2" ||
            compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x3" ||
            compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x4") &&
        selectedMember.isStatic &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
    {
        std::optional<MethodCoreOperationKind> simdKind;
        if (selectedMember.name == "Add")
            simdKind = MethodCoreOperationKind::SimdFloat32x4Add;
        else if (selectedMember.name == "Subtract")
            simdKind = MethodCoreOperationKind::SimdFloat32x4Subtract;
        else if (selectedMember.name == "Multiply")
            simdKind = MethodCoreOperationKind::SimdFloat32x4Multiply;
        if (simdKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *simdKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
    if (compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x4" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> shuffleKind;
        if (selectedMember.name == "SwizzleWzyx" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey)
            shuffleKind = MethodCoreOperationKind::SimdSwizzleWzyx;
        else if (selectedMember.name == "ShuffleLowHigh" && selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
            selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
            shuffleKind = MethodCoreOperationKind::SimdShuffleLowHigh;
        if (shuffleKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *shuffleKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
    if (compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x4" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> mathKind;
        if (selectedMember.name == "Sqrt" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey)
            mathKind = MethodCoreOperationKind::SimdSqrt;
        else if (selectedMember.name == "ReciprocalSqrt" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey)
            mathKind = MethodCoreOperationKind::SimdReciprocalSqrt;
        else if (selectedMember.name == "FusedMultiplyAdd" && selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
            selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey &&
            selectedMember.parameters[2].typeKey == selectedMember.declaredTypeKey)
            mathKind = MethodCoreOperationKind::SimdFusedMultiplyAdd;
        if (mathKind.has_value())
            coreBindings_.push_back(MethodCoreBinding{*mathKind, expression, methodMemberOrdinal,
                scopeOrdinal, selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
    }
}

void ProjectMethodBodyModel::recognizeCoreInt128Operations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Item 347: the two public signed 128-bit integer carriers share one intrinsic
    // family. Identity and exact self/self -> self signatures prevent user methods with
    // coincidentally matching names from acquiring compiler semantics.
    const std::string integerVectorName = declaringType.identity.fullyQualifiedName();
    if ((integerVectorName == "NativeSharp.Core.Numerics.Int32x4" ||
            integerVectorName == "NativeSharp.Core.Numerics.Int64x2") &&
        selectedMember.isStatic && selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
    {
        std::optional<MethodCoreOperationKind> simdKind;
        if (selectedMember.name == "Add")
            simdKind = MethodCoreOperationKind::SimdInt128Add;
        else if (selectedMember.name == "Subtract")
            simdKind = MethodCoreOperationKind::SimdInt128Subtract;
        else if (selectedMember.name == "Multiply")
            simdKind = MethodCoreOperationKind::SimdInt128Multiply;
        if (simdKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *simdKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
    if (integerVectorName == "NativeSharp.Core.Numerics.Int32x4" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> memoryKind;
        if (selectedMember.name == "LoadAligned" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey + "*")
        {
            memoryKind = MethodCoreOperationKind::SimdLoadAligned128;
        }
        else if (selectedMember.name == "StoreAligned" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey ==
                selectedMember.parameters[1].typeKey + "*")
        {
            memoryKind = MethodCoreOperationKind::SimdStoreAligned128;
        }
        else if (selectedMember.name == "LoadUnaligned" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey + "*")
        {
            memoryKind = MethodCoreOperationKind::SimdLoadUnaligned128;
        }
        else if (selectedMember.name == "StoreUnaligned" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey ==
                selectedMember.parameters[1].typeKey + "*")
        {
            memoryKind = MethodCoreOperationKind::SimdStoreUnaligned128;
        }
        if (memoryKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *memoryKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreHalfOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Item 354: Half is a two-byte ABI carrier. Only these exact conversion signatures
    // acquire native IEEE binary16 semantics.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Numerics.Half" &&
        selectedMember.isStatic && selectedMember.parameters.size() == 1)
    {
        std::optional<MethodCoreOperationKind> halfKind;
        if (selectedMember.name == "FromFloat" &&
            selectedMember.parameters[0].typeKey == "builtin:float" &&
            selectedMember.declaredTypeKey != "builtin:float")
            halfKind = MethodCoreOperationKind::HalfFromFloat;
        else if (selectedMember.name == "ToFloat" &&
            selectedMember.declaredTypeKey == "builtin:float" &&
            selectedMember.parameters[0].typeKey != "builtin:float")
            halfKind = MethodCoreOperationKind::HalfToFloat;
        if (halfKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *halfKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.parameters[0].typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreScalarOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreHalfOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Numerics.BitOperations" &&
        selectedMember.isStatic && selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        (selectedMember.declaredTypeKey == "builtin:uint" ||
            selectedMember.declaredTypeKey == "builtin:ulong"))
    {
        std::optional<MethodCoreOperationKind> bitKind;
        if (selectedMember.name == "PopCount")
            bitKind = MethodCoreOperationKind::BitPopulationCount;
        else if (selectedMember.name == "LeadingZeroCount")
            bitKind = MethodCoreOperationKind::BitLeadingZeroCount;
        else if (selectedMember.name == "TrailingZeroCount")
            bitKind = MethodCoreOperationKind::BitTrailingZeroCount;
        if (bitKind.has_value())
            coreBindings_.push_back(MethodCoreBinding{*bitKind, expression, methodMemberOrdinal,
                scopeOrdinal, selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
    }
    if (declaringType.identity.fullyQualifiedName() ==
            "NativeSharp.Core.Runtime.CompilerServices.HardwareIntrinsics" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> hardwareKind;
        if (selectedMember.name == "PrefetchRead" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:void*" &&
            selectedMember.declaredTypeKey == "builtin:void")
            hardwareKind = MethodCoreOperationKind::CpuPrefetchRead;
        else if (selectedMember.name == "Pause" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
            hardwareKind = MethodCoreOperationKind::CpuPause;
        else if (selectedMember.name == "ReadTimestamp" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:ulong")
            hardwareKind = MethodCoreOperationKind::CpuReadTimestamp;
        if (hardwareKind.has_value())
            coreBindings_.push_back(MethodCoreBinding{*hardwareKind, expression, methodMemberOrdinal,
                scopeOrdinal, selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters[0].typeKey,
                false, std::nullopt, std::nullopt});
    }
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Numerics.BitOperations" &&
        selectedMember.isStatic &&
        (selectedMember.declaredTypeKey == "builtin:uint" || selectedMember.declaredTypeKey == "builtin:ulong"))
    {
        std::optional<MethodCoreOperationKind> rotateKind;
        if ((selectedMember.name == "RotateLeft" || selectedMember.name == "RotateRight") &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
            selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
            rotateKind = selectedMember.name == "RotateLeft"
                ? MethodCoreOperationKind::BitRotateLeft : MethodCoreOperationKind::BitRotateRight;
        else if (selectedMember.name == "ReverseEndianness" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey)
            rotateKind = MethodCoreOperationKind::BitByteSwap;
        if (rotateKind.has_value())
            coreBindings_.push_back(MethodCoreBinding{*rotateKind, expression, methodMemberOrdinal,
                scopeOrdinal, selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
    }
}

void ProjectMethodBodyModel::recognizeCoreVector256Operations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Item 348: the 256-bit floating family uses the same exact-signature gate as the
    // earlier SIMD families while retaining distinct MIR operations for FP semantics.
    const std::string floatingVector256Name =
        declaringType.identity.fullyQualifiedName();
    if ((floatingVector256Name == "NativeSharp.Core.Numerics.Float32x8" ||
            floatingVector256Name == "NativeSharp.Core.Numerics.Float64x4") &&
        selectedMember.isStatic && selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
    {
        std::optional<MethodCoreOperationKind> simdKind;
        if (selectedMember.name == "Add")
            simdKind = MethodCoreOperationKind::SimdFloat256Add;
        else if (selectedMember.name == "Subtract")
            simdKind = MethodCoreOperationKind::SimdFloat256Subtract;
        else if (selectedMember.name == "Multiply")
            simdKind = MethodCoreOperationKind::SimdFloat256Multiply;
        if (simdKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *simdKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
    const std::string integerVector256Name =
        declaringType.identity.fullyQualifiedName();
    if ((integerVector256Name == "NativeSharp.Core.Numerics.Int32x8" ||
            integerVector256Name == "NativeSharp.Core.Numerics.Int64x4") &&
        selectedMember.isStatic && selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
    {
        std::optional<MethodCoreOperationKind> simdKind;
        if (selectedMember.name == "Add")
            simdKind = MethodCoreOperationKind::SimdInt256Add;
        else if (selectedMember.name == "Subtract")
            simdKind = MethodCoreOperationKind::SimdInt256Subtract;
        else if (selectedMember.name == "Multiply")
            simdKind = MethodCoreOperationKind::SimdInt256Multiply;
        if (simdKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *simdKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreConsoleRuntimeOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // SHR-022. Las cuatro salidas de consola. La firma es identica en las cuatro
    // --estatica, void, un solo `string`-- asi que lo unico que las separa es el nombre, y
    // por eso la tabla es de nombre a operacion en vez de cuatro `if` copiados.
    //
    // POR QUE `ErrorWriteLine` Y NO `Console.Error.WriteLine`. Lo segundo se lee mejor y
    // pide un objeto: `Error` tendria que ser una propiedad que devuelve un TextWriter, con
    // despacho de instancia. TextWriter es SHR-020 y vive en el Bloque 5. Adelantarlo para
    // ganar un punto en el nombre habria arrastrado un tipo entero y su modelo de despacho
    // dentro del Bloque 2. Cuando TextWriter exista, estas cuatro siguen siendo las
    // primitivas de debajo y la forma con objeto se construye encima sin tocar el backend.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Console" &&
        selectedMember.isStatic &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters.front().typeKey == "builtin:string")
    {
        std::optional<MethodCoreOperationKind> consoleKind;
        if (selectedMember.name == "WriteLine")
            consoleKind = MethodCoreOperationKind::ConsoleWriteLine;
        else if (selectedMember.name == "Write")
            consoleKind = MethodCoreOperationKind::ConsoleWrite;
        else if (selectedMember.name == "ErrorWriteLine")
            consoleKind = MethodCoreOperationKind::ConsoleErrorWriteLine;
        else if (selectedMember.name == "ErrorWrite")
            consoleKind = MethodCoreOperationKind::ConsoleErrorWrite;
        if (consoleKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *consoleKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "builtin:void",
                "builtin:string",
                false, std::nullopt, std::nullopt});
        }
    }
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Console" &&
        selectedMember.isStatic && selectedMember.name == "ReadLine" &&
        selectedMember.declaredTypeKey == "builtin:string" &&
        selectedMember.declaredOwnership == sema::OwnershipKind::Owned &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters.front().pointeeTypeKey == "builtin:int" &&
        selectedMember.parameters.front().passKind == ParameterPassKind::Out)
    {
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::ConsoleReadLine,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            "builtin:string",
            "builtin:int",
            false, std::nullopt, std::nullopt});
    }
    // C8 "type root" -- NativeSharp.Core.RuntimeHelpers' two primitives. Same recognition
    // shape as every core operation before them (exact class, exact static method name,
    // exact signature): read a type's name out of its runtime descriptor, and turn a
    // reference into an integer. Neither can be written in NativeSharp, which is the
    // whole reason they are core operations rather than library code -- see Object.ns.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.RuntimeHelpers" &&
        selectedMember.isStatic && selectedMember.parameters.size() == 1)
    {
        std::optional<MethodCoreOperationKind> helperKind;
        if (selectedMember.name == "TypeName" &&
            selectedMember.declaredTypeKey == "builtin:string")
        {
            helperKind = MethodCoreOperationKind::ObjectTypeName;
        }
        else if (selectedMember.name == "ReferenceHash" &&
            selectedMember.declaredTypeKey == "builtin:int")
        {
            helperKind = MethodCoreOperationKind::ObjectReferenceHash;
        }
        if (helperKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *helperKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreClockOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 53.1 "Time" -- NativeSharp.Core.Diagnostics.NativeClock's three raw
    // zero-argument Win32 wrappers. Same recognition shape as ConsoleWriteLine just
    // above (exact class, exact static method name, exact signature), just three
    // sibling methods instead of one. See NativeClock.ns for the full rationale.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Diagnostics.NativeClock" &&
        selectedMember.isStatic &&
        selectedMember.declaredTypeKey == "builtin:long" &&
        selectedMember.parameters.empty())
    {
        std::optional<MethodCoreOperationKind> clockKind;
        if (selectedMember.name == "QueryCounterRaw")
        {
            clockKind = MethodCoreOperationKind::ClockQueryCounter;
        }
        else if (selectedMember.name == "QueryFrequencyRaw")
        {
            clockKind = MethodCoreOperationKind::ClockQueryFrequency;
        }
        else if (selectedMember.name == "QuerySystemFileTimeRaw")
        {
            clockKind = MethodCoreOperationKind::ClockQuerySystemFileTime;
        }
        if (clockKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *clockKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "builtin:long",
                std::string{},
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreClockEnvironmentOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreClockOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // Stage 53.2 "Environment" -- NativeSharp.Core.Environment's three raw Win32
    // wrappers. TickCount64/ProcessorCount reuse the exact zero-argument recognition
    // shape NativeClock established just above (only the declared return type differs
    // between the two); Exit reuses the one-argument/void shape Console.WriteLine
    // established further above. See Environment.ns for why GetEnvironmentVariable/
    // SetEnvironmentVariable are NOT recognized here -- that needs dynamic string
    // allocation from the runtime, a genuinely new kind of marshaling deferred to its
    // own round (53.2b).
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Environment" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> environmentKind;
        if (selectedMember.name == "TickCount64" &&
            selectedMember.declaredTypeKey == "builtin:long" &&
            selectedMember.parameters.empty())
        {
            environmentKind = MethodCoreOperationKind::EnvironmentTickCount64;
        }
        else if (selectedMember.name == "ProcessorCount" &&
            selectedMember.declaredTypeKey == "builtin:int" &&
            selectedMember.parameters.empty())
        {
            environmentKind = MethodCoreOperationKind::EnvironmentProcessorCount;
        }
        else if (selectedMember.name == "Exit" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:int")
        {
            environmentKind = MethodCoreOperationKind::EnvironmentExit;
        }
        // Stage 53.2b "Environment (variables)" -- GetEnvironmentVariable/
        // SetEnvironmentVariable, the piece Stage 53.2's first round deliberately left
        // out (see Environment.ns). Recognized the same way as every other core
        // operation: exact class, exact static method name, exact signature.
        else if (selectedMember.name == "GetEnvironmentVariable" &&
            selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:string")
        {
            environmentKind = MethodCoreOperationKind::EnvironmentGetVariable;
        }
        else if (selectedMember.name == "SetEnvironmentVariable" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:string")
        {
            environmentKind = MethodCoreOperationKind::EnvironmentSetVariable;
        }
        // Stage 53.5 "Command-line arguments" -- GetCommandLineArgCount (zero-argument,
        // same shape as TickCount64/ProcessorCount above) and GetCommandLineArgs (one
        // builtin:string[] argument -- the "buffer provided by the caller" pattern, see
        // Environment.ns's header comment).
        // Bloque 4, fase A. CurrentDirectory: cero argumentos, builtin:string fuera --
        // la misma forma de cero operandos que TickCount64/ProcessorCount, con el tipo de
        // salida de GetEnvironmentVariable. Cierra `current_path`, la ultima de las siete
        // operaciones de filesystem que el doc 81 midio.
        else if (selectedMember.name == "CurrentDirectory" &&
            selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.empty())
        {
            environmentKind = MethodCoreOperationKind::EnvironmentCurrentDirectory;
        }
        else if (selectedMember.name == "GetCommandLineArgCount" &&
            selectedMember.declaredTypeKey == "builtin:int" &&
            selectedMember.parameters.empty())
        {
            environmentKind = MethodCoreOperationKind::EnvironmentGetCommandLineArgCount;
        }
        else if (selectedMember.name == "GetCommandLineArgs" &&
            selectedMember.declaredTypeKey == "builtin:int" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:string[]")
        {
            environmentKind = MethodCoreOperationKind::EnvironmentGetCommandLineArgs;
        }
        if (environmentKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *environmentKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty()
                    ? std::string{}
                    : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreFileDirectoryOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 53.3 "Filesystem/Paths" -- NativeSharp.Core.IO.File.Exists/Delete and
    // NativeSharp.Core.IO.Directory.Exists/CreateDirectory. Same one-argument recognition
    // shape as Environment.GetEnvironmentVariable (Exists) or Environment.Exit
    // (Delete/CreateDirectory, one argument + void return) above -- two classes instead
    // of one, so this is its own "if", not folded into the Environment block.
    if ((declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.File" ||
            declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.Directory") &&
        selectedMember.isStatic)
    {
        const bool isFile =
            declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.File";
        std::optional<MethodCoreOperationKind> ioKind =
            basicFileDirectoryOperation(selectedMember, isFile);
        if (!ioKind.has_value()) ioKind = fileContentOperation(selectedMember, isFile);
        if (!ioKind.has_value()) ioKind = directoryEnumerationOperation(selectedMember, isFile);
        if (ioKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *ioKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::basicFileDirectoryOperation(
    const ClassMemberSymbol& selectedMember,
    bool isFile)
{
    if (selectedMember.name == "Exists" &&
        selectedMember.declaredTypeKey == "builtin:bool" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return isFile ? MethodCoreOperationKind::FileExists
                      : MethodCoreOperationKind::DirectoryExists;
    if (isFile && selectedMember.name == "Delete" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileDelete;
    if (!isFile && selectedMember.name == "CreateDirectory" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::DirectoryCreate;
    // Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- ReadAllText/
    // WriteAllText/AppendAllText, File-only (Directory has no equivalent). Same
    // one-argument-returns-string shape as GetEnvironmentVariable (ReadAllText), and
    // the same two-argument-void shape as SetEnvironmentVariable
    // (WriteAllText/AppendAllText).
    if (isFile && selectedMember.name == "ReadAllText" &&
        selectedMember.declaredTypeKey == "builtin:string" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileReadAllText;
    if (isFile && selectedMember.name == "WriteAllText" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileWriteAllText;
    return std::nullopt;
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::fileContentOperation(
    const ClassMemberSymbol& selectedMember,
    bool isFile)
{
    // Bloque 4, fase A -- SHR-028. ReadAllBytes: misma forma de un argumento que
    // ReadAllText, y el mismo tipo de salida en memoria: un `byte[]` y un `string`
    // comparten layout, { i64 longitud; datos }.
    if (isFile && selectedMember.name == "ReadAllBytes" &&
        selectedMember.declaredTypeKey == "builtin:byte[]" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileReadAllBytes;
    // ReadBytes: ruta + arreglo del llamador, cuantos bytes se leyeron fuera. El
    // patron "buffer provisto por el llamador" de Directory.GetFiles, que es el que
    // no asigna y por tanto el que usara el driver.
    if (isFile && selectedMember.name == "ReadBytes" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:byte[]")
        return MethodCoreOperationKind::FileReadBytes;
    // SHR-029. WriteAllBytes: ruta + arreglo + CUANTOS bytes escribir. El contador es
    // explicito y no la longitud del arreglo porque quien construye un buffer casi
    // siempre lo tiene mas grande que lo que ha llenado.
    if (isFile && selectedMember.name == "WriteAllBytes" &&
        selectedMember.declaredTypeKey == "builtin:bool" &&
        selectedMember.parameters.size() == 3 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:byte[]" &&
        selectedMember.parameters[2].typeKey == "builtin:int")
        return MethodCoreOperationKind::FileWriteAllBytes;
    // Bloque 4, fase A -- SHR-030. `File.GetSize(string)` devuelve builtin:long.
    // Misma forma de un argumento que ReadAllText, otro tipo de salida.
    if (isFile && selectedMember.name == "GetSize" &&
        selectedMember.declaredTypeKey == "builtin:long" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileGetSize;
    if (isFile && selectedMember.name == "AppendAllText" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileAppendAllText;
    return std::nullopt;
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::directoryEnumerationOperation(
    const ClassMemberSymbol& selectedMember,
    bool isFile)
{
    // Stage 53.4 "Filesystem/Paths (cierre)" -- Directory.Delete, calco exacto de
    // File.Delete arriba (mismo one-argument/void shape), solo del lado !isFile.
    if (!isFile && selectedMember.name == "Delete" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::DirectoryDelete;
    // Stage 53.6 "Enumeracion de directorios" -- GetFileCount/GetDirectoryCount (un
    // builtin:string entra, builtin:int sale) y GetFiles/GetDirectories (un
    // builtin:string + un builtin:string[] entran -- la "buffer provisto por el
    // caller" que GetCommandLineArgs establecio -- builtin:int sale). Ver el
    // comentario de cabecera de Directory.ns para el diseno completo.
    if (!isFile && selectedMember.name == "GetFileCount" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::DirectoryGetFileCount;
    if (!isFile && selectedMember.name == "GetFiles" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string[]")
        return MethodCoreOperationKind::DirectoryGetFiles;
    if (!isFile && selectedMember.name == "GetDirectoryCount" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::DirectoryGetDirectoryCount;
    if (!isFile && selectedMember.name == "GetDirectories" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string[]")
        return MethodCoreOperationKind::DirectoryGetDirectories;
    // Pre-Stage-56 "utilidades basicas" -- File.CopyRaw/MoveRaw, Directory.MoveRaw.
    // CopyRaw is File-only, three operands (source, dest, overwrite) -- joins
    // StringSubstring in the HIR bucket's expected=3U list. MoveRaw exists on BOTH
    // File and Directory (Win32's MoveFileA works identically for either -- see
    // buildMoveFile in WindowsRuntimeBuilder.cpp, the first runtime hook shared by a
    // File AND a Directory core operation, same "shared wrapper" precedent
    // Semaphore.Wait/Event.Wait established in Stage 54.5), two operands each, falls
    // through to the default expected=2U bucket.
    if (isFile && selectedMember.name == "CopyRaw" &&
        selectedMember.declaredTypeKey == "builtin:bool" &&
        selectedMember.parameters.size() == 3 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string" &&
        selectedMember.parameters[2].typeKey == "builtin:bool")
        return MethodCoreOperationKind::FileCopyRaw;
    if (selectedMember.name == "MoveRaw" &&
        selectedMember.declaredTypeKey == "builtin:bool" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string")
        return isFile ? MethodCoreOperationKind::FileMoveRaw
                      : MethodCoreOperationKind::DirectoryMoveRaw;
    return std::nullopt;
}

void ProjectMethodBodyModel::recognizeCoreFilePathOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreFileDirectoryOperations(
        expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // Stage 53.4 "Filesystem/Paths (cierre)" -- NativeSharp.Core.IO.Path.GetFullPath, la
    // PRIMERA operacion nucleo de Path (ver Path.ns): un builtin:string (path) entra, un
    // builtin:string (recien alocado por el runtime, NO owned -- mismo patron que
    // GetEnvironmentVariable/ReadAllText) sale. Su propio "if" porque Path es una tercera
    // clase distinta de File/Directory arriba, y todos los demas metodos de Path
    // (Combine, GetFileName, etc.) deben seguir siendo NativeSharp puro, sin pasar por
    // esta rama en absoluto.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.Path" &&
        selectedMember.isStatic &&
        selectedMember.name == "GetFullPath" &&
        selectedMember.declaredTypeKey == "builtin:string" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
    {
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::PathGetFullPath,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            selectedMember.declaredTypeKey,
            selectedMember.parameters.front().typeKey,
            false, std::nullopt, std::nullopt});
    }
}

void ProjectMethodBodyModel::recognizeCoreThreadOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 54.1 "Threads" -- NativeSharp.Core.Threading.Thread's tres helpers privados y
    // estaticos. A diferencia de TODOS los core operations anteriores, estos tres NO son
    // llamados directamente por codigo NativeSharp del usuario -- son llamados desde
    // adentro de la propia clase Thread (constructor/Join()/Detach()), que es codigo
    // NativeSharp normal, no interceptado. Por eso mismo el reconocimiento sigue siendo
    // "clase exacta + nombre exacto + firma exacta", igual que cualquier otro core
    // operation: el compilador no distingue "quien" llama, solo QUE se llama. Ver
    // Thread.ns para el diseno completo.
    //
    // ThreadCreate: un argumento de tipo delegate (ThreadStart) entra, builtin:nint (el
    // HANDLE de Win32, guardado por valor) sale. Primera vez que un core operation recibe
    // un delegate como operando -- ver ProjectOopHirModel.cpp para como el bloque HIR
    // compartido (bucket expected=1U) no necesita ningun cambio para aceptar esto: nunca
    // exigio un tipo de operando particular, solo bajo lo que hubiera ahi con
    // lowerExpression.
    // ThreadJoin/ThreadDetach: un builtin:nint (el HANDLE que el llamador ya tiene
    // guardado) entra, builtin:void sale -- mismo shape que DirectoryDelete, cambiando
    // solo el tipo del operando (nint en vez de string).
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Thread" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> threadKind;
        if (selectedMember.name == "ThreadCreate" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 1)
        {
            // ThreadStart's typeKey is "type:" + identity.key(), y key() incluye el
            // moduleName ("modulo::NombreCalificado") -- NO simplemente
            // "type:<FullyQualifiedName>". En vez de asumir un moduleName literal (fragil
            // si cambia el nombre del modulo de la biblioteca), resolvemos una vez el ordinal
            // de ThreadStart por su nombre completamente calificado al iniciar el modelo y
            // comparamos contra la key() real -- el mismo patron defensivo que
            // delegateTypeOrdinalForTypeKey (mas abajo en este archivo) usa yendo en la
            // direccion inversa (typeKey -> ordinal).
            const bool parameterIsThreadStart = !coreThreadStartTypeKey_.empty() &&
                selectedMember.parameters[0].typeKey == coreThreadStartTypeKey_;
            if (parameterIsThreadStart)
            {
                threadKind = MethodCoreOperationKind::ThreadCreate;
            }
        }
        else if (selectedMember.name == "ThreadJoin" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            threadKind = MethodCoreOperationKind::ThreadJoin;
        }
        else if (selectedMember.name == "ThreadDetach" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            threadKind = MethodCoreOperationKind::ThreadDetach;
        }
        // Pre-Stage-56 "utilidades basicas" -- Thread.SleepRaw. Same one-argument
        // (builtin:int)/void shape as Environment.Exit (53.2) -- sleeps the CALLING
        // thread (Win32 Sleep(ms)), not any particular Thread instance, which is why it
        // lives here as a static helper rather than on a Thread receiver.
        else if (selectedMember.name == "SleepRaw" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:int")
        {
            threadKind = MethodCoreOperationKind::ThreadSleepRaw;
        }
        if (threadKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *threadKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

bool ProjectMethodBodyModel::isMemoryOrderTypeKey(std::string_view typeKey) const
{
    return !coreMemoryOrderTypeKey_.empty() && typeKey == coreMemoryOrderTypeKey_;
}

void ProjectMethodBodyModel::recognizeCoreAtomicValueOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    ExpressionResult& result,
    diagnostics::DiagnosticEngine& diagnostics)
{
    (void)result;
    // Stage 54.2/Item 311 atomics. Recognition is deliberately exact by declaring type,
    // name, parameter mode, value type, result type, and MemoryOrder position.
    //
    // Wrapper calls operate on their receiver's private `value_` field. This block resolves
    // that field once and stores its ordinal in implicitFieldMemberOrdinal, allowing HIR to
    // synthesize a FieldAddress without exposing implementation storage.
    //
    // Public Interlocked calls are static and begin with a true `ref int`, `ref long`, or
    // `ref void*` parameter. For those bindings implicitFieldMemberOrdinal stays empty:
    // HIR consumes the already-address-valued first argument as the location. Both source
    // shapes reuse the same MethodCoreOperationKind identities and all later lowering.
    const std::string atomicDeclaringType = declaringType.identity.fullyQualifiedName();
    const bool atomicWrapper =
        atomicDeclaringType == "NativeSharp.Core.Threading.AtomicInt32" ||
        atomicDeclaringType == "NativeSharp.Core.Threading.AtomicInt64" ||
        atomicDeclaringType == "NativeSharp.Core.Threading.AtomicPointer";
    const bool internalSharedAtomic =
        atomicDeclaringType == "NativeSharp.Core.Internal.SharedAtomic" &&
        !selectedMember.parameters.empty() &&
        selectedMember.parameters[0].pointeeTypeKey == "builtin:long" &&
        (selectedMember.name == "Load" || selectedMember.name == "CompareExchange");
    const bool publicInterlocked =
        (atomicDeclaringType == "NativeSharp.Core.Threading.Interlocked" || internalSharedAtomic) &&
        selectedMember.isStatic && !selectedMember.parameters.empty() &&
        selectedMember.parameters[0].passKind == ParameterPassKind::Ref &&
        (selectedMember.parameters[0].pointeeTypeKey == "builtin:int" ||
         selectedMember.parameters[0].pointeeTypeKey == "builtin:long" ||
         selectedMember.parameters[0].pointeeTypeKey == "builtin:void*");
    if (!((atomicWrapper && !selectedMember.isStatic) || publicInterlocked)) return;
    const std::size_t parameterOffset = publicInterlocked ? 1U : 0U;
    const std::string locationValueType = publicInterlocked
        ? selectedMember.parameters[0].pointeeTypeKey
        : atomicDeclaringType == "NativeSharp.Core.Threading.AtomicInt32"
            ? "builtin:int" : atomicDeclaringType == "NativeSharp.Core.Threading.AtomicPointer"
                ? "builtin:void*" : "builtin:long";
    const bool isInt32 = locationValueType == "builtin:int";
    const bool isPointer = locationValueType == "builtin:void*";
    const std::string valueTypeKey =
        isInt32 ? "builtin:int" : isPointer ? "builtin:void*" : "builtin:long";
    bool orderedLoad = false;
    bool orderedStore = false;
    bool orderedRmw = false;
    bool orderedCompareExchange = false;
    classifyOrderedAtomicCall(
        orderedLoad, orderedStore, orderedRmw, orderedCompareExchange,
        selectedMember, parameterOffset, valueTypeKey);
    std::optional<MethodCoreOperationKind> atomicKind = atomicOperationKind(
        selectedMember, parameterOffset, valueTypeKey, isInt32, isPointer,
        orderedLoad, orderedStore, orderedRmw, orderedCompareExchange);
    // EN: The order checks and the lowering of atomic operations read arguments by position, so a
    //     named or reordered argument list validated and executed the wrong operands (LKF-283).
    // ES: Las comprobaciones de orden y el lowering atómico leen los argumentos por posición; una
    //     lista con nombres o reordenada validaba y ejecutaba operandos equivocados (LKF-283).
    if (atomicKind.has_value() && std::ranges::any_of(expression->children(), [](const ast::AstNode* argument)
        {
            return argument != nullptr && !argument->argumentName().empty();
        }))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "atomic operations take positional arguments; write the arguments in parameter order "
            "without names");
        return;
    }
    std::optional<std::uint8_t> atomicMemoryOrder;
    std::optional<std::uint8_t> atomicFailureMemoryOrder;
    if (atomicKind.has_value() && orderedCompareExchange)
        validateAtomicCompareExchangeOrders(
            atomicKind, atomicMemoryOrder, atomicFailureMemoryOrder,
            expression, selectedMember, parameterOffset, methodMemberOrdinal, diagnostics);
    else if (atomicKind.has_value() && (orderedLoad || orderedStore || orderedRmw))
        validateAtomicMemoryOrder(
            atomicKind, atomicMemoryOrder, expression, selectedMember,
            methodMemberOrdinal, orderedLoad, orderedStore, diagnostics);
    if (atomicKind.has_value())
        bindAtomicOperation(
            *atomicKind, atomicMemoryOrder, atomicFailureMemoryOrder,
            expression, selectedMember, valueTypeKey, atomicWrapper, publicInterlocked,
            methodMemberOrdinal, scopeOrdinal);
}

void ProjectMethodBodyModel::classifyOrderedAtomicCall(
    bool& orderedLoad,
    bool& orderedStore,
    bool& orderedRmw,
    bool& orderedCompareExchange,
    const ClassMemberSymbol& selectedMember,
    std::size_t parameterOffset,
    const std::string& valueTypeKey) const
{
    orderedLoad = selectedMember.name == "Load" &&
        selectedMember.parameters.size() == parameterOffset + 1U &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset].typeKey) &&
        selectedMember.declaredTypeKey == valueTypeKey;
    orderedStore = selectedMember.name == "Store" &&
        selectedMember.parameters.size() == parameterOffset + 2U &&
        selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset + 1U].typeKey) &&
        selectedMember.declaredTypeKey == "builtin:void";
    const bool orderedUnaryRmw =
        (selectedMember.name == "Increment" || selectedMember.name == "Decrement") &&
        selectedMember.parameters.size() == parameterOffset + 1U &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset].typeKey) &&
        selectedMember.declaredTypeKey == valueTypeKey;
    const bool orderedValueRmw =
        (selectedMember.name == "Add" || selectedMember.name == "Exchange") &&
        selectedMember.parameters.size() == parameterOffset + 2U &&
        selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset + 1U].typeKey) &&
        selectedMember.declaredTypeKey == valueTypeKey;
    orderedRmw = orderedUnaryRmw || orderedValueRmw;
    orderedCompareExchange = selectedMember.name == "CompareExchange" &&
        selectedMember.parameters.size() == parameterOffset + 4U &&
        selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
        selectedMember.parameters[parameterOffset + 1U].typeKey == valueTypeKey &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset + 2U].typeKey) &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset + 3U].typeKey) &&
        selectedMember.declaredTypeKey == valueTypeKey;
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::atomicOperationKind(
    const ClassMemberSymbol& selectedMember,
    std::size_t parameterOffset,
    const std::string& valueTypeKey,
    bool isInt32,
    bool isPointer,
    bool orderedLoad,
    bool orderedStore,
    bool orderedRmw,
    bool orderedCompareExchange)
{
    if ((selectedMember.name == "Load" &&
            selectedMember.parameters.size() == parameterOffset &&
            selectedMember.declaredTypeKey == valueTypeKey) || orderedLoad)
        return isPointer ? MethodCoreOperationKind::AtomicLoadPointer
                         : isInt32 ? MethodCoreOperationKind::AtomicLoadInt32
                                   : MethodCoreOperationKind::AtomicLoadInt64;
    if ((selectedMember.name == "Store" &&
            selectedMember.parameters.size() == parameterOffset + 1U &&
            selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
            selectedMember.declaredTypeKey == "builtin:void") || orderedStore)
        return isPointer ? MethodCoreOperationKind::AtomicStorePointer
                         : isInt32 ? MethodCoreOperationKind::AtomicStoreInt32
                                   : MethodCoreOperationKind::AtomicStoreInt64;
    if (!isPointer &&
        ((selectedMember.name == "Increment" &&
            selectedMember.parameters.size() == parameterOffset &&
            selectedMember.declaredTypeKey == valueTypeKey) ||
         (orderedRmw && selectedMember.name == "Increment")))
        return isInt32 ? MethodCoreOperationKind::InterlockedIncrementInt32
                       : MethodCoreOperationKind::InterlockedIncrementInt64;
    if (!isPointer &&
        ((selectedMember.name == "Decrement" &&
            selectedMember.parameters.size() == parameterOffset &&
            selectedMember.declaredTypeKey == valueTypeKey) ||
         (orderedRmw && selectedMember.name == "Decrement")))
        return isInt32 ? MethodCoreOperationKind::InterlockedDecrementInt32
                       : MethodCoreOperationKind::InterlockedDecrementInt64;
    if (!isPointer &&
        ((selectedMember.name == "Add" &&
            selectedMember.parameters.size() == parameterOffset + 1U &&
            selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
            selectedMember.declaredTypeKey == valueTypeKey) ||
         (orderedRmw && selectedMember.name == "Add")))
        return isInt32 ? MethodCoreOperationKind::InterlockedAddInt32
                       : MethodCoreOperationKind::InterlockedAddInt64;
    if ((selectedMember.name == "Exchange" &&
            selectedMember.parameters.size() == parameterOffset + 1U &&
            selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
            selectedMember.declaredTypeKey == valueTypeKey) ||
        (orderedRmw && selectedMember.name == "Exchange"))
        return isPointer ? MethodCoreOperationKind::InterlockedExchangePointer
                         : isInt32 ? MethodCoreOperationKind::InterlockedExchangeInt32
                                   : MethodCoreOperationKind::InterlockedExchangeInt64;
    if ((selectedMember.name == "CompareExchange" &&
            selectedMember.parameters.size() == parameterOffset + 2U &&
            selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
            selectedMember.parameters[parameterOffset + 1U].typeKey == valueTypeKey &&
            selectedMember.declaredTypeKey == valueTypeKey) || orderedCompareExchange)
        return isPointer ? MethodCoreOperationKind::InterlockedCompareExchangePointer
                         : isInt32 ? MethodCoreOperationKind::InterlockedCompareExchangeInt32
                                   : MethodCoreOperationKind::InterlockedCompareExchangeInt64;
    return std::nullopt;
}

void ProjectMethodBodyModel::validateAtomicCompareExchangeOrders(
    std::optional<MethodCoreOperationKind>& atomicKind,
    std::optional<std::uint8_t>& atomicMemoryOrder,
    std::optional<std::uint8_t>& atomicFailureMemoryOrder,
    const ast::AstNode* expression,
    const ClassMemberSymbol& selectedMember,
    std::size_t parameterOffset,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ast::AstNode* successSyntax = expression->children().size() >= parameterOffset + 3U
        ? expression->children()[parameterOffset + 2U] : nullptr;
    const ast::AstNode* failureSyntax = expression->children().size() >= parameterOffset + 4U
        ? expression->children()[parameterOffset + 3U] : nullptr;
    const std::optional<ProjectIntegerValue> foldedSuccess =
        foldIntegerConstant(successSyntax, selectedMember.declaringTypeOrdinal, methodMemberOrdinal);
    const std::optional<ProjectIntegerValue> foldedFailure =
        foldIntegerConstant(failureSyntax, selectedMember.declaringTypeOrdinal, methodMemberOrdinal);
    if (!foldedSuccess.has_value() || !foldedFailure.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ConstRequiresConstant,
            diagnostics::DiagnosticSeverity::Error,
            !foldedSuccess.has_value() && successSyntax != nullptr
                ? successSyntax->range()
                : failureSyntax != nullptr ? failureSyntax->range() : expression->range(),
            "atomic compare-exchange orders must be compile-time MemoryOrder constants");
        atomicKind.reset();
        return;
    }
    const std::uint64_t success = foldedSuccess->bits;
    const std::uint64_t failure = foldedFailure->bits;
    const std::optional<language::MemoryOrder> successOrder =
        language::decodeMemoryOrder(success);
    const std::optional<language::MemoryOrder> failureOrder =
        language::decodeMemoryOrder(failure);
    if (!successOrder.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            successSyntax != nullptr ? successSyntax->range() : expression->range(),
            "atomic compare-exchange success order is outside MemoryOrder");
        atomicKind.reset();
    }
    else if (!failureOrder.has_value() ||
        !language::validMemoryOrder(language::AtomicOperation::Load, *failureOrder))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            failureSyntax != nullptr ? failureSyntax->range() : expression->range(),
            "atomic compare-exchange failure order must be Relaxed, Acquire, or SequentiallyConsistent");
        atomicKind.reset();
    }
    else if (!language::validCompareExchangeOrders(*successOrder, *failureOrder))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            failureSyntax != nullptr ? failureSyntax->range() : expression->range(),
            "atomic compare-exchange failure order cannot be stronger than success order");
        atomicKind.reset();
    }
    else
    {
        atomicMemoryOrder = static_cast<std::uint8_t>(success);
        atomicFailureMemoryOrder = static_cast<std::uint8_t>(failure);
    }
}

void ProjectMethodBodyModel::validateAtomicMemoryOrder(
    std::optional<MethodCoreOperationKind>& atomicKind,
    std::optional<std::uint8_t>& atomicMemoryOrder,
    const ast::AstNode* expression,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    bool orderedLoad,
    bool orderedStore,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ast::AstNode* orderSyntax =
        expression->children().empty() ? nullptr : expression->children().back();
    const std::optional<ProjectIntegerValue> foldedOrder =
        foldIntegerConstant(orderSyntax, selectedMember.declaringTypeOrdinal, methodMemberOrdinal);
    if (!foldedOrder.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ConstRequiresConstant,
            diagnostics::DiagnosticSeverity::Error,
            orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
            "atomic memory order must be a compile-time MemoryOrder constant");
        atomicKind.reset();
        return;
    }
    const std::uint64_t order = foldedOrder->bits;
    const std::optional<language::MemoryOrder> decodedOrder =
        language::decodeMemoryOrder(order);
    const language::AtomicOperation operation = orderedLoad
        ? language::AtomicOperation::Load
        : orderedStore ? language::AtomicOperation::Store
                       : language::AtomicOperation::ReadModifyWrite;
    const bool legal = decodedOrder.has_value() &&
        language::validMemoryOrder(operation, *decodedOrder);
    if (!legal)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
            orderedLoad
                ? "atomic Load order must be Relaxed, Acquire, or SequentiallyConsistent"
                : orderedStore
                    ? "atomic Store order must be Relaxed, Release, or SequentiallyConsistent"
                    : "atomic read-modify-write order is outside MemoryOrder");
        atomicKind.reset();
        return;
    }
    atomicMemoryOrder = static_cast<std::uint8_t>(order);
}

void ProjectMethodBodyModel::bindAtomicOperation(
    MethodCoreOperationKind atomicKind,
    std::optional<std::uint8_t> atomicMemoryOrder,
    std::optional<std::uint8_t> atomicFailureMemoryOrder,
    const ast::AstNode* expression,
    const ClassMemberSymbol& selectedMember,
    const std::string& valueTypeKey,
    bool atomicWrapper,
    bool publicInterlocked,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    std::optional<std::size_t> valueFieldOrdinal;
    if (atomicWrapper && memberModel_ != nullptr)
    {
        for (const std::size_t candidateOrdinal : memberModel_->findMembers(
                 selectedMember.declaringTypeOrdinal, "value_"))
        {
            if (candidateOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
            if (candidate.kind == ClassMemberKind::Field)
            {
                valueFieldOrdinal = candidateOrdinal;
                break;
            }
        }
    }
    if (!publicInterlocked && !valueFieldOrdinal.has_value()) return;
    MethodCoreBinding binding{
        atomicKind,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        selectedMember.declaredTypeKey,
        valueTypeKey,
        false, std::nullopt, std::nullopt};
    binding.atomicMemoryOrder = atomicMemoryOrder;
    binding.atomicFailureMemoryOrder = atomicFailureMemoryOrder;
    if (valueFieldOrdinal.has_value())
        binding.implicitFieldMemberOrdinal = valueFieldOrdinal;
    coreBindings_.push_back(std::move(binding));
}

void ProjectMethodBodyModel::recognizeCoreAtomicOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    ExpressionResult& result,
    diagnostics::DiagnosticEngine& diagnostics)
{
    recognizeCoreAtomicValueOperations(
        expression, declaringType, selectedMember,
        methodMemberOrdinal, scopeOrdinal, result, diagnostics);
    const auto isSharedGeneric = [&](std::string_view name) {
        return selectedMember.name == name ||
            (selectedMember.genericTemplateOrdinal.has_value() &&
             selectedMember.name.starts_with(std::string(name) + "__g"));
    };
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Internal.SharedStorage" &&
        (isSharedGeneric("InstanceSize") || isSharedGeneric("InstanceAlignment")) &&
        selectedMember.isStatic && selectedMember.declaredTypeKey == "builtin:long" &&
        selectedMember.parameters.size() == 1)
    {
        const auto targetType = referenceTypeOrdinalForTypeKey(selectedMember.parameters[0].typeKey);
        if (!targetType.has_value() || inheritanceModel_ == nullptr || !inheritanceModel_->isClass(*targetType) ||
            expression->children().size() != 1 ||
            expression->children()[0]->kind() != ast::AstNodeKind::NullLiteralExpression)
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, expression->range(),
                "shared instance layout requires a class type and a null witness");
            result.valid = false;
            return;
        }
        coreBindings_.push_back(MethodCoreBinding{
            isSharedGeneric("InstanceSize") ? MethodCoreOperationKind::SharedInstanceSize
                                             : MethodCoreOperationKind::SharedInstanceAlignment,
            expression, methodMemberOrdinal, scopeOrdinal, "builtin:long",
            selectedMember.parameters[0].typeKey, false, std::nullopt, std::nullopt});
    }
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Internal.SharedStorage" &&
        (selectedMember.name == "DestroyObject" ||
         (selectedMember.genericTemplateOrdinal.has_value() && selectedMember.name.starts_with("DestroyObject__g"))) &&
        selectedMember.isStatic && selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1)
    {
        const auto targetType = referenceTypeOrdinalForTypeKey(selectedMember.parameters[0].typeKey);
        if (!targetType.has_value() || inheritanceModel_ == nullptr || !inheritanceModel_->isClass(*targetType))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, expression->range(),
                "shared object destruction requires a class with a NativeSharp object descriptor");
            result.valid = false;
            return;
        }
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::SharedDestroyObject, expression, methodMemberOrdinal, scopeOrdinal,
            "builtin:void", selectedMember.parameters[0].typeKey, false, std::nullopt, std::nullopt});
    }
    // Item 307: static fences consume MemoryOrder during semantic analysis. The source
    // argument never becomes a runtime integer; HIR carries only the validated ABI value.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.AtomicFence" &&
        selectedMember.isStatic && selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 && !coreMemoryOrderTypeKey_.empty())
    {
        const bool isMemoryOrder =
            isMemoryOrderTypeKey(selectedMember.parameters[0].typeKey);
        std::optional<MethodCoreOperationKind> fenceKind;
        if (isMemoryOrder && selectedMember.name == "Thread")
            fenceKind = MethodCoreOperationKind::AtomicThreadFence;
        else if (isMemoryOrder && selectedMember.name == "Compiler")
            fenceKind = MethodCoreOperationKind::AtomicCompilerFence;
        if (fenceKind.has_value())
        {
            const ast::AstNode* orderSyntax = expression->children().empty()
                ? nullptr
                : expression->children()[0];
            const std::optional<ProjectIntegerValue> foldedOrder =
                foldIntegerConstant(
                    orderSyntax, selectedMember.declaringTypeOrdinal, methodMemberOrdinal);
            if (!foldedOrder.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::ConstRequiresConstant,
                    diagnostics::DiagnosticSeverity::Error,
                    orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
                    "atomic fence order must be a compile-time MemoryOrder constant");
            }
            else if (const std::optional<language::MemoryOrder> order =
                         language::decodeMemoryOrder(foldedOrder->bits);
                !order.has_value() ||
                !language::validMemoryOrder(language::AtomicOperation::Fence, *order))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
                    "atomic fence order must be Acquire, Release, AcquireRelease, or SequentiallyConsistent");
            }
            else
            {
                MethodCoreBinding binding{
                    *fenceKind, expression, methodMemberOrdinal, scopeOrdinal,
                    "builtin:void", "", false, std::nullopt, std::nullopt};
                binding.atomicMemoryOrder = static_cast<std::uint8_t>(foldedOrder->bits);
                coreBindings_.push_back(std::move(binding));
            }
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreMutexOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 54.3 "Mutex" -- NativeSharp.Core.Threading.Mutex's cuatro helpers privados y
    // estaticos. EXACTO mismo patron estructural que Thread arriba (Stage 54.1): un
    // HANDLE de Win32 guardado por valor como builtin:nint, ida y vuelta entre llamadas
    // NativeSharp separadas. MutexCreate es la PRIMERA core operation de todo el proyecto
    // sin ningun operando explicito y sin receptor -- ver el bucket "expected=0U" nuevo
    // en ProjectOopHirModel.cpp.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Mutex" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> mutexKind;
        if (selectedMember.name == "MutexCreate" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.empty())
        {
            mutexKind = MethodCoreOperationKind::MutexCreate;
        }
        else if (selectedMember.name == "MutexCreateNamed" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:string")
        {
            mutexKind = MethodCoreOperationKind::MutexCreateNamed;
        }
        else if (selectedMember.name == "MutexLock" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            mutexKind = MethodCoreOperationKind::MutexLock;
        }
        else if (selectedMember.name == "MutexUnlock" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            mutexKind = MethodCoreOperationKind::MutexUnlock;
        }
        else if (selectedMember.name == "MutexClose" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            mutexKind = MethodCoreOperationKind::MutexClose;
        }
        if (mutexKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *mutexKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreMutexMonitorOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreMutexOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // Stage 54.4 "Condition Variables" -- NativeSharp.Core.Threading.Monitor's five
    // INSTANCE methods. Same "search the declaring type's own members by field name"
    // mechanism Stage 54.2 established for AtomicInt32/AtomicInt64's `value_`, just
    // resolving TWO distinct field names (`lock_`, `cv_`) instead of one -- see
    // MethodCoreOperationKind::MonitorWait's comment in ProjectMethodBodyModel.h for why
    // Wait needs both ordinals recorded here (never re-lowering the receiver expression a
    // second time at HIR level).
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Monitor" &&
        !selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> monitorKind;
        if (selectedMember.name == "Enter" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
        {
            monitorKind = MethodCoreOperationKind::MonitorEnter;
        }
        else if (selectedMember.name == "Exit" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
        {
            monitorKind = MethodCoreOperationKind::MonitorExit;
        }
        else if (selectedMember.name == "Wait" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:int" &&
            selectedMember.declaredTypeKey == "builtin:bool")
        {
            monitorKind = MethodCoreOperationKind::MonitorWait;
        }
        else if (selectedMember.name == "Pulse" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
        {
            monitorKind = MethodCoreOperationKind::MonitorPulse;
        }
        else if (selectedMember.name == "PulseAll" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
        {
            monitorKind = MethodCoreOperationKind::MonitorPulseAll;
        }
        if (monitorKind.has_value() && memberModel_ != nullptr)
        {
            std::optional<std::size_t> lockFieldOrdinal;
            std::optional<std::size_t> cvFieldOrdinal;
            for (const std::size_t candidateOrdinal : memberModel_->findMembers(
                     selectedMember.declaringTypeOrdinal, "lock_"))
            {
                if (candidateOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                if (candidate.kind == ClassMemberKind::Field)
                    lockFieldOrdinal = candidateOrdinal;
            }
            for (const std::size_t candidateOrdinal : memberModel_->findMembers(
                     selectedMember.declaringTypeOrdinal, "cv_"))
            {
                if (candidateOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                if (candidate.kind == ClassMemberKind::Field)
                    cvFieldOrdinal = candidateOrdinal;
            }
            const bool needsBoth = *monitorKind == MethodCoreOperationKind::MonitorWait;
            const bool needsLockOnly = *monitorKind == MethodCoreOperationKind::MonitorEnter ||
                *monitorKind == MethodCoreOperationKind::MonitorExit;
            const bool ready = needsBoth
                ? (lockFieldOrdinal.has_value() && cvFieldOrdinal.has_value())
                : (needsLockOnly ? lockFieldOrdinal.has_value() : cvFieldOrdinal.has_value());
            if (ready)
            {
                MethodCoreBinding binding{
                    *monitorKind,
                    expression,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    selectedMember.declaredTypeKey,
                    selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                    false, std::nullopt, std::nullopt};
                if (needsBoth)
                {
                    // Primary = cv_ (SleepConditionVariableSRW's FIRST pointer argument),
                    // secondary = lock_ (its SECOND) -- see ProjectOopHirModel.cpp's
                    // CoreMonitorWait case for the argument-order contract this mirrors.
                    binding.implicitFieldMemberOrdinal = cvFieldOrdinal;
                    binding.secondaryImplicitFieldMemberOrdinal = lockFieldOrdinal;
                }
                else if (needsLockOnly)
                {
                    binding.implicitFieldMemberOrdinal = lockFieldOrdinal;
                }
                else
                {
                    binding.implicitFieldMemberOrdinal = cvFieldOrdinal;
                }
                coreBindings_.push_back(std::move(binding));
            }
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreSemaphoreOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 54.5 "Otras primitivas de sincronizacion" -- NativeSharp.Core.Threading.
    // Semaphore's cuatro helpers privados y estaticos, EXACTO mismo patron que Mutex
    // arriba (Stage 54.3): HANDLE de kernel via builtin:nint, cero mecanismo de
    // FieldAddress. SemaphoreWait es la UNICA diferencia real de forma: toma un SEGUNDO
    // operando (el timeout en milisegundos) y devuelve builtin:bool (true = adquirido,
    // false = timeout expirado) en vez de builtin:void -- ver el comentario de
    // MethodCoreOperationKind::SemaphoreWait en el header para el detalle completo.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Semaphore" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> semaphoreKind;
        if (selectedMember.name == "SemaphoreCreate" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:int" &&
            selectedMember.parameters[1].typeKey == "builtin:int")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreCreate;
        }
        else if (selectedMember.name == "SemaphoreCreateNamed" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == "builtin:int" &&
            selectedMember.parameters[1].typeKey == "builtin:int" &&
            selectedMember.parameters[2].typeKey == "builtin:string")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreCreateNamed;
        }
        else if (selectedMember.name == "SemaphoreWait" &&
            selectedMember.declaredTypeKey == "builtin:bool" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:nint" &&
            selectedMember.parameters[1].typeKey == "builtin:int")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreWait;
        }
        else if (selectedMember.name == "SemaphoreRelease" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreRelease;
        }
        else if (selectedMember.name == "SemaphoreClose" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreClose;
        }
        if (semaphoreKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *semaphoreKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreSemaphoreEventOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreSemaphoreOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // NativeSharp.Core.Threading.Event's cinco helpers privados y estaticos, mismo patron
    // que Semaphore arriba. EventCreate toma DOS operandos explicitos (manualReset,
    // initialState, ambos builtin:bool) sin receptor -- mismo bucket compartido
    // "expected=2U" que SemaphoreCreate ya usa. EventWait comparte la MISMA forma y el
    // MISMO runtime hook de bajo nivel que SemaphoreWait (ver WindowsRuntimeBuilder.cpp).
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Event" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> eventKind;
        if (selectedMember.name == "EventCreate" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:bool" &&
            selectedMember.parameters[1].typeKey == "builtin:bool")
        {
            eventKind = MethodCoreOperationKind::EventCreate;
        }
        else if (selectedMember.name == "EventCreateNamed" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == "builtin:bool" &&
            selectedMember.parameters[1].typeKey == "builtin:bool" &&
            selectedMember.parameters[2].typeKey == "builtin:string")
        {
            eventKind = MethodCoreOperationKind::EventCreateNamed;
        }
        else if (selectedMember.name == "EventSet" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            eventKind = MethodCoreOperationKind::EventSet;
        }
        else if (selectedMember.name == "EventReset" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            eventKind = MethodCoreOperationKind::EventReset;
        }
        else if (selectedMember.name == "EventWait" &&
            selectedMember.declaredTypeKey == "builtin:bool" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:nint" &&
            selectedMember.parameters[1].typeKey == "builtin:int")
        {
            eventKind = MethodCoreOperationKind::EventWait;
        }
        else if (selectedMember.name == "EventClose" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            eventKind = MethodCoreOperationKind::EventClose;
        }
        if (eventKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *eventKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreProcessOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 55.1 "Process" -- NativeSharp.Core.Diagnostics.Process. Five zero-argument Raw
    // getters reuse NativeClock's (53.1) exact zero-argument/builtin:long recognition
    // shape; SetPriorityClassRaw reuses Environment.Exit's (53.2) one-argument
    // (builtin:int)/void shape. Only the CURRENT process is ever addressed -- the Win32
    // HANDLE (GetCurrentProcess()) is obtained inside the runtime wrapper, never exposed
    // to NativeSharp, so no receiver/implicit-field machinery is needed here at all.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Diagnostics.Process" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> processKind;
        // Bloque 4, fase C -- SHR-032. `Run(exe, linea, salida)`: tres builtin:string
        // entran, builtin:int sale. Es la PRIMERA operacion nucleo de esta clase que
        // habla de OTRO proceso -- las cinco de Stage 55.1 son todas sobre el actual, y
        // su comentario de arriba lo dice. La forma de tres argumentos es la misma que
        // String.Substring y File.WriteAllBytes ya usan.
        if (selectedMember.name == "Run" &&
            selectedMember.declaredTypeKey == "builtin:int" &&
            selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:string" &&
            selectedMember.parameters[2].typeKey == "builtin:string")
        {
            processKind = MethodCoreOperationKind::ProcessRun;
        }
        else if (selectedMember.declaredTypeKey == "builtin:long" && selectedMember.parameters.empty())
        {
            if (selectedMember.name == "GetCurrentProcessIdRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetCurrentProcessIdRaw;
            }
            else if (selectedMember.name == "GetWorkingSetSizeRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetWorkingSetSizeRaw;
            }
            else if (selectedMember.name == "GetKernelTimeRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetKernelTimeRaw;
            }
            else if (selectedMember.name == "GetUserTimeRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetUserTimeRaw;
            }
            else if (selectedMember.name == "GetPriorityClassRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetPriorityClassRaw;
            }
        }
        else if (selectedMember.name == "SetPriorityClassRaw" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:int")
        {
            processKind = MethodCoreOperationKind::ProcessSetPriorityClassRaw;
        }
        if (processKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *processKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreDiagnosticsOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreProcessOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // Stage 55.2 "Hardware / System Info" -- NativeSharp.Core.Diagnostics.SystemInfo. All
    // six reuse the same zero-argument/builtin:long shape as Process above. Deliberately
    // does NOT duplicate Environment.ProcessorCount (53.2) -- see the enum header comment
    // in ProjectMethodBodyModel.h for the full rationale on scope.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Diagnostics.SystemInfo" &&
        selectedMember.isStatic &&
        selectedMember.declaredTypeKey == "builtin:long" &&
        selectedMember.parameters.empty())
    {
        std::optional<MethodCoreOperationKind> systemInfoKind;
        if (selectedMember.name == "GetPageSizeRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetPageSizeRaw;
        }
        else if (selectedMember.name == "GetProcessorArchitectureRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetProcessorArchitectureRaw;
        }
        else if (selectedMember.name == "GetAllocationGranularityRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetAllocationGranularityRaw;
        }
        else if (selectedMember.name == "GetTotalPhysicalMemoryBytesRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetTotalPhysicalMemoryBytesRaw;
        }
        else if (selectedMember.name == "GetAvailablePhysicalMemoryBytesRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetAvailablePhysicalMemoryBytesRaw;
        }
        else if (selectedMember.name == "GetMemoryLoadPercentRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetMemoryLoadPercentRaw;
        }
        if (systemInfoKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *systemInfoKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "builtin:long",
                std::string{},
                false, std::nullopt, std::nullopt});
        }
    }
    // Stage 55.3 "Diagnostics" -- NativeSharp.Core.Diagnostics.Debug. The two Raw getters
    // reuse the same zero-argument/builtin:long shape; OutputStringRaw reuses File.Delete's
    // (53.3) one-argument(builtin:string)/void shape, marshaled through the exact same
    // buildNullTerminatedCopy helper Environment.SetEnvironmentVariable (53.2b) introduced.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Diagnostics.Debug" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> debugKind;
        if (selectedMember.declaredTypeKey == "builtin:long" && selectedMember.parameters.empty())
        {
            if (selectedMember.name == "IsDebuggerPresentRaw")
            {
                debugKind = MethodCoreOperationKind::DebugIsDebuggerPresentRaw;
            }
            else if (selectedMember.name == "CaptureStackFrameCountRaw")
            {
                debugKind = MethodCoreOperationKind::DebugCaptureStackFrameCountRaw;
            }
        }
        else if (selectedMember.name == "OutputStringRaw" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:string")
        {
            debugKind = MethodCoreOperationKind::DebugOutputStringRaw;
        }
        if (debugKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *debugKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::applySelectedCoreOperations(
    const ast::AstNode* expression,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    MethodCallBinding& call,
    ExpressionResult& result,
    diagnostics::DiagnosticEngine& diagnostics)
{
    (void)call;
    if (nameLookup_ != nullptr && selectedMember.declaringTypeOrdinal < nameLookup_->namedTypes().size())
    {
        const NamedTypeDeclaration& declaringType = nameLookup_->namedTypes()[selectedMember.declaringTypeOrdinal];
        recognizeCoreStringOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreFloat128Operations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreInt128Operations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreScalarOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreVector256Operations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreConsoleRuntimeOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreClockEnvironmentOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreFilePathOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreThreadOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreAtomicOperations(
            expression, declaringType, selectedMember,
            methodMemberOrdinal, scopeOrdinal, result, diagnostics);
        recognizeCoreMutexMonitorOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreSemaphoreEventOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreDiagnosticsOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    }
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeAssertionIntrinsicCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // T07-F06: compiler-owned assertion family. These bindings are formed before ordinary name
    // lookup because the parser marks only unqualified contextual call heads. The argument is
    // analyzed exactly once here and later becomes the single HIR/MIR child.
    const std::string_view assertionIntrinsic = expression->auxiliaryText();
    if (assertionIntrinsic == "builtin-assert" ||
        assertionIntrinsic == "builtin-unreachable" ||
        assertionIntrinsic == "builtin-assume" ||
        assertionIntrinsic == "builtin-panic")
    {
        const bool hasCondition = assertionIntrinsic == "builtin-assert" ||
            assertionIntrinsic == "builtin-assume";
        const bool isPanic = assertionIntrinsic == "builtin-panic";
        if (!validateAssertionIntrinsicArguments(
                expression, assertionIntrinsic, hasCondition, isPanic,
                methodMemberOrdinal, scopeOrdinal, diagnostics)) return ExpressionResult{};

        MethodCoreBinding binding;
        binding.kind = assertionIntrinsic == "builtin-assert"
            ? MethodCoreOperationKind::Assert
            : assertionIntrinsic == "builtin-unreachable"
                ? MethodCoreOperationKind::Unreachable
                : assertionIntrinsic == "builtin-assume"
                    ? MethodCoreOperationKind::Assume
                    : MethodCoreOperationKind::Panic;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.resultTypeKey = hasCondition ? "builtin:void" : "builtin:never";
        coreBindings_.push_back(std::move(binding));
        result.typeKey = hasCondition ? "builtin:void" : "builtin:never";
        result.valid = true;
        if (!hasCondition) ownershipFlowReachable_ = false;
        return result;
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::validateAssertionIntrinsicArguments(
    const ast::AstNode* expression,
    std::string_view assertionIntrinsic,
    bool hasCondition,
    bool isPanic,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::string_view displayName = assertionIntrinsic.substr(8);
    const std::size_t expectedArguments = hasCondition ? 1U : 0U;
    const bool invalidArity = isPanic
        ? expression->children().size() > 1U
        : expression->children().size() != expectedArguments;
    if (invalidArity || !expression->typeSyntaxes().empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            std::string(displayName) + (isPanic
                ? "() accepts zero or one string payload and no generic type arguments"
                : "() requires exactly " + std::to_string(expectedArguments) +
                    " value argument" + (expectedArguments == 1U ? "" : "s") +
                " and no generic type arguments"));
        return false;
    }
    if (hasCondition)
    {
        const ExpressionResult condition = analyzeExpression(
            expression->children().front(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!condition.valid) return false;
        if (condition.typeKey != "builtin:bool")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCondition,
                diagnostics::DiagnosticSeverity::Error,
                expression->children().front()->range(),
                std::string(displayName) + "() condition must have type bool");
            return false;
        }
    }
    else if (isPanic && !expression->children().empty())
    {
        const ExpressionResult payload = analyzeExpression(
            expression->children().front(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!payload.valid) return false;
        if (payload.typeKey != "builtin:string" && payload.typeKey != "string-literal")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->children().front()->range(),
                "panic() payload must have type string");
            return false;
        }
    }
    return true;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeNamespaceCallHead(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // EN: T13-005. A call that may name a C++ function template: the first analysis records the types of its
    //     arguments, Clang deduces the template arguments and the bodies are analyzed again with the deduced
    //     specializations among the candidates. A local or a member of the same name shadows the template.
    // ES: T13-005. Una llamada que puede nombrar una plantilla de funcion C++: el primer analisis registra los
    //     tipos de sus argumentos, Clang deduce los argumentos de plantilla y los cuerpos se analizan otra vez
    //     con las especializaciones deducidas entre los candidatos. Un local o miembro con ese nombre la oculta.
    if (namespaceFunctionModel_ != nullptr)
        if (const CppTemplateCallSite* site = namespaceFunctionModel_->cppTemplateCallSite(expression))
        {
            const bool unqualified = expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression;
            bool shadowed = unqualified && lookupScopeName(scopeOrdinal, expression->first()->text()).has_value();
            if (unqualified && !shadowed && memberModel_ != nullptr)
                if (const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
                    caller != nullptr && caller->declaringTypeOrdinal < nameLookup_->namedTypes().size())
                    shadowed = !memberModel_->findMembers(caller->declaringTypeOrdinal, expression->first()->text()).empty();
            if (!shadowed && !site->macroReason.empty())
            {
                diagnostics.report(diagnostics::DiagnosticId::UnknownIdentifier, diagnostics::DiagnosticSeverity::Error,
                    expression->range(), "C/C++ macro '" + site->qualifiedNames.front() + "' " + site->macroReason);
                return ExpressionResult{};
            }
            if (!shadowed)
            {
                CppTemplateDeductionRequest request;
                request.syntax = expression;
                request.projectOrdinal = site->projectOrdinal;
                request.qualifiedNames = site->qualifiedNames;
                request.range = expression->range();
                request.functionMacro = site->functionMacro;
                bool valid = true;
                for (const ast::AstNode* argument : expression->children())
                {
                    // T19-040: an argument the macro takes as tokens is not analyzed. / Un argumento que la macro
                    //     toma como tokens no se analiza.
                    const auto position = static_cast<unsigned>(request.argumentTypeKeys.size());
                    // T19-042: a type argument is passed as tokens too. / Un argumento de tipo tambien se pasa como
                    //     tokens.
                    // EN: Item 5.9. A name that is not a value in scope but names an imported C/C++ type (`SIZE(Pt)`)
                    //     is a type argument too; the header spells it the same way.
                    // ES: Item 5.9. Un nombre que no es un valor en el ambito pero nombra un tipo C/C++ importado
                    //     (`SIZE(Pt)`) tambien es un argumento de tipo; el encabezado lo escribe igual.
                    const bool importedTypeName = argument != nullptr &&
                        argument->kind() == ast::AstNodeKind::IdentifierExpression &&
                        !lookupScopeName(scopeOrdinal, argument->text()).has_value() && [&] {
                            const auto& types = nameLookup_->namedTypes();
                            for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
                                if (types[ordinal].identity.name() == argument->text()) return true;
                            return false;
                        }();
                    if ((argument != nullptr && argument->kind() == ast::AstNodeKind::TypeArgumentExpression) ||
                        importedTypeName ||
                        std::find(site->tokenParameters.begin(), site->tokenParameters.end(), position) !=
                        site->tokenParameters.end() ||
                        // C2-06: every variable argument of a macro that stringifies or pastes them. / Cada
                        //     argumento variable de una macro que los convierte en cadena o los pega.
                        std::any_of(site->tokenParameters.begin(), site->tokenParameters.end(), [&](unsigned value) {
                            return value >= interop::kCppVariadicTokenParameters &&
                                position >= value - interop::kCppVariadicTokenParameters;
                        }))
                    {
                        request.argumentTypeKeys.push_back("token:");
                        continue;
                    }
                    // A `ref`, `out` or `in` argument passes its operand as an lvalue.
                    // Un argumento `ref`, `out` o `in` pasa su operando como lvalue.
                    const bool place = argument != nullptr && argument->first() != nullptr &&
                        argument->kind() == ast::AstNodeKind::UnaryExpression &&
                        std::string_view(argument->auxiliaryText()).starts_with("pass:");
                    const ExpressionResult value = analyzeExpression(
                        place ? argument->first() : argument, methodMemberOrdinal, scopeOrdinal, diagnostics);
                    valid = valid && value.valid && !value.typeKey.empty();
                    // EN: C5-02. An `sbyte` that a C function returns as `signed char` keeps that C type: `_Generic`
                    //     distinguishes it from `char`, which an `sbyte` is otherwise (item 5.5).
                    // ES: C5-02. Un `sbyte` que una funcion C devuelve como `signed char` conserva ese tipo C:
                    //     `_Generic` lo distingue de `char`, que es un `sbyte` en otro caso (item 5.5).
                    bool signedCharOrigin = false;
                    if (!place && value.typeKey == "builtin:sbyte" && argument != nullptr &&
                        namespaceFunctionModel_ != nullptr)
                        if (const auto* call = namespaceFunctionCall(argument);
                            call && call->selectedFunctionOrdinal &&
                            *call->selectedFunctionOrdinal < namespaceFunctionModel_->functions().size())
                        {
                            const auto& callee = namespaceFunctionModel_->functions()[*call->selectedFunctionOrdinal];
                            signedCharOrigin = callee.cppReturnType &&
                                callee.cppReturnType->unqualifiedCanonicalSpelling == "signed char";
                        }
                    request.argumentTypeKeys.push_back((place ? "lvalue:" : signedCharOrigin ? "csigned:" : "") +
                                                       value.typeKey);
                }
                if (valid) namespaceFunctionModel_->recordCppTemplateDeduction(std::move(request));
                // T19-003: a C/C++ function-like macro needs its expansion. / Una macro de funcion C/C++ necesita
                //     expandirse.
                diagnostics.report(diagnostics::DiagnosticId::InvalidCall, diagnostics::DiagnosticSeverity::Error,
                    expression->range(), site->functionMacro
                        ? "C/C++ function-like macro '" + site->qualifiedNames.front() +
                              "' needs its expansion for this call"
                        : "C++ function template '" + site->qualifiedNames.front() +
                              "' needs its template arguments deduced from this call");
                return ExpressionResult{};
            }
        }
    if (namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceFunctionReferenceBinding* reference =
                namespaceFunctionModel_->referenceBinding(expression);
            reference != nullptr)
        {
            bool shadowed = lookupScopeName(scopeOrdinal,
                expression->first() != nullptr ? expression->first()->text() : std::string{}).has_value();
            if (!reference->qualified && !shadowed && memberModel_ != nullptr)
            {
                const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
                if (caller != nullptr && caller->declaringTypeOrdinal < nameLookup_->namedTypes().size())
                {
                    for (const std::size_t ordinal : memberModel_->findMembers(
                             caller->declaringTypeOrdinal,
                             expression->first() != nullptr ? expression->first()->text() : std::string{}))
                    {
                        if (ordinal < memberModel_->members().size() &&
                            memberModel_->members()[ordinal].kind == ClassMemberKind::Method)
                        {
                            shadowed = true;
                            break;
                        }
                    }
                }
            }
            if (reference->qualified || !shadowed)
            {
                for (const auto& deferred : namespaceFunctionModel_->cppDeferredFunctions())
                {
                    if (!deferred.directCall || deferred.syntax != expression) continue;
                    // EN: T19-007. An overload that cannot take this many arguments is not a C++ candidate of the
                    //     call ([over.match.viable]), so it does not leave the set incomplete (`std::min(a, b)` and
                    //     `std::min(std::initializer_list<T>)`).
                    // ES: T19-007. Una sobrecarga que no admite este numero de argumentos no es candidata C++ de la
                    //     llamada ([over.match.viable]), asi que no deja incompleto el conjunto (`std::min(a, b)` y
                    //     `std::min(std::initializer_list<T>)`).
                    const std::size_t written = expression->children().size();
                    const auto& declared = deferred.declaration.parameters;
                    const std::size_t required = static_cast<std::size_t>(std::count_if(declared.begin(),
                        declared.end(), [](const auto& parameter) { return !parameter.hasDefaultArgument; }));
                    if (written < required || (!deferred.declaration.variadic && written > declared.size())) continue;
                    for (const auto ordinal : reference->candidateFunctionOrdinals)
                    {
                        if (ordinal >= namespaceFunctionModel_->functions().size()) continue;
                        const auto& function = namespaceFunctionModel_->functions()[ordinal];
                        std::string qualified;
                        for (const char c : function.namespaceName)
                            qualified += c == '.' ? "::" : std::string(1, c);
                        if (!qualified.empty()) qualified += "::";
                        qualified += function.name;
                        if (qualified != deferred.declaration.qualifiedName) continue;
                        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
                            diagnostics::DiagnosticSeverity::Error, expression->range(),
                            "C/C++ overload set is incomplete for '" + reference->writtenName +
                                "': " + deferred.reason);
                        return ExpressionResult{};
                    }
                }
                return analyzeNamespaceFunctionCall(
                    expression, *reference, methodMemberOrdinal, scopeOrdinal, diagnostics);
            }
        }
        else if (expression->first() != nullptr &&
                 expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto& name = expression->first()->text();
            bool shadowed = lookupScopeName(scopeOrdinal, name).has_value();
            if (!shadowed && memberModel_ != nullptr)
            {
                const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
                if (caller != nullptr && caller->declaringTypeOrdinal < nameLookup_->namedTypes().size())
                    for (const std::size_t ordinal : memberModel_->findMembers(caller->declaringTypeOrdinal, name))
                        if (ordinal < memberModel_->members().size() &&
                            memberModel_->members()[ordinal].kind == ClassMemberKind::Method)
                        {
                            shadowed = true;
                            break;
                        }
            }
            if (!shadowed)
            {
                for (const auto& deferred : namespaceFunctionModel_->cppDeferredFunctions())
                    if (deferred.directCall && deferred.syntax == expression &&
                        !deferred.conflictingOrigins.empty())
                    {
                        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
                            diagnostics::DiagnosticSeverity::Error, expression->range(), deferred.reason);
                        return ExpressionResult{};
                    }
                // A C/C++ function the header declares but whose signature has no NativeSharp mapping (for
                // example a floating format without a carrier) is still unknown here; say why.
                // Una función C/C++ que el encabezado declara pero cuya firma no tiene correspondencia en
                // NativeSharp (por ejemplo, un formato flotante sin portador) sigue siendo desconocida;
                // se dice por qué.
                for (const auto& deferred : namespaceFunctionModel_->cppDeferredFunctions())
                    if (deferred.directCall && deferred.syntax == expression && deferred.conflictingOrigins.empty() &&
                        !deferred.reason.empty() && deferred.declaration.name == name)
                    {
                        diagnostics.reportStructured(diagnostics::DiagnosticId::UnknownIdentifier,
                            diagnostics::DiagnosticSeverity::Error, expression->first()->range(),
                            "unknown identifier '" + name + "' in callable/initializer context: C/C++ function '" +
                                deferred.declaration.qualifiedName + "' cannot be imported: " + deferred.reason,
                            {}, cppDeclarationNotes(deferred.declaration));
                        return ExpressionResult{};
                    }
            }
        }
        else if (expression->first() != nullptr &&
                 expression->first()->kind() == ast::AstNodeKind::MemberExpression)
        {
            // EN: T14-007. A qualified call to a C/C++ function the header declares without a NativeSharp
            //     signature says why, unless a local names the first segment.
            // ES: T14-007. Una llamada calificada a una funcion C/C++ que el encabezado declara sin firma
            //     NativeSharp dice por que, salvo que un local nombre el primer segmento.
            std::vector<std::string> segments;
            const ast::AstNode* current = expression->first();
            for (; current != nullptr && current->kind() == ast::AstNodeKind::MemberExpression; current = current->first())
                segments.push_back(current->text());
            if (current != nullptr && current->kind() == ast::AstNodeKind::IdentifierExpression &&
                !lookupScopeName(scopeOrdinal, current->text()).has_value())
            {
                segments.push_back(current->text());
                std::string written;
                for (auto segment = segments.rbegin(); segment != segments.rend(); ++segment)
                    written += (written.empty() ? "" : "::") + *segment;
                for (const auto& deferred : namespaceFunctionModel_->cppDeferredFunctions())
                {
                    const auto& qualified = deferred.declaration.qualifiedName;
                    if (!deferred.directCall || deferred.syntax != expression || !deferred.conflictingOrigins.empty() ||
                        deferred.reason.empty() || deferred.declaration.name != segments.front() ||
                        qualified.size() < written.size() ||
                        qualified.compare(qualified.size() - written.size(), written.size(), written) != 0 ||
                        (qualified.size() > written.size() &&
                         qualified.compare(qualified.size() - written.size() - 2, 2, "::") != 0))
                        continue;
                    diagnostics.reportStructured(diagnostics::DiagnosticId::UnknownIdentifier,
                        diagnostics::DiagnosticSeverity::Error, expression->first()->range(),
                        "unknown identifier '" + segments.back() + "' in callable/initializer context: C/C++ function '" +
                            qualified + "' cannot be imported: " + deferred.reason,
                        {}, cppDeclarationNotes(deferred.declaration));
                    return ExpressionResult{};
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeListPatternRestCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // NS304-T03-F03: list-pattern rest capture `.. var x`. For a receiver without a
    // public Slice(int,int) -- native array, inline array, native string -- synthesize
    // the borrowed view directly. Other receivers fall through to ordinary Slice
    // method resolution below. (This first slice covers the non-generic StringView
    // case; array/inline synthesize Span<T>/ReadOnlySpan<T> in a later step.)
    if (expression->auxiliaryText() == "list-pattern-rest" &&
        expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::MemberExpression &&
        expression->first()->first() != nullptr)
    {
        ExpressionResult restReceiver = analyzeExpression(
            expression->first()->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        const bool nativeString = restReceiver.valid && !restReceiver.methodGroup &&
            (ProjectTypeConversion::isNativeString(restReceiver.typeKey) ||
             restReceiver.typeKey == "string-literal");
        if (nativeString)
        {
            if (const auto handled = analyzeNativeStringPatternRestCall(
                    expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
                handled.has_value()) return *handled;
        }
        if (const auto handled = analyzeArrayPatternRestCall(
                expression, restReceiver, methodMemberOrdinal, scopeOrdinal, diagnostics);
            handled.has_value()) return *handled;
    }

    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzePreCallKinds(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (const auto handled = analyzeAssertionIntrinsicCall(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return handled;
    if (const auto handled = analyzeNamespaceCallHead(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return handled;
    if (const auto handled = analyzeListPatternRestCall(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return handled;
    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeNativeStringPatternRestCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool argumentsValid = expression->children().size() == 2;
    for (const ast::AstNode* child : expression->children())
    {
        const ExpressionResult arg = analyzeExpression(
            child, methodMemberOrdinal, scopeOrdinal, diagnostics);
        argumentsValid = argumentsValid && arg.valid && !arg.methodGroup;
    }
    if (!argumentsValid) return std::nullopt;
    // La clave de tipo de un named type SIEMPRE lleva el modulo: `type:<modulo>::...`.
    if (coreStringViewTypeKey_.empty()) return ExpressionResult{};
    coreBindings_.push_back(MethodCoreBinding{
        MethodCoreOperationKind::StringViewFromString, expression, methodMemberOrdinal,
        scopeOrdinal, coreStringViewTypeKey_, "builtin:string",
        false, std::nullopt, std::nullopt});
    ExpressionResult viewResult;
    viewResult.typeKey = coreStringViewTypeKey_;
    viewResult.valid = true;
    viewResult.assignable = false;
    viewResult.addressable = false;
    return viewResult;
}

std::optional<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeArrayPatternRestCall(
    const ast::AstNode* expression,
    const ExpressionResult& restReceiver,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T03-F03: array nativo / inline array sin Slice publico -> sintetiza
    // Span<T> (receptor mutable) o ReadOnlySpan<T> (receptor solo lectura) sin copia.
    const std::optional<std::string> arrayElement =
        restReceiver.valid && !restReceiver.methodGroup &&
                ProjectTypeConversion::isNativeArray(restReceiver.typeKey)
            ? ProjectTypeConversion::nativeArrayElementType(restReceiver.typeKey)
            : std::nullopt;
    const std::optional<std::string> fixedElement =
        !arrayElement.has_value() && restReceiver.valid && !restReceiver.methodGroup
            ? ProjectMemberModel::fixedArrayElementTypeKey(restReceiver.typeKey)
            : std::nullopt;
    if (!arrayElement.has_value() && !fixedElement.has_value()) return std::nullopt;
    bool argumentsValid = expression->children().size() == 2;
    for (const ast::AstNode* child : expression->children())
    {
        const ExpressionResult arg = analyzeExpression(
            child, methodMemberOrdinal, scopeOrdinal, diagnostics);
        argumentsValid = argumentsValid && arg.valid && !arg.methodGroup;
    }
    if (!argumentsValid) return std::nullopt;
    const std::string& elementKey = arrayElement.has_value() ? *arrayElement : *fixedElement;
    const bool readOnly = restReceiver.readonlyReceiver;
    std::string resultKey = genericModel_ != nullptr
        ? genericModel_->closedTypeKeyForTemplate(readOnly ? "ReadOnlySpan" : "Span", elementKey)
        : std::string{};
    const bool materialized = genericModel_ != nullptr && !resultKey.empty() &&
        genericModel_->specializationForTypeKey(resultKey) != nullptr;
    if (materialized)
    {
        MethodCoreOperationKind kind = MethodCoreOperationKind::SpanFromArray;
        if (fixedElement.has_value())
            kind = readOnly ? MethodCoreOperationKind::ReadOnlySpanFromFixedArray
                            : MethodCoreOperationKind::SpanFromFixedArray;
        else if (readOnly)
            kind = MethodCoreOperationKind::ReadOnlySpanFromArray;
        coreBindings_.push_back(MethodCoreBinding{
            kind, expression, methodMemberOrdinal, scopeOrdinal, resultKey, elementKey,
            false, std::nullopt, std::nullopt});
        ExpressionResult viewResult;
        viewResult.typeKey = std::move(resultKey);
        viewResult.valid = true;
        viewResult.assignable = false;
        viewResult.addressable = false;
        return viewResult;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "array/inline rest capture requires a materialized Span<T>/ReadOnlySpan<T>; "
        "reference NativeSharp.Core, which declares the views a rest binding uses");
    return ExpressionResult{};
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeEnumRootCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // C8 "type root", struct half. The member access above resolved to a synthesized struct
    // member (see the facade at the end of analyzeMember); this is where its arity and argument
    // are checked and the core binding is recorded. Everything downstream -- HIR, MIR, LLVM --
    // ID 129. La llamada al `ToString()` sintetizado de un enum. Cero argumentos, y el tipo del
    // enum viaja en `elementTypeKey` para que el HIR pueda buscar sus enumeradores.
    if (callee.methodGroup && callee.typeKey == kEnumRootFacadeTypeKey)
    {
        if (!expression->children().empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "the compiler-provided 'ToString' on an enum takes no arguments");
            return ExpressionResult{};
        }
        // NO HAY RESTRICCION SOBRE EL RECEPTOR. La primera version de esto exigia un receptor
        // re-evaluable --la misma lista que usa la asignacion compuesta sobre propiedades-- porque
        // la bajada era una cadena de condicionales que bajaba el receptor una vez por enumerador,
        // y `Dame().ToString()` habria llamado a `Dame()` una vez por nombre.
        //
        // Eso era una limitacion del COMPILADOR escrita como si fuera una regla del LENGUAJE. La
        // bajada usa ahora un nodo propio que baja el valor UNA SOLA VEZ (ProjectOopHirModel.cpp,
        // ProjectOopHirNodeKind::EnumName), asi que la restriccion sobra y cualquier expresion de
        // tipo enum vale como receptor.
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::EnumToString,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            "builtin:string",
            callee.receiverTypeKey,
            false, std::nullopt, std::nullopt});
        ExpressionResult enumResult;
        enumResult.typeKey = "builtin:string";
        enumResult.valid = true;
        return enumResult;
    }

    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeIteratorFacadeCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // NS304-T06-F08: `it.MoveNext()`/`it.GetEnumerator()` zero-argument calls over an opaque
    // generator iterator. MoveNext returns builtin:bool; GetEnumerator returns the iterator itself.
    if (callee.methodGroup && callee.typeKey == kIteratorFacadeTypeKey)
    {
        if (!expression->children().empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "the compiler-provided iterator MoveNext/GetEnumerator takes no arguments");
            return ExpressionResult{};
        }
        const std::string& memberName = call.methodName;
        const bool isMoveNext = memberName == "MoveNext";
        const bool isDrop = memberName == "Drop";
        const bool isMoveNextAsync = memberName == "MoveNextAsync";
        const bool isCancel = memberName == "Cancel";
        // NS304-T07-F02/F03: MoveNextAsync returns the future of the next demand with the
        // async-iterator's error type (future:<bool>|<E>).
        const std::optional<std::string_view> asyncError =
            language::asyncIteratorErrorTypeKey(callee.receiverTypeKey);
        const std::string moveNextAsyncResult = language::futureTypeKey(
            "builtin:bool", asyncError.has_value() ? std::string(*asyncError) : std::string{});
        const std::string resultTypeKey = isMoveNext
            ? "builtin:bool"
            : isMoveNextAsync ? moveNextAsyncResult
            : isDrop ? "builtin:void" : isCancel ? "builtin:void" : callee.receiverTypeKey;
        // NS304-T07-F04: a `ref`/`ref mut` Current borrow lives only until the next demand or
        // destruction. Advancing or dropping the iterator while that borrow is live is rejected.
        if (isMoveNext || isMoveNextAsync || isDrop)
        {
            bool receiverPrecise = true;
            const std::optional<std::string> receiverPlace = storagePlaceKey(
                callee.receiverSyntax, scopeOrdinal, &receiverPrecise);
            if (receiverPlace.has_value() &&
                conflictingMutableReference(*receiverPlace, scopeOrdinal).has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "cannot advance or drop the iterator while a reference to its Current item is still live");
                return ExpressionResult{};
            }
            // EN: LKF-310: advancing yields a new item, so a move(it.Current) made in this scope or a
            //     nested one no longer blocks moving it. A MoveNext in a branch that does not enclose
            //     the move keeps the mark, because that branch may not run.
            // ES: LKF-310: avanzar entrega un elemento nuevo, así que un move(it.Current) hecho en este
            //     scope o en uno anidado ya no impide moverlo. Un MoveNext en una rama que no contiene
            //     al movimiento conserva la marca, porque esa rama puede no ejecutarse.
            if ((isMoveNext || isMoveNextAsync) && receiverPlace.has_value())
            {
                const auto moved = movedIteratorItems_.find(*receiverPlace);
                if (moved != movedIteratorItems_.end() && scopeIsWithin(moved->second, scopeOrdinal))
                    movedIteratorItems_.erase(moved);
            }
        }
        coreBindings_.push_back(MethodCoreBinding{
            isMoveNext ? MethodCoreOperationKind::IteratorMoveNext
                       : isMoveNextAsync ? MethodCoreOperationKind::IteratorMoveNextAsync
                       : isDrop ? MethodCoreOperationKind::IteratorDrop
                       : isCancel ? MethodCoreOperationKind::IteratorCancel
                                : MethodCoreOperationKind::IteratorGetEnumerator,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            resultTypeKey,
            callee.receiverTypeKey,
            false, std::nullopt, std::nullopt});
        ExpressionResult iteratorResult;
        iteratorResult.typeKey = resultTypeKey;
        iteratorResult.valid = true;
        return iteratorResult;
    }

    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeStructRootCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // then treats it exactly like any other core operation.
    if (callee.methodGroup && callee.typeKey == kStructRootFacadeTypeKey)
    {
        const std::string& memberName = call.methodName;
        const std::string& structTypeKey = callee.receiverTypeKey;
        std::string resultTypeKey;
        std::size_t expectedArity = 0;
        const std::optional<MethodCoreOperationKind> structKind = structRootOperation(
            memberName, structTypeKey, resultTypeKey, expectedArity);
        if (!structKind.has_value()) return ExpressionResult{};
        if (expression->children().size() != expectedArity)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "the compiler-provided '" + memberName + "' on struct '" + structTypeKey +
                    "' takes " + std::to_string(expectedArity) + " argument(s)");
            return ExpressionResult{};
        }
        // A synthesized Equals compares a struct with ITS OWN type. Widening it to take a root
        // reference would mean boxing, which this language does not do; a struct that wants to
        // compare against anything else declares its own Equals.
        if (expectedArity == 1)
        {
            const ExpressionResult other = analyzeExpression(
                expression->children().front(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!other.valid) return ExpressionResult{};
            if (other.typeKey != structTypeKey)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children().front()->range(),
                    "the compiler-provided 'Equals' on struct '" + structTypeKey +
                        "' compares against its own type, not '" + other.typeKey +
                        "'; declare an Equals of your own to compare against anything else");
                return ExpressionResult{};
            }
        }
        if (!validateStructRootOperation(
                expression, *structKind, memberName, structTypeKey, diagnostics))
            return ExpressionResult{};
        coreBindings_.push_back(MethodCoreBinding{
            *structKind,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            resultTypeKey,
            structTypeKey,
            false, std::nullopt, std::nullopt});
        result.typeKey = resultTypeKey;
        result.valid = true;
        result.assignable = false;
        return result;
    }

    return std::nullopt;
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::structRootOperation(
    std::string_view memberName,
    const std::string& structTypeKey,
    std::string& resultTypeKey,
    std::size_t& expectedArity)
{
    if (memberName == "ToString")
    {
        resultTypeKey = "builtin:string";
        return MethodCoreOperationKind::StructToString;
    }
    if (memberName == "GetHashCode")
    {
        resultTypeKey = "builtin:int";
        return MethodCoreOperationKind::StructHashCode;
    }
    if (memberName == "Equals")
    {
        resultTypeKey = "builtin:bool";
        expectedArity = 1;
        return MethodCoreOperationKind::StructEquals;
    }
    if (memberName == "Clone")
    {
        resultTypeKey = structTypeKey;
        return MethodCoreOperationKind::StructClone;
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::validateStructRootOperation(
    const ast::AstNode* expression,
    MethodCoreOperationKind structKind,
    const std::string& memberName,
    const std::string& structTypeKey,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // Equality and hashing read every scalar leaf of the value. A union has no such reading:
    // only one of its members is live at a time and nothing in the value says which. C refuses
    // to compare unions for the same reason, and guessing here would be worse than refusing.
    if (structKind != MethodCoreOperationKind::StructToString && aggregateIsUnion(structTypeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "union '" + structTypeKey + "' has no compiler-provided '" + memberName +
                "': only one member of a union is live at a time, so comparing or hashing "
                "every field would be meaningless; declare your own");
        return false;
    }
    std::string unsupportedReason;
    if (!aggregateSupportsSynthesizedMember(structTypeKey, memberName, unsupportedReason))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            unsupportedReason);
        return false;
    }
    return true;
}

std::optional<std::size_t> ProjectMethodBodyModel::callNamedStorageKey(
    const ast::AstNode* syntax,
    std::size_t scopeOrdinal) const
{
    if (syntax == nullptr || syntax->kind() != ast::AstNodeKind::IdentifierExpression)
        return std::nullopt;
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, syntax->text());
    if (!name.has_value() ||
        (name->kind != ScopeNameKind::Local && name->kind != ScopeNameKind::Parameter))
        return std::nullopt;
    return name->semanticOrdinal * 2U +
        (name->kind == ScopeNameKind::Parameter ? 1U : 0U);
}

std::optional<std::size_t> ProjectMethodBodyModel::callMarkerCase(std::string_view marker)
{
    const std::size_t separator = marker.rfind(':');
    if (separator == std::string_view::npos || separator + 1U >= marker.size())
        return std::nullopt;
    std::size_t value = 0;
    const auto [end, error] = std::from_chars(
        marker.data() + separator + 1U, marker.data() + marker.size(), value);
    return error == std::errc{} && end == marker.data() + marker.size()
        ? std::optional<std::size_t>(value) : std::nullopt;
}

void ProjectMethodBodyModel::seedCallLambdaTargets(
    std::vector<std::string>& lambdaTargetTypes,
    const ast::AstNode* expression,
    const std::vector<std::string>& parameterTypes) const
{
    if (parameterTypes.size() != expression->children().size()) return;
    for (std::size_t index = 0; index < parameterTypes.size(); ++index)
    {
        if (expression->children()[index]->kind() == ast::AstNodeKind::LambdaExpression &&
            isDelegateTypeKey(parameterTypes[index]))
        {
            lambdaTargetTypes[index] = parameterTypes[index];
        }
    }
}

void ProjectMethodBodyModel::analyzeCallArguments(
    std::vector<ExpressionResult>& arguments,
    bool& allArgumentsValid,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallBinding& call,
    std::span<const std::string> lambdaTargetTypes,
    bool coreStringForwarding,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t prefixBorrowBase = activePrefixArgumentOwnerLocals_.size();
    bool arrayArgumentEscapes = false;
    const std::size_t fieldBorrowBase = activeArgumentOwnedFields_.size();
    const std::size_t parameterFieldBorrowBase = activeArgumentParameterOwnedFields_.size();
    const MethodExpressionTypeBinding* receiverType = callee.receiverSyntax != nullptr
        ? expressionTypeBinding(callee.receiverSyntax, methodMemberOrdinal) : nullptr;
    if (receiverType != nullptr && receiverType->ownershipSourceLocalOrdinal.has_value() &&
        (receiverType->ownership == sema::OwnershipKind::Borrowed ||
         (receiverType->ownership == sema::OwnershipKind::Owned && !receiverType->ownershipTransfer)))
        activePrefixArgumentOwnerLocals_.push_back(*receiverType->ownershipSourceLocalOrdinal);
    else if (receiverType != nullptr && receiverType->ownership == sema::OwnershipKind::Borrowed)
        retainNestedPrefixBorrowOwners(*this, callee.receiverSyntax, methodMemberOrdinal,
                                      activePrefixArgumentOwnerLocals_);
    if (receiverType != nullptr && receiverType->ownership == sema::OwnershipKind::Borrowed)
        retainNestedPrefixBorrowFields(*this, callee.receiverSyntax, methodMemberOrdinal,
                                      activeArgumentOwnedFields_, activeArgumentParameterOwnedFields_);
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < expression->children().size(); ++argumentOrdinal)
    {
        const ast::AstNode* argument = expression->children()[argumentOrdinal];
        ExpressionResult argumentResult;
        if (argument->kind() == ast::AstNodeKind::LambdaExpression &&
            !lambdaTargetTypes[argumentOrdinal].empty())
        {
            argumentResult = bindLambdaToDelegate(
                argument, lambdaTargetTypes[argumentOrdinal],
                methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        else
        {
            argumentResult = analyzeExpression(
                argument, methodMemberOrdinal, scopeOrdinal, diagnostics, true, false, true);
            if (argumentResult.methodGroup && callee.methodGroup)
            {
                std::vector<std::string> targets;
                for (const std::size_t candidateOrdinal : callee.methodCandidates)
                {
                    const ClassMemberSymbol* candidate = methodSymbol(candidateOrdinal);
                    if (candidate == nullptr || candidate->kind != ClassMemberKind::Method) continue;
                    const NamedArgumentMap mapping = mapArgumentsToParameters(
                        expression->children(), *candidate, coreStringForwarding ? 1U : 0U);
                    if (!mapping.valid || argumentOrdinal >= mapping.explicitParameterOrdinals.size())
                        continue;
                    const std::size_t parameterOrdinal =
                        mapping.explicitParameterOrdinals[argumentOrdinal];
                    if (parameterOrdinal >= candidate->parameters.size()) continue;
                    const std::string& target = candidate->parameters[parameterOrdinal].typeKey;
                    if (methodGroupExactlyMatchesDelegate(argumentResult, target) &&
                        std::ranges::find(targets, target) == targets.end())
                        targets.push_back(target);
                }
                if (targets.size() == 1)
                {
                    argumentResult = bindMethodGroupToDelegate(
                        argument, argumentResult, targets.front(), methodMemberOrdinal,
                        scopeOrdinal, diagnostics);
                }
                else
                {
                    diagnostics.report(
                        targets.empty() ? diagnostics::DiagnosticId::InvalidConversion
                                        : diagnostics::DiagnosticId::InvalidCall,
                        diagnostics::DiagnosticSeverity::Error,
                        argument->range(),
                        targets.empty()
                            ? "method group does not match any delegate parameter of '" +
                                call.methodName + "'"
                            : "method group argument has more than one compatible delegate target in '" +
                                call.methodName + "'");
                }
            }
        }
        if (argumentResult.readonlyBorrowedObject)
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, argument->range(),
                "borrowed argument requires a readonly-safe parameter; readonly borrowed object "
                "cannot be passed to an unchecked function");
            allArgumentsValid = false;
        }
        if (!argumentResult.valid || argumentResult.methodGroup) allArgumentsValid = false;
        if (argumentResult.valid && argumentResult.ownershipSourceLocalOrdinal.has_value() &&
            (argumentResult.ownership == sema::OwnershipKind::Borrowed ||
             (argumentResult.ownership == sema::OwnershipKind::Owned && !argumentResult.ownershipTransfer)))
            activePrefixArgumentOwnerLocals_.push_back(*argumentResult.ownershipSourceLocalOrdinal);
        if (argumentResult.valid && argumentResult.ownership == sema::OwnershipKind::Borrowed)
            retainNestedPrefixBorrowOwners(*this, argument, methodMemberOrdinal, activePrefixArgumentOwnerLocals_);
        const std::size_t fieldsBefore = activeArgumentOwnedFields_.size();
        const std::size_t parameterFieldsBefore = activeArgumentParameterOwnedFields_.size();
        rememberDirectOwnedFieldArgument(argument, argumentResult, methodMemberOrdinal, scopeOrdinal);
        if (activeArgumentOwnedFields_.size() != fieldsBefore ||
            activeArgumentParameterOwnedFields_.size() != parameterFieldsBefore)
        {
            argumentResult.ownership = sema::OwnershipKind::Borrowed;
            argumentResult.objectLifetimeOwnership = sema::OwnershipKind::Borrowed;
            argumentResult.storageOwnership = sema::OwnershipKind::Borrowed;
            if (activeArgumentOwnedFields_.size() != fieldsBefore)
                argumentResult.ownershipSourceLocalOrdinal = activeArgumentOwnedFields_.back().first;
            if (activeArgumentParameterOwnedFields_.size() != parameterFieldsBefore)
                argumentResult.ownershipSourceParameterOrdinal = activeArgumentParameterOwnedFields_.back().first;
            recordExpressionType(argument, methodMemberOrdinal, scopeOrdinal, argumentResult);
        }
        if (argumentResult.valid && argumentResult.ownership == sema::OwnershipKind::Borrowed)
            retainNestedPrefixBorrowFields(*this, argument, methodMemberOrdinal, activeArgumentOwnedFields_,
                                          activeArgumentParameterOwnedFields_);
        // EN: Applied after this call's view checks: the array escapes during the call, not before it.
        // ES: Se aplica tras las comprobaciones de vistas de esta llamada: escapa durante ella.
        if (argumentResult.valid && ProjectTypeConversion::isNativeArray(argumentResult.typeKey))
            arrayArgumentEscapes = true;
        arguments.push_back(std::move(argumentResult));
    }
    for (const ast::AstNode* argument : expression->children())
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression) continue;
        if ((activeArgumentOwnedFields_.empty() && activeArgumentParameterOwnedFields_.empty()) ||
            !hasActiveViewOfStorage(argument, methodMemberOrdinal, scopeOrdinal, true)) continue;
        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error, argument->range(),
            "cannot pass object with a borrowed field view to a potentially mutating call");
        allArgumentsValid = false;
    }
    if ((!activeArgumentOwnedFields_.empty() || !activeArgumentParameterOwnedFields_.empty()) &&
        callee.receiverSyntax != nullptr &&
        callee.receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
        hasActiveViewOfStorage(callee.receiverSyntax, methodMemberOrdinal, scopeOrdinal, true))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error, callee.receiverSyntax->range(),
            "cannot call potentially mutating method while a borrowed field view is alive");
        allArgumentsValid = false;
    }
    // EN: Marking every array local before the checks made a freshly allocated array, passed for
    //     the first time, conflict with an older view of unknown root (NSC-NEW-018).
    // ES: Marcar cada local arreglo antes de las comprobaciones hacía que un arreglo recién creado,
    //     pasado por primera vez, chocara con una vista anterior de raíz desconocida (NSC-NEW-018).
    lastCallNewlyEscapedLocals_.clear();
    for (const ast::AstNode* argument : expression->children())
    {
        const ast::AstNode* named = argument != nullptr && argument->kind() == ast::AstNodeKind::MoveExpression
            ? argument->first() : argument;
        if (named == nullptr || named->kind() != ast::AstNodeKind::IdentifierExpression) continue;
        const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, named->text());
        if (!name.has_value() || name->kind != ScopeNameKind::Local || name->semanticOrdinal >= locals_.size())
            continue;
        MethodLocalSymbol& local = locals_[name->semanticOrdinal];
        if (ProjectTypeConversion::isNativeArray(local.typeKey) && !local.passedToCall &&
            !local.mayAliasUnknownReference)
            lastCallNewlyEscapedLocals_.push_back(name->semanticOrdinal);
        local.passedToCall = true;
    }
    if (arrayArgumentEscapes)
        for (MethodLocalSymbol& local : locals_)
            if (local.methodMemberOrdinal == methodMemberOrdinal &&
                ProjectTypeConversion::isNativeArray(local.typeKey))
                local.mayAliasUnknownReference = true;
    activePrefixArgumentOwnerLocals_.resize(prefixBorrowBase);
    activeArgumentOwnedFields_.resize(fieldBorrowBase);
    activeArgumentParameterOwnedFields_.resize(parameterFieldBorrowBase);
}

const ast::AstNode* ProjectMethodBodyModel::resolvedCallArgumentSyntax(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::size_t ordinal,
    bool coreStringForwarding) const
{
    if (coreStringForwarding)
    {
        if (ordinal == 0) return callee.receiverSyntax;
        --ordinal;
    }
    return ordinal < expression->children().size() ? expression->children()[ordinal] : nullptr;
}

bool ProjectMethodBodyModel::callIsFullExpressionBorrow(
    const ClassMemberSymbol& candidate,
    std::size_t argumentOrdinal,
    const ExpressionResult& argument) const
{
    if (!argument.valid || !argument.ownershipFreshOwner || !argument.ownershipTransfer ||
        argument.ownership != sema::OwnershipKind::Owned ||
        argumentOrdinal >= candidate.parameters.size())
    {
        return false;
    }
    if (argument.ownedPostfixTemporary &&
        candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::Borrowed)
        return true;
    if (candidate.parameters[argumentOrdinal].typeKey != "builtin:string") return false;
    if (candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::Borrowed)
        return true;
    return candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::None;
}

bool ProjectMethodBodyModel::callIsStringLiteralBorrow(
    const ClassMemberSymbol& candidate,
    std::size_t parameterOrdinal,
    const ExpressionResult& argument) noexcept
{
    return parameterOrdinal < candidate.parameters.size() &&
        candidate.parameters[parameterOrdinal].ownership == sema::OwnershipKind::Borrowed &&
        candidate.parameters[parameterOrdinal].typeKey == "builtin:string" &&
        argument.typeKey == "string-literal";
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeGenericLambdaSpecializedCall(
    const ast::AstNode* expression,
    [[maybe_unused]] const ExpressionResult& callee,
    std::vector<ExpressionResult>& arguments,
    bool allArgumentsValid,
    [[maybe_unused]] MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (expression->auxiliaryText() == "generic-lambda-specialized-call")
    {
        if (!allArgumentsValid || expression->second() == nullptr ||
            expression->typeSyntax() == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr)
            return result;
        const std::string targetType =
            memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
        const ExpressionResult specialized = bindLambdaToDelegate(
            expression->second(), targetType, methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!specialized.valid) return result;
        const auto lambda = std::ranges::find_if(
            lambdaBindings_.rbegin(), lambdaBindings_.rend(), [&](const MethodLambdaBinding& item)
            {
                return item.syntax == expression->second();
            });
        if (lambda == lambdaBindings_.rend()) return result;

        std::string returnType;
        std::vector<std::string> parameterTypes;
        std::size_t delegateOrdinal = 0;
        if (!delegateSignature(targetType, returnType, parameterTypes, delegateOrdinal)) return result;
        if (parameterTypes.size() != arguments.size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "generic lambda invocation argument count does not match its specialized signature");
            return result;
        }

        MethodDelegateBinding binding;
        binding.kind = MethodDelegateOperationKind::Invoke;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.delegateTypeKey = targetType;
        binding.delegateTypeOrdinal = delegateOrdinal;
        binding.directLambdaName = lambda->generatedName;
        binding.directLambdaUsesEnvironment = true;
        binding.resultTypeKey = returnType;
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            if (memberModel_->typeIsMoveOnly(parameterTypes[index]) &&
                arguments[index].addressable && !arguments[index].ownershipTransfer)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children()[index]->range(),
                    "move-only argument cannot be copied into a by-value generic-lambda parameter; use move(argument)");
                return result;
            }
            ProjectImplicitConversion conversion = classifyImplicit(
                arguments[index].typeKey, parameterTypes[index], arguments[index].integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children()[index]->range(),
                    "generic lambda argument is not implicitly convertible to its specialized parameter type");
                return result;
            }
            binding.argumentConversions.push_back(std::move(conversion));
        }
        delegateBindings_.push_back(std::move(binding));
        result.typeKey = returnType;
        result.valid = true;
        // EN: Freeze (plan item 135a): the delegate result may alias a lent argument temporary.
        // ES: Congelamiento (elemento 135a del plan): el resultado del delegate puede ser alias de un temporal
        //     argumento prestado.
        markDelegateCallTemporaryBorrow(result, arguments, delegateOrdinal);
        return result;
    }

    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeEventInvocationCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::vector<ExpressionResult>& arguments,
    bool allArgumentsValid,
    MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    (void)call;
    ExpressionResult result;
    if (callee.eventMemberOrdinal.has_value())
    {
        if (!allArgumentsValid || memberModel_ == nullptr ||
            *callee.eventMemberOrdinal >= memberModel_->members().size())
        {
            return result;
        }
        const ClassMemberSymbol& event = memberModel_->members()[*callee.eventMemberOrdinal];
        const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
        if (caller == nullptr || caller->declaringTypeOrdinal != event.declaringTypeOrdinal)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InaccessibleMember,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "Stage 42 events may only be invoked from within their declaring type");
            return result;
        }
        std::string returnType;
        std::vector<std::string> parameterTypes;
        std::size_t delegateOrdinal = 0;
        if (!delegateSignature(event.declaredTypeKey, returnType, parameterTypes, delegateOrdinal))
        {
            return result;
        }
        if (parameterTypes.size() != arguments.size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "event invocation argument count does not match its delegate signature");
            return result;
        }
        MethodEventBinding binding;
        binding.kind = MethodEventOperationKind::Invoke;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.eventMemberOrdinal = *callee.eventMemberOrdinal;
        const std::optional<std::size_t> backing = eventBackingMember(*callee.eventMemberOrdinal);
        if (!backing.has_value()) return result;
        binding.backingMemberOrdinal = *backing;
        binding.delegateTypeKey = event.declaredTypeKey;
        binding.delegateTypeOrdinal = delegateOrdinal;
        binding.receiverSyntax = callee.receiverSyntax;
        binding.receiverTypeKey = callee.receiverTypeKey;
        binding.receiverTypeOrdinal = callee.receiverTypeOrdinal;
        binding.staticEvent = event.isStatic;
        binding.resultTypeKey = returnType;
        if (!bindEventInvocationArguments(
                expression, arguments, parameterTypes, binding, diagnostics)) return result;
        const auto viewArguments = prepareIndirectViewArguments(expression, arguments,
            [&](std::string_view name) -> std::optional<std::string>
            {
                const auto target = lookupScopeName(scopeOrdinal, name);
                if (!target.has_value() || target->kind != ScopeNameKind::Local ||
                    target->semanticOrdinal >= locals_.size()) return std::nullopt;
                return locals_[target->semanticOrdinal].typeKey;
            },
            [&](const ast::AstNode* target) -> std::optional<std::string>
            {
                const std::string_view type = recordedExpressionTypeKey(target, methodMemberOrdinal);
                return !type.empty() && type != "<error>" ? std::optional<std::string>(type) : std::nullopt;
            });
        recordViewOutputOrigins(viewArguments.syntaxes, arguments, viewArguments.candidate,
            viewArguments.callable, scopeOrdinal, diagnostics);
        for (std::size_t index = 0;
             index < arguments.size() && index < expression->children().size(); ++index)
        {
            if (!isReferenceTypeKey(arguments[index].typeKey) &&
                !arguments[index].typeKey.ends_with("[]")) continue;
            const ast::AstNode* argument = expression->children()[index];
            if (argument == nullptr ||
                !hasActiveViewOfStorage(argument, methodMemberOrdinal, scopeOrdinal, true)) continue;
            diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error, argument->range(),
                "cannot pass object with a borrowed field view to a potentially mutating event");
            return result;
        }
        eventBindings_.push_back(std::move(binding));
        result.typeKey = returnType;
        result.valid = true;
        return result;
    }

    return std::nullopt;
}

bool ProjectMethodBodyModel::bindEventInvocationArguments(
    const ast::AstNode* expression,
    std::span<const ExpressionResult> arguments,
    std::span<const std::string> parameterTypes,
    MethodEventBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        if (memberModel_->typeIsMoveOnly(parameterTypes[index]) &&
            arguments[index].addressable && !arguments[index].ownershipTransfer)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                diagnostics::DiagnosticSeverity::Error,
                expression->children()[index]->range(),
                "move-only event argument cannot be copied into a by-value delegate parameter; use move(argument)");
            return false;
        }
        ProjectImplicitConversion conversion = classifyImplicit(
            arguments[index].typeKey, parameterTypes[index], arguments[index].integerConstant);
        if (!conversion.exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->children()[index]->range(),
                "event invocation argument is not implicitly convertible to delegate parameter type");
            return false;
        }
        binding.argumentConversions.push_back(std::move(conversion));
    }
    return true;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeDelegateInvocationCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::vector<ExpressionResult>& arguments,
    bool allArgumentsValid,
    MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (!callee.methodGroup && isDelegateTypeKey(callee.typeKey))
    {
        if (!allArgumentsValid) return result;
        std::string returnType;
        std::vector<std::string> parameterTypes;
        std::size_t delegateOrdinal = 0;
        if (!delegateSignature(callee.typeKey, returnType, parameterTypes, delegateOrdinal)) return result;
        const std::optional<language::FunctionType> structuralFunction =
            language::parseFunctionTypeIdentity(callee.typeKey);
        const bool variadic = structuralFunction.has_value() && structuralFunction->variadic;
        // EN: T08-002: pointee qualifiers of a structural callback or of a named delegate.
        // ES: T08-002: calificadores del dato apuntado de un callback estructural o de un delegate con nombre.
        const std::optional<language::FunctionType> qualifiedFunction = delegatePointeeQualifiers(callee.typeKey);
        // NS304-T04-F04: aplicacion de defaults en el call-site de delegate. Cada parametro
        // omitido debe tener un default consteval en la declaracion del delegate; su sintaxis se
        // guarda para que el HIR lo emita como argumento real.
        std::vector<const ast::AstNode*> defaultedSyntaxes;
        std::vector<std::string> defaultedTypeKeys;
        if (!collectDelegateDefaultArguments(
                defaultedSyntaxes, defaultedTypeKeys, expression, arguments.size(),
                parameterTypes, delegateOrdinal, variadic, diagnostics)) return result;
        MethodDelegateBinding binding;
        binding.kind = MethodDelegateOperationKind::Invoke;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.delegateTypeKey = callee.typeKey;
        binding.delegateTypeOrdinal = delegateOrdinal;
        if (const std::optional<language::AnonymousLambdaType> lambdaType =
                language::parseAnonymousLambdaTypeIdentity(callee.typeKey))
            binding.directLambdaName = language::anonymousLambdaGeneratedName(*lambdaType);
        binding.resultTypeKey = returnType;
        if (!bindDelegateInvocationArguments(
                expression, arguments, parameterTypes, binding, diagnostics)) return result;
        std::vector<const ast::AstNode*> possibleViewReceivers;
        if (!structuralFunction.has_value() && !isExternCDelegateTypeKey(callee.typeKey))
        {
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const auto local = lookupScopeName(scopeOrdinal, expression->first()->text());
                if (local.has_value() && local->kind == ScopeNameKind::Local &&
                    local->semanticOrdinal < locals_.size() && locals_[local->semanticOrdinal].syntax != nullptr)
                {
                    std::vector<std::size_t> pendingLocals{local->semanticOrdinal};
                    std::unordered_set<std::size_t> visitedLocals;
                    std::vector<const ast::AstNode*> possibleSources;
                    while (!pendingLocals.empty())
                    {
                        const std::size_t ordinal = pendingLocals.back();
                        pendingLocals.pop_back();
                        if (ordinal >= locals_.size() || !visitedLocals.insert(ordinal).second) continue;
                        const MethodLocalSymbol& delegateLocal = locals_[ordinal];
                        std::vector<const ast::AstNode*> sources = delegateLocal.possibleDelegateSources;
                        if (delegateLocal.syntax != nullptr) sources.push_back(delegateLocal.syntax->first());
                        while (!sources.empty())
                        {
                            const ast::AstNode* source = sources.back();
                            sources.pop_back();
                            if (source == nullptr) continue;
                            if (source->kind() == ast::AstNodeKind::ConditionalExpression)
                            {
                                sources.push_back(source->second());
                                sources.push_back(source->third());
                                continue;
                            }
                            possibleSources.push_back(source);
                            if (source->kind() != ast::AstNodeKind::IdentifierExpression) continue;
                            const MethodNameBinding* sourceBinding = nameBinding(source, methodMemberOrdinal);
                            if (sourceBinding != nullptr && sourceBinding->kind == MethodNameBindingKind::Local &&
                                sourceBinding->localOrdinal.has_value())
                                pendingLocals.push_back(*sourceBinding->localOrdinal);
                        }
                    }
                    for (const MethodLambdaBinding& lambda : lambdaBindings_)
                    {
                        if (lambda.ownerMemberOrdinal != methodMemberOrdinal ||
                            std::ranges::find(possibleSources, lambda.syntax) == possibleSources.end()) continue;
                        if (std::ranges::any_of(lambda.captures, [](const MethodLambdaCapture& capture)
                            { return capture.ownsCapturedValue; }))
                        {
                            for (const std::size_t ordinal : visitedLocals)
                            {
                                if (!hasActiveBorrowOfOwnerLocal(ordinal, scopeOrdinal)) continue;
                                diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
                                    diagnostics::DiagnosticSeverity::Error, expression->range(),
                                    "cannot invoke delegate while a borrowed local is alive");
                                return result;
                            }
                            possibleViewReceivers.push_back(expression->first());
                        }
                    }
                    for (const MethodDelegateBinding& created : delegateBindings_)
                    {
                        if (created.kind != MethodDelegateOperationKind::Create ||
                            created.methodMemberOrdinal != methodMemberOrdinal ||
                            std::ranges::find(possibleSources, created.syntax) == possibleSources.end() ||
                            created.staticTarget || created.receiverSyntax == nullptr)
                            continue;
                        possibleViewReceivers.push_back(created.receiverSyntax);
                        bool activeView = hasActiveViewOfStorage(
                            created.receiverSyntax, methodMemberOrdinal, scopeOrdinal, true);
                        const ast::AstNode* root = created.receiverSyntax;
                        while (root != nullptr && (root->kind() == ast::AstNodeKind::MemberExpression ||
                                                   root->kind() == ast::AstNodeKind::IndexExpression))
                            root = root->first();
                        const MethodNameBinding* original = root != nullptr &&
                            root->kind() == ast::AstNodeKind::IdentifierExpression
                            ? nameBinding(root, methodMemberOrdinal) : nullptr;
                        if (original != nullptr && (original->localOrdinal.has_value() ||
                                                    original->parameterOrdinal.has_value()))
                        {
                            for (const MethodLocalSymbol& view : locals_)
                            {
                                if (view.methodMemberOrdinal != methodMemberOrdinal ||
                                    !scopeContains(view.scopeOrdinal, scopeOrdinal)) continue;
                                for (const std::string& field : view.viewSourceFields)
                                {
                                    if (field.size() <= 1 || field.front() != '@') continue;
                                    const auto source = lookupScopeName(view.scopeOrdinal, std::string_view(field).substr(1));
                                    if (!source.has_value()) continue;
                                    if ((source->kind == ScopeNameKind::Local && original->localOrdinal.has_value() &&
                                         source->semanticOrdinal == *original->localOrdinal) ||
                                        (source->kind == ScopeNameKind::Parameter && original->parameterOrdinal.has_value() &&
                                         source->semanticOrdinal == *original->parameterOrdinal)) activeView = true;
                                }
                            }
                        }
                        if (!activeView) continue;
                        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
                            diagnostics::DiagnosticSeverity::Error, expression->range(),
                            "cannot invoke delegate on an object with a borrowed field view");
                        return result;
                    }
                }
            }
            for (std::size_t index = 0;
                 index < arguments.size() && index < expression->children().size(); ++index)
            {
                if (!isReferenceTypeKey(arguments[index].typeKey) &&
                    !arguments[index].typeKey.ends_with("[]")) continue;
                const ast::AstNode* argument = expression->children()[index];
                if (argument == nullptr ||
                    !hasActiveViewOfStorage(argument, methodMemberOrdinal, scopeOrdinal, true)) continue;
                diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
                    diagnostics::DiagnosticSeverity::Error, argument->range(),
                    "cannot pass object with a borrowed field view to a potentially mutating delegate");
                return result;
            }
        }
        // Delegate targets may write an input view to a mutable output just as
        // ordinary calls do. The invocation arguments already carry pass kind
        // and resolved type, including for structural callable signatures.
        const auto viewArguments = prepareIndirectViewArguments(expression, arguments,
            [&](std::string_view name) -> std::optional<std::string>
            {
                const auto target = lookupScopeName(scopeOrdinal, name);
                if (!target.has_value() || target->kind != ScopeNameKind::Local ||
                    target->semanticOrdinal >= locals_.size()) return std::nullopt;
                return locals_[target->semanticOrdinal].typeKey;
            },
            [&](const ast::AstNode* target) -> std::optional<std::string>
            {
                const std::string_view type = recordedExpressionTypeKey(target, methodMemberOrdinal);
                return !type.empty() && type != "<error>" ? std::optional<std::string>(type) : std::nullopt;
            });
        recordViewOutputOrigins(viewArguments.syntaxes, arguments, viewArguments.candidate,
            viewArguments.callable, scopeOrdinal, diagnostics);
        for (const ast::AstNode* receiver : possibleViewReceivers)
            recordViewOutputOrigins(viewArguments.syntaxes, arguments, viewArguments.candidate,
                viewArguments.callable, scopeOrdinal, diagnostics, receiver);
        rejectDelegateNewToOrdinaryParameters(expression, arguments, delegateOrdinal, diagnostics);
        // EN: LKF-286: a function type lends every parameter, so a fresh object passed to it reached no
        //     owner and leaked; the declared-delegate check above did not see structural types.
        // ES: LKF-286: un tipo función presta todos sus parámetros, así que un objeto nuevo pasado a él
        //     no llegaba a ningún dueño y se perdía; la comprobación de delegates declarados no veía los
        //     tipos estructurales.
        if (structuralFunction.has_value() || language::parseAnonymousLambdaTypeIdentity(callee.typeKey))
        {
            for (std::size_t index = 0; index < arguments.size() && index < expression->children().size(); ++index)
            {
                const ast::AstNode* argumentSyntax = expression->children()[index];
                if (arguments[index].argumentPassKind != ParameterPassKind::Value ||
                    !isFreshObjectCreation(argumentSyntax, arguments[index])) continue;
                reportLostOwnership(argumentSyntax,
                    "a parameter of a function type, which lends every parameter", diagnostics);
            }
        }
        if (expression->first() != nullptr &&
            expression->first()->kind() == ast::AstNodeKind::ConditionalExpression)
        {
            std::vector<const ast::AstNode*> pending{expression->first()};
            const ast::AstNode* sharedEnvironment = nullptr;
            bool knownSameEnvironment = true;
            bool allAlternativesKnown = true;
            bool allAlternativesConst = true;
            std::unordered_set<const ast::AstNode*> alternatives;
            while (!pending.empty())
            {
                const ast::AstNode* alternative = pending.back();
                pending.pop_back();
                if (alternative == nullptr) continue;
                if (alternative->kind() == ast::AstNodeKind::ConditionalExpression)
                {
                    const ast::AstNode* condition = alternative->first();
                    const MethodExpressionTypeBinding* conditionType = condition != nullptr
                        ? expressionTypeBinding(condition, methodMemberOrdinal) : nullptr;
                    if (conditionType != nullptr && conditionType->integerConstant.has_value())
                        pending.push_back(conditionType->integerConstant->value != 0
                            ? alternative->second() : alternative->third());
                    else if (condition != nullptr && condition->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
                        (condition->tokenKind() == TokenKind::KwTrue || condition->tokenKind() == TokenKind::KwFalse))
                        pending.push_back(condition->tokenKind() == TokenKind::KwTrue
                            ? alternative->second() : alternative->third());
                    else
                    {
                        pending.push_back(alternative->second());
                        pending.push_back(alternative->third());
                    }
                    continue;
                }
                const MethodNameBinding* name = nameBinding(alternative, methodMemberOrdinal);
                if (name == nullptr || name->kind != MethodNameBindingKind::Local ||
                    !name->localOrdinal.has_value())
                {
                    knownSameEnvironment = false;
                    allAlternativesKnown = false;
                    break;
                }
                const auto found = fnOnceEnvironments_.find(*name->localOrdinal);
                if (*name->localOrdinal >= locals_.size() || !locals_[*name->localOrdinal].isConst)
                    allAlternativesConst = false;
                if (found == fnOnceEnvironments_.end() || found->second.size() != 1)
                {
                    knownSameEnvironment = false;
                    allAlternativesKnown = false;
                    break;
                }
                const ast::AstNode* environment = *found->second.begin();
                if (sharedEnvironment != nullptr && sharedEnvironment != environment)
                    knownSameEnvironment = false;
                sharedEnvironment = environment;
                alternatives.insert(environment);
            }
            if (knownSameEnvironment && sharedEnvironment != nullptr)
            {
                if (consumedFnOnceEnvironments_.contains(sharedEnvironment))
                {
                    diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                        diagnostics::DiagnosticSeverity::Error, expression->range(),
                        "FnOnce closure selected by a conditional was already invoked or may have been invoked");
                    return result;
                }
                consumedFnOnceEnvironments_.insert(sharedEnvironment);
            }
            else if (allAlternativesKnown && allAlternativesConst && !alternatives.empty())
            {
                std::unordered_set<const ast::AstNode*> definitelyInvoked;
                for (const std::size_t ordinal : invokedFnOnceLocals_)
                {
                    if (ordinal >= locals_.size() || !locals_[ordinal].isConst) continue;
                    const auto found = fnOnceEnvironments_.find(ordinal);
                    if (found != fnOnceEnvironments_.end() && found->second.size() == 1)
                        definitelyInvoked.insert(*found->second.begin());
                }
                if (std::ranges::all_of(alternatives, [&](const ast::AstNode* environment)
                    { return definitelyInvoked.contains(environment); }))
                {
                    diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                        diagnostics::DiagnosticSeverity::Error, expression->range(),
                        "every FnOnce closure selected by this conditional was already invoked");
                    return result;
                }
            }
        }
        if (!validateCallableTraitInvocation(
                expression, callee, scopeOrdinal, diagnostics)) return result;
        binding.selectedDefaultArguments = std::move(defaultedSyntaxes);
        binding.selectedDefaultArgumentTypeKeys = std::move(defaultedTypeKeys);
        // EN: Freeze C-13: invoking a `throws E` delegate is a call to a `throws E` callable.
        // ES: Congelamiento C-13: invocar un delegate `throws E` es llamar a un callable `throws E`.
        requireDeclaredErrorHandled(
            expression, delegateErrorTypeKey(callee.typeKey),
            expression->first() != nullptr ? expression->first()->text() : std::string_view("delegate"),
            methodMemberOrdinal, diagnostics);
        delegateBindings_.push_back(std::move(binding));
        result.typeKey = returnType;
        // EN: T08-002: a C callback declared to return `volatile T*` keeps that qualification on the
        //     result, exactly like a direct call to a C function with the same return type.
        // ES: T08-002: un callback C declarado con retorno `volatile T*` conserva esa calificación en el
        //     resultado, igual que una llamada directa a una función C con el mismo tipo de retorno.
        result.pointerPointeeVolatile =
            qualifiedFunction.has_value() && qualifiedFunction->resultPointeeVolatile;
        // C1-02: and its intermediate levels. / Y sus niveles intermedios.
        result.pointerLevelQualifiers =
            qualifiedFunction.has_value() ? qualifiedFunction->resultPointerLevelQualifiers : 0U;
        result.pointerPointeeReadonly =
            qualifiedFunction.has_value() && qualifiedFunction->resultPointeeReadonly;
        // T19-038: a C callback returning `_Atomic T*`. / Un callback C que devuelve `_Atomic T*`.
        result.cppPointerPointeeAtomic =
            qualifiedFunction.has_value() && qualifiedFunction->resultPointeeAtomic;
        result.valid = true;
        // EN: Freeze (plan item 135a): the delegate result may alias a lent argument temporary.
        // ES: Congelamiento (elemento 135a del plan): el resultado del delegate puede ser alias de un temporal
        //     argumento prestado.
        markDelegateCallTemporaryBorrow(result, arguments, delegateOrdinal);
        return result;
    }

    call.candidateMemberOrdinals = callee.methodCandidates;
    return std::nullopt;
}

bool ProjectMethodBodyModel::collectDelegateDefaultArguments(
    std::vector<const ast::AstNode*>& defaultedSyntaxes,
    std::vector<std::string>& defaultedTypeKeys,
    const ast::AstNode* expression,
    std::size_t argumentCount,
    std::span<const std::string> parameterTypes,
    std::size_t delegateOrdinal,
    bool variadic,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!variadic && argumentCount < parameterTypes.size())
    {
        std::vector<const ast::AstNode*> delegateParameters;
        if (nameLookup_ != nullptr && delegateOrdinal < nameLookup_->namedTypes().size())
        {
            const ast::AstNode* delegateSyntax = nameLookup_->namedTypes()[delegateOrdinal].syntax;
            if (delegateSyntax != nullptr)
            {
                for (const ast::AstNode* child : delegateSyntax->children())
                {
                    if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
                        delegateParameters.push_back(child);
                }
            }
        }
        bool missingDefault = delegateParameters.size() != parameterTypes.size();
        for (std::size_t index = argumentCount;
             index < parameterTypes.size() && !missingDefault; ++index)
        {
            if (index >= delegateParameters.size() || delegateParameters[index]->first() == nullptr)
                missingDefault = true;
            else
            {
                defaultedSyntaxes.push_back(delegateParameters[index]->first());
                defaultedTypeKeys.push_back(parameterTypes[index]);
            }
        }
        if (missingDefault)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "delegate invocation argument count does not match its signature");
            return false;
        }
    }
    if ((!variadic && parameterTypes.size() != argumentCount + defaultedSyntaxes.size()) ||
        (variadic && argumentCount < parameterTypes.size()))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "delegate invocation argument count does not match its signature");
        return false;
    }
    return true;
}

// EN: Freeze C-01 (plan item 115): a delegate parameter declared without `owned` takes no ownership, as a
//     method parameter does. A structural function type has no declaration and is not checked here.
// ES: Congelamiento C-01 (elemento 115 del plan): un parámetro de delegate declarado sin `owned` no recibe
//     ownership, como el de un método. Un tipo función estructural no tiene declaración y no se revisa aquí.
void ProjectMethodBodyModel::rejectDelegateNewToOrdinaryParameters(
    const ast::AstNode* expression,
    std::span<const ExpressionResult> arguments,
    std::size_t delegateOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (nameLookup_ == nullptr || delegateOrdinal >= nameLookup_->namedTypes().size()) return;
    const NamedTypeDeclaration& declaration = nameLookup_->namedTypes()[delegateOrdinal];
    if (declaration.kind != NamedTypeDeclarationKind::Delegate || declaration.syntax == nullptr) return;
    std::vector<const ast::AstNode*> parameters;
    for (const ast::AstNode* child : declaration.syntax->children())
        if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration) parameters.push_back(child);
    for (std::size_t index = 0;
         index < arguments.size() && index < parameters.size() && index < expression->children().size(); ++index)
    {
        const ast::TypeSyntax* type = parameters[index]->typeSyntax();
        const ast::AstNode* argumentSyntax = expression->children()[index];
        if (type == nullptr || type->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
            arguments[index].argumentPassKind != ParameterPassKind::Value ||
            !isFreshObjectCreation(argumentSyntax, arguments[index]))
            continue;
        reportLostOwnership(
            argumentSyntax,
            "the parameter '" + parameters[index]->text() + "' of delegate '" + declaration.syntax->text() +
                "', which is not owned",
            diagnostics);
    }
}

bool ProjectMethodBodyModel::bindDelegateInvocationArguments(
    const ast::AstNode* expression,
    std::span<const ExpressionResult> arguments,
    std::span<const std::string> parameterTypes,
    MethodDelegateBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // EN: T08-002: the structural callback identity records which parameters point to volatile data.
    //     A volatile-pointee argument is accepted only by such a parameter; a parameter without the
    //     qualification, a variadic slot or a callee type without that information would discard it.
    // ES: T08-002: la identidad estructural del callback registra qué parámetros apuntan a datos
    //     volatile. Un argumento con dato apuntado volatile solo se acepta en uno de esos parámetros; un
    //     parámetro sin la calificación, una posición variádica o un tipo sin esa información la descartaría.
    const std::optional<language::FunctionType> structuralFunction =
        delegatePointeeQualifiers(binding.delegateTypeKey);
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        // T19-002: a function pointer type has no atomic pointee. / Un tipo puntero a funcion no tiene apuntado
        //     atomico.
        // EN: T19-038. A C function type does: a pointer to atomic storage binds exactly its `_Atomic T*`
        //     parameters, and only such a pointer binds them, as in C.
        // ES: T19-038. Un tipo de funcion C si lo tiene: un puntero a almacenamiento atomico enlaza exactamente sus
        //     parametros `_Atomic T*`, y solo un puntero asi los enlaza, como en C.
        const bool atomicParameter = index < parameterTypes.size() && structuralFunction.has_value() &&
            language::functionParameterPointeeAtomic(*structuralFunction, index);
        if (atomicParameter && !arguments[index].cppPointerPointeeAtomic &&
            arguments[index].typeKey != "builtin:null")
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error, expression->children()[index]->range(),
                "indirect call argument would add pointer pointee atomic qualification");
            return false;
        }
        if (arguments[index].cppPointerPointeeAtomic &&
            (!atomicParameter || arguments[index].cppAtomicPointeeHoldsVolatile))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error, expression->children()[index]->range(),
                "indirect call argument would discard pointer pointee atomic qualification");
            return false;
        }
        if (arguments[index].pointerPointeeVolatile &&
            (index >= parameterTypes.size() || !structuralFunction.has_value() ||
             !language::functionParameterPointeeVolatile(*structuralFunction, index)))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error, expression->children()[index]->range(),
                "indirect call argument would discard pointer pointee volatile qualification");
            return false;
        }
        // EN: A `const` pointee passed to a pointer parameter of a structural callback without `const`
        //     would let the callee write it; a direct call rejects the same argument. A variadic slot
        //     takes it unchanged, as in C (T08-002).
        // ES: Un dato apuntado `const` pasado a un parámetro puntero sin `const` de un callback estructural
        //     permitiría que el llamado lo escriba; una llamada directa rechaza el mismo argumento. Una
        //     posición variádica lo recibe sin cambios, como en C (T08-002).
        // EN: T19-037. An `in` or `ref` argument is a reference, not a pointer value: its parameter is by
        //     reference and keeps its own readonly rules (`InComparison<T>` in SortedList).
        // ES: T19-037. Un argumento `in` o `ref` es una referencia, no un valor puntero: su parametro es por
        //     referencia y conserva sus propias reglas de solo lectura (`InComparison<T>` en SortedList).
        if (arguments[index].pointerPointeeReadonly && structuralFunction.has_value() &&
            arguments[index].argumentPassKind == ParameterPassKind::Value &&
            index < parameterTypes.size() && ProjectTypeConversion::isPointer(parameterTypes[index]) &&
            !language::functionParameterPointeeReadonly(*structuralFunction, index))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error, expression->children()[index]->range(),
                "indirect call argument would discard pointer pointee const qualification");
            return false;
        }
        if (index >= parameterTypes.size())
        {
            ProjectImplicitConversion conversion = classifyCVariadicPromotion(arguments[index].typeKey,
                structuralFunction && (structuralFunction->callingConvention == language::CallingConvention::C ||
                                       structuralFunction->callingConvention == language::CallingConvention::SysV));
            const auto& key = arguments[index].typeKey;
            if (!conversion.exists() && structuralFunction && nameLookup_ &&
                (structuralFunction->callingConvention == language::CallingConvention::SysV ||
                 structuralFunction->callingConvention == language::CallingConvention::C) &&
                key.starts_with("type:") && !key.ends_with('*'))
            {
                const auto ordinal = nameLookup_->typeOrdinalForIdentity(std::string_view(key).substr(5));
                const auto* imported = ordinal ? nameLookup_->cppImportedType(*ordinal) : nullptr;
                if (imported && imported->origin == CppImportedTypeOrigin::CppRecord &&
                    !imported->isIncomplete && imported->deferredImportError.empty() && imported->clangUsr &&
                    imported->objectLayout && imported->cppValueTraits &&
                    imported->cppValueTraits->triviallyCopyable && imported->cppValueTraits->standardLayout &&
                    imported->cppValueTraits->trivialDestructor &&
                    !imported->cppValueTraits->hasNonLiteralOrVolatileFieldsOrBases &&
                    (structuralFunction->callingConvention != language::CallingConvention::C ||
                     (imported->cppValueOperations && interop::cppRecordValueOperationsEligible(
                         *imported->cppValueTraits, *imported->cppValueOperations))))
                    conversion = ProjectImplicitConversion{ProjectConversionKind::Identity, 10, key, key, std::nullopt};
            }
            if (!conversion.exists() ||
                arguments[index].argumentPassKind != ParameterPassKind::Value ||
                arguments[index].ownership != sema::OwnershipKind::None ||
                arguments[index].ownershipTransfer || arguments[index].ownershipFreshOwner)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children()[index]->range(),
                    "variadic function-pointer arguments must be by-value C ABI scalars, "
                    "raw pointers, enums, function pointers, or C records");
                return false;
            }
            binding.argumentConversions.push_back(std::move(conversion));
            continue;
        }
        if (memberModel_->typeIsMoveOnly(parameterTypes[index]) &&
            arguments[index].addressable && !arguments[index].ownershipTransfer)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                diagnostics::DiagnosticSeverity::Error,
                expression->children()[index]->range(),
                "move-only argument cannot be copied into a by-value delegate parameter; use move(argument)");
            return false;
        }
        ProjectImplicitConversion conversion = classifyImplicit(
            arguments[index].typeKey, parameterTypes[index], arguments[index].integerConstant);
        if (!conversion.exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->children()[index]->range(),
                "delegate invocation argument is not implicitly convertible to parameter type");
            return false;
        }
        binding.argumentConversions.push_back(std::move(conversion));
    }
    return true;
}

void ProjectMethodBodyModel::assignFnOnceEnvironments(
    std::size_t localOrdinal, const ast::AstNode* source,
    std::size_t scopeOrdinal, bool sourceIsFnOnce)
{
    std::unordered_set<const ast::AstNode*> environments;
    std::vector<const ast::AstNode*> pending{source};
    while (!pending.empty())
    {
        const ast::AstNode* current = pending.back();
        pending.pop_back();
        if (current == nullptr) continue;
        if (current->kind() == ast::AstNodeKind::ConditionalExpression)
        {
            pending.push_back(current->second());
            pending.push_back(current->third());
        }
        else if (current->kind() == ast::AstNodeKind::LambdaExpression && sourceIsFnOnce)
            environments.insert(current);
        else if (current->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto name = lookupScopeName(scopeOrdinal, current->text());
            if (!name.has_value() || name->kind != ScopeNameKind::Local) continue;
            const auto found = fnOnceEnvironments_.find(name->semanticOrdinal);
            if (found != fnOnceEnvironments_.end())
                environments.insert(found->second.begin(), found->second.end());
        }
    }
    if (environments.empty()) fnOnceEnvironments_.erase(localOrdinal);
    else fnOnceEnvironments_[localOrdinal] = std::move(environments);
}

bool ProjectMethodBodyModel::validateCallableTraitInvocation(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IdentifierExpression) return true;
    const std::optional<ScopeName> callableName = lookupScopeName(
        scopeOrdinal, expression->first()->text());
    if (!callableName.has_value() || callableName->kind != ScopeNameKind::Local ||
        callableName->semanticOrdinal >= locals_.size()) return true;
    const std::size_t localOrdinal = callableName->semanticOrdinal;
    const MethodLocalSymbol& callableLocal = locals_[localOrdinal];
    if (callee.callableTrait == language::CallableTrait::FnMut && callableLocal.isConst)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "FnMut closure local '" + callableLocal.name +
                "' cannot be invoked through a const binding");
        return false;
    }
    const auto environments = fnOnceEnvironments_.find(localOrdinal);
    if (callee.callableTrait != language::CallableTrait::FnOnce &&
        environments == fnOnceEnvironments_.end()) return true;
    bool consumedEnvironment = false;
    if (environments != fnOnceEnvironments_.end())
        for (const ast::AstNode* environment : environments->second)
            consumedEnvironment |= consumedFnOnceEnvironments_.contains(environment);
    if (consumedEnvironment || invokedFnOnceLocals_.contains(localOrdinal) ||
        maybeInvokedFnOnceLocals_.contains(localOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "FnOnce closure local '" + callableLocal.name +
                "' was already invoked or may have been invoked on another path");
        return false;
    }
    invokedFnOnceLocals_.insert(localOrdinal);
    if (environments != fnOnceEnvironments_.end())
        consumedFnOnceEnvironments_.insert(environments->second.begin(), environments->second.end());
    return true;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeSpecialCallKinds(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::vector<ExpressionResult>& arguments,
    bool allArgumentsValid,
    MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (const auto handled = analyzeGenericLambdaSpecializedCall(
            expression, callee, arguments, allArgumentsValid, call,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return handled;
    if (const auto handled = analyzeEventInvocationCall(
            expression, callee, arguments, allArgumentsValid, call,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return handled;
    if (const auto handled = analyzeDelegateInvocationCall(
            expression, callee, arguments, allArgumentsValid, call,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return handled;
    return std::nullopt;
}

void ProjectMethodBodyModel::evaluateCallCandidates(
    bool& hasMatchingArity,
    std::vector<const ast::AstNode*>& constraintRejectedSyntaxes,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    MethodCallBinding& call)
{
    hasMatchingArity = false;
    // Las candidatas que eran viables por tipos y propiedad y cayeron SOLO por su `where`. Si al
    // final no queda ninguna viable, estas son las que explican por que.
    constraintRejectedSyntaxes.clear();
    if (memberModel_ == nullptr) return;
    call.evaluatedCandidates.reserve(callee.methodCandidates.size());
    call.viableCandidateMemberOrdinals.reserve(callee.methodCandidates.size());
    for (const std::size_t candidateOrdinal : callee.methodCandidates)
    {
        if (candidateOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
        if (candidate.kind != ClassMemberKind::Method) continue;
        MethodCallCandidate candidateBinding;
        candidateBinding.memberOrdinal = candidateOrdinal;
        candidateBinding.isVariadicCandidate = candidate.isVariadic;
        if (!prepareCallCandidateMapping(
                candidateBinding, candidate, expression,
                resolvedArguments, coreStringForwarding))
        {
            call.evaluatedCandidates.push_back(std::move(candidateBinding));
            continue;
        }
        hasMatchingArity = true;
        candidateBinding.viable = true;
        evaluateCallCandidateArguments(candidateBinding, candidate, resolvedArguments);
        if (callee.readonlyReceiver && candidate.isCppExternal && candidate.declaringTypeIsStruct &&
            !candidate.isStatic && !candidate.isReadonly) candidateBinding.viable = false;
        finishCallCandidateEvaluation(
            constraintRejectedSyntaxes, candidateBinding, candidate, candidateOrdinal, call);
        call.evaluatedCandidates.push_back(std::move(candidateBinding));
    }
}

bool ProjectMethodBodyModel::prepareCallCandidateMapping(
    MethodCallCandidate& candidateBinding,
    const ClassMemberSymbol& candidate,
    const ast::AstNode* expression,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding) const
{
    const NamedArgumentMap explicitMapping = mapArgumentsToParameters(
        expression->children(), candidate, coreStringForwarding ? 1U : 0U);
    if (!explicitMapping.valid) return false;
    candidateBinding.argumentParameterOrdinals = explicitMapping.explicitParameterOrdinals;
    candidateBinding.defaultedParameterOrdinals = explicitMapping.defaultedParameterOrdinals;
    candidateBinding.parameterPackElements = explicitMapping.parameterPackElements;
    candidateBinding.parameterPackOrdinal = explicitMapping.parameterPackOrdinal;
    candidateBinding.usesExpandedParameterPack = explicitMapping.usesExpandedParameterPack;
    candidateBinding.synthesizesEmptyParameterPack = explicitMapping.synthesizesEmptyParameterPack;
    candidateBinding.defaultedParameterCount = explicitMapping.defaultedParameterOrdinals.size();
    if (coreStringForwarding)
    {
        candidateBinding.argumentParameterOrdinals.insert(
            candidateBinding.argumentParameterOrdinals.begin(), 0U);
        candidateBinding.parameterPackElements.insert(
            candidateBinding.parameterPackElements.begin(), false);
    }
    if (!candidateBinding.usesExpandedParameterPack ||
        std::ranges::count(candidateBinding.parameterPackElements, true) != 1 ||
        !candidateBinding.parameterPackOrdinal.has_value()) return true;
    const auto packElement = std::ranges::find(candidateBinding.parameterPackElements, true);
    const std::size_t argumentOrdinal = static_cast<std::size_t>(
        std::distance(candidateBinding.parameterPackElements.begin(), packElement));
    const MethodParameterSymbol& pack = candidate.parameters[*candidateBinding.parameterPackOrdinal];
    if (classifyImplicit(
            resolvedArguments[argumentOrdinal].typeKey,
            pack.typeKey,
            resolvedArguments[argumentOrdinal].integerConstant).exists())
    {
        candidateBinding.parameterPackElements[argumentOrdinal] = false;
        candidateBinding.usesExpandedParameterPack = false;
    }
    return true;
}

void ProjectMethodBodyModel::evaluateCallCandidateArguments(
    MethodCallCandidate& candidateBinding,
    const ClassMemberSymbol& candidate,
    std::span<const ExpressionResult> resolvedArguments) const
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size(); ++argumentOrdinal)
    {
        evaluateCallCandidateArgument(
            candidateBinding, candidate, resolvedArguments[argumentOrdinal], argumentOrdinal);
    }
}

void ProjectMethodBodyModel::evaluateCallCandidateArgument(
    MethodCallCandidate& candidateBinding,
    const ClassMemberSymbol& candidate,
    const ExpressionResult& argument,
    std::size_t argumentOrdinal) const
{
    const std::size_t parameterOrdinal = candidateBinding.argumentParameterOrdinals[argumentOrdinal];
    if (parameterOrdinal >= candidate.parameters.size())
    {
        ProjectImplicitConversion conversion = classifyCVariadicPromotion(argument.typeKey);
        if (!conversion.exists() || argument.pointerPointeeVolatile || argument.cppPointerPointeeAtomic ||
            argument.argumentPassKind != ParameterPassKind::Value ||
            argument.ownership != sema::OwnershipKind::None ||
            argument.ownershipTransfer || argument.ownershipFreshOwner)
        {
            candidateBinding.viable = false;
            candidateBinding.invalidVariadicArgumentOrdinal = argumentOrdinal;
        }
        candidateBinding.conversions.push_back(std::move(conversion));
        return;
    }
    const bool packElement = argumentOrdinal < candidateBinding.parameterPackElements.size() &&
        candidateBinding.parameterPackElements[argumentOrdinal];
    // T19-007: as for a namespace function, a value for a C++ `T&&`. / Como en una funcion de namespace.
    const bool rvalueArgument = !packElement && candidate.parameters[parameterOrdinal].cppRvalueReference &&
        argument.argumentPassKind == ParameterPassKind::Value;
    std::string conversionTarget = rvalueArgument ? candidate.parameters[parameterOrdinal].pointeeTypeKey
                                                  : candidate.parameters[parameterOrdinal].typeKey;
    if (packElement)
    {
        const std::string& elementType =
            candidate.parameters[parameterOrdinal].parameterPackElementTypeKey;
        if (elementType.empty()) candidateBinding.viable = false;
        else conversionTarget = elementType;
    }
    ProjectImplicitConversion conversion = classifyImportedFlexibleConversion(argument, conversionTarget);
    const MethodParameterSymbol& parameter = candidate.parameters[parameterOrdinal];
    // Items 3.2 and 3.3: the C conversions at a C/C++ boundary. / Las conversiones C en una frontera C/C++.
    if (!conversion.exists() && !packElement && (candidate.isImportedC || candidate.isCppExternal))
        conversion = classifyCFrontierConversion(argument, parameter, conversionTarget,
            candidate.isImportedC || (candidate.isCppExternal && !candidate.cppExternalSymbol.starts_with('?')));
    const bool boundLvalue = rvalueArgument && conversion.exists() &&
        conversion.kind == ProjectConversionKind::Identity && argument.addressable;
    // EN: Freeze P3: as above, an `in` parameter accepts a readonly place.
    // ES: Congelamiento P3: como arriba, un parametro `in` acepta un lugar readonly.
    const bool parameterPointeeReadonly =
        parameter.pointerPointeeReadonly || parameter.passKind == ParameterPassKind::In;
    const bool discardsPointeeConst = !packElement &&
        ((argument.pointerPointeeReadonly && ProjectTypeConversion::isPointer(parameter.typeKey) &&
          !parameterPointeeReadonly) ||
         method_body_detail::addsNestedPointeeConst(argument.typeKey, argument.pointerPointeeReadonly,
             parameter.typeKey, parameter.pointerPointeeReadonly) ||
         // C1-04: as above for the volatile base mark. / Como arriba para la marca volatile de la base.
         method_body_detail::nestedPointerQualificationError(argument.typeKey, argument.pointerLevelQualifiers,
             argument.pointerPointeeReadonly, argument.pointerPointeeVolatile, parameter.typeKey,
             parameter.pointerLevelQualifiers, parameter.pointerPointeeReadonly,
             parameter.pointerPointeeVolatile) != nullptr);
    const bool violatesPointerContract = !packElement &&
        ProjectTypeConversion::isPointer(parameter.typeKey) &&
        !language::pointerContractImplicitlyAssignable(
            pointerContract(
                // Item 8.3: a string literal is never null. / Un literal de cadena nunca es nulo.
                argument.pointerNullable && argument.typeKey != "string-literal", argument.pointerPointeeReadonly,
                argument.pointerHasProvenance, argument.pointerKnownAlignment,
                argument.pointerAddressSpace),
            pointerContract(
                parameter.pointerNullable, parameterPointeeReadonly,
                parameter.pointerHasProvenance, parameter.pointerKnownAlignment,
                parameter.pointerAddressSpace));
    // LKF-061: a `ref`/`out`/`in` written at the call site selects only parameters declared with that
    // same keyword. All three are represented as a pointer to the referent, so without this check
    // overloads that differ only in the keyword were all viable with identical conversions and the
    // call was reported as ambiguous. An argument written without a keyword is unaffected (it does
    // not convert to the pointer); a sole candidate with the wrong keyword still gets the precise
    // pass-kind diagnostic from reportTerminalCallCandidateDiagnostic.
    const bool writtenPassKindMismatch = !packElement &&
        argument.argumentPassKind != ParameterPassKind::Value &&
        parameter.passKind != argument.argumentPassKind;
    // C1-02 (item 4.7): as for a namespace function. / Como en una funcion de namespace.
    const bool decayDropsQualifier = conversion.exists() &&
        conversion.kind == ProjectConversionKind::CFlexibleArrayToPointer &&
        ProjectMemberModel::fixedArrayElementTypeKey(argument.typeKey).has_value() &&
        method_body_detail::decayedFixedArrayDropsQualifier(conversionTarget, argument.readonlyReceiver,
            argument.volatileStorage, argument.pointerLevelQualifiers, argument.pointerPointeeReadonly,
            argument.pointerPointeeVolatile, parameter.typeKey, parameter.pointerLevelQualifiers,
            parameterPointeeReadonly, parameter.pointerPointeeVolatile);
    if (!conversion.exists() || (!rvalueArgument && (discardsPointeeConst || violatesPointerContract)) ||
        boundLvalue || writtenPassKindMismatch || decayDropsQualifier ||
        (argument.pointerPointeeVolatile &&
         (packElement || !parameter.pointerPointeeVolatile)) ||
        // T19-002: as for a namespace function. / Como en una funcion de namespace.
        (ProjectTypeConversion::isPointer(parameter.typeKey) && parameter.typeKey != "builtin:void*" &&
         argument.typeKey != "builtin:null" && argument.cppPointerPointeeAtomic != parameter.cppPointerPointeeAtomic) ||
        (packElement && argument.cppPointerPointeeAtomic) || argument.cppAtomicPointeeHoldsVolatile)
        candidateBinding.viable = false;
    else if (packElement &&
        (argument.ownership != sema::OwnershipKind::None || argument.ownershipTransfer ||
         argument.ownershipFreshOwner || argument.argumentPassKind != ParameterPassKind::Value))
    {
        candidateBinding.viable = false;
        candidateBinding.invalidPackOwnershipArgumentOrdinal = argumentOrdinal;
    }
    else
    {
        const sema::OwnershipKind parameterOwnership = parameter.ownership;
        // Stage 52: an owned/borrowed qualifier written on a generic parameter
        // ("owned T", "borrowed T") monomorphizes to a real qualifier even when T
        // is closed over a plain value type (int, bool, a struct, ...) that never
        // needed ownership tracking in the first place -- there is no way to write
        // the qualifier conditionally in source. Requiring move(...)/borrow-source
        // ceremony for such a parameter would make it impossible to call a generic
        // method like "void Add(owned T value)" with a plain int argument. Treat
        // the qualifier as inert or such a closed T, exactly like `delete` on a
        // value-typed expression is already a safe no-op (see ProjectOopHirModel's
        // DeleteExpression handling) -- only a parameter whose underlying type is
        // actually ownable (a class/interface reference or native string) enforces
        // the transfer contract.
        const bool parameterOwnable = typeKeyIsOwnable(parameter.typeKey);
        // NSC-LIMIT-010: la regla vive ahora en classifyOwnershipArgument, arriba de
        // este fichero, y la comparte ProjectConstructionModel. Lo que sigue a este
        // bloque es el comentario historico de Stage 52 que explica POR QUE el orden
        // de las comprobaciones es el que es; se conserva porque esa razon no esta en
        // ningun otro sitio.
        if (!packElement && !callIsStringLiteralBorrow(candidate, parameterOrdinal, argument) &&
            !callIsFullExpressionBorrow(candidate, parameterOrdinal, argument) &&
            classifyOwnershipArgument(
                parameterOwnable, parameterOwnership, argument.ownership,
                argument.ownershipTransfer, argument.ownershipFreshOwner,
                argument.typeKey == "builtin:null") != OwnershipArgumentFit::Ok)
            candidateBinding.viable = false;
    }
    if (candidateBinding.viable && parameter.passKind == ParameterPassKind::Value &&
        memberModel_->typeIsMoveOnly(conversionTarget) && argument.addressable &&
        !argument.ownershipTransfer)
    {
        candidateBinding.viable = false;
        candidateBinding.invalidMoveOnlyArgumentOrdinal = argumentOrdinal;
    }
    candidateBinding.conversions.push_back(std::move(conversion));
}

void ProjectMethodBodyModel::finishCallCandidateEvaluation(
    std::vector<const ast::AstNode*>& constraintRejectedSyntaxes,
    MethodCallCandidate& candidateBinding,
    const ClassMemberSymbol& candidate,
    std::size_t candidateOrdinal,
    MethodCallBinding& call) const
{
    // NSC-LIMIT-007, tercera vuelta. LOS CONSTRAINTS SE CONSULTAN AQUI, y solo despues
    // de que la candidata sea viable por tipos y por propiedad.
    //
    // El orden importa: una candidata que no acepta estos argumentos no incumple nada
    // --nadie la va a llamar-- y acusarla llenaria la salida de errores sobre
    // sobrecargas que el programa descarta. Y a la inversa: si es viable, su `where` es
    // parte del contrato que la llamada tiene que cumplir, exactamente igual que los
    // tipos de sus parametros.
    //
    // El modelo de constraints guarda estas violaciones sin emitirlas precisamente
    // porque solo aqui se sabe cual de las candidatas es la que el programa usa.
    if (candidateBinding.viable && genericConstraintModel_ != nullptr &&
        !genericConstraintModel_->specializationSatisfiesConstraints(candidate.syntax))
    {
        candidateBinding.viable = false;
        constraintRejectedSyntaxes.push_back(candidate.syntax);
    }
    if (candidateBinding.viable)
        call.viableCandidateMemberOrdinals.push_back(candidateOrdinal);
}

bool ProjectMethodBodyModel::collectViableCallCandidates(
    std::vector<std::size_t>& viableEvaluationOrdinals,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> arguments,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    bool hasMatchingArity,
    std::span<const ast::AstNode* const> constraintRejectedSyntaxes,
    MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // EN: T19-006 (T17-003). The alternatives of a C++ method call, as for a namespace function.
    // ES: T19-006 (T17-003). Las alternativas de una llamada a un metodo C++, como en una funcion de namespace.
    const auto candidateNotes = [&](std::span<const std::size_t> listed, bool explainRejection) {
        if (memberModel_ == nullptr) return std::vector<std::string>{};
        return cppMemberCandidateNotes<MethodCallCandidate>(
            std::span<const ClassMemberSymbol>(memberModel_->members()),
            std::span<const MethodCallCandidate>(call.evaluatedCandidates), listed, explainRejection,
            [](const MethodCallCandidate& candidate) {
                return std::optional<std::size_t>(candidate.memberOrdinal);
            });
    };
    std::vector<std::size_t> allCandidates(call.evaluatedCandidates.size());
    for (std::size_t index = 0; index < allCandidates.size(); ++index) allCandidates[index] = index;
    if (!hasMatchingArity)
    {
        diagnostics.reportStructured(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "no overload of '" + call.methodName + "' accepts " +
                std::to_string(arguments.size()) + " argument(s)",
            {}, candidateNotes(allCandidates, true));
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }

    viableEvaluationOrdinals.clear();
    viableEvaluationOrdinals.reserve(call.viableCandidateMemberOrdinals.size());
    for (std::size_t index = 0; index < call.evaluatedCandidates.size(); ++index)
    {
        if (call.evaluatedCandidates[index].viable)
        {
            viableEvaluationOrdinals.push_back(index);
        }
    }

    if (viableEvaluationOrdinals.empty())
    {
        if (reportTerminalCallCandidateDiagnostic(
                expression, callee, coreStringForwarding,
                constraintRejectedSyntaxes, call, diagnostics)) return false;
        const bool reportedOwnershipDiagnostic = reportCallOwnershipDiagnostic(
            expression, callee, resolvedArguments, coreStringForwarding, diagnostics);
        // C4 "language references": same precision rule as the other overload-failure path --
        // when exactly one overload has the right arity and the ONLY thing wrong is the missing
        // (or extra) `ref`/`out`/`in`, say THAT. The type mismatch the programmer would otherwise
        // be shown (`int` versus `int*`) is an artifact of how a by-reference parameter is
        // represented, not something they wrote.
        const bool reportedPassKindDiagnostic = !reportedOwnershipDiagnostic &&
            reportCallPassKindDiagnostic(expression, arguments, call, diagnostics);
        if (!reportedOwnershipDiagnostic && !reportedPassKindDiagnostic)
        {
            diagnostics.reportStructured(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "no viable overload of '" + call.methodName + "' accepts the argument types",
                {}, candidateNotes(allCandidates, true));
        }
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }

    return true;
}

bool ProjectMethodBodyModel::reportTerminalCallCandidateDiagnostic(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    bool coreStringForwarding,
    std::span<const ast::AstNode* const> constraintRejectedSyntaxes,
    MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // Ninguna candidata sobrevivio. Si alguna cayo por su `where` --y no por tipos-- ese es
    // el error del programa, y se dice con el mensaje del constraint en vez de con un
    // «ninguna sobrecarga acepta estos argumentos» que no explica nada.
    if (!constraintRejectedSyntaxes.empty() && genericConstraintModel_ != nullptr)
    {
        for (const ast::AstNode* rejected : constraintRejectedSyntaxes)
            genericConstraintModel_->reportSpecializationConstraintViolations(
                rejected, expression->range(), diagnostics);
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return true;
    }
    const auto packOwnership = std::ranges::find_if(call.evaluatedCandidates,
        [](const MethodCallCandidate& candidate)
        {
            return candidate.invalidPackOwnershipArgumentOrdinal.has_value();
        });
    if (packOwnership != call.evaluatedCandidates.end())
    {
        const std::size_t ordinal = *packOwnership->invalidPackOwnershipArgumentOrdinal;
        const ast::AstNode* argument = resolvedCallArgumentSyntax(
            expression, callee, ordinal, coreStringForwarding);
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            argument != nullptr ? argument->range() : expression->range(),
            "an expanded params span only borrows its elements for the call; pass a non-owning reference, "
            "or build a Span/ReadOnlySpan over the owners and pass it as the params argument");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return true;
    }
    for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
    {
        if (candidate.invalidMoveOnlyArgumentOrdinal.has_value())
        {
            const std::size_t ordinal = *candidate.invalidMoveOnlyArgumentOrdinal;
            const ast::AstNode* argument = resolvedCallArgumentSyntax(
                expression, callee, ordinal, coreStringForwarding);
            diagnostics.report(
                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                diagnostics::DiagnosticSeverity::Error,
                argument != nullptr ? argument->range() : expression->range(),
                "move-only argument cannot be copied into a by-value parameter; use move(argument)");
            call.resolution = MethodCallResolutionKind::NoViableCandidate;
            recordCallBinding(std::move(call));
            return true;
        }
        if (!candidate.invalidVariadicArgumentOrdinal.has_value()) continue;
        const std::size_t ordinal = *candidate.invalidVariadicArgumentOrdinal;
        const ast::AstNode* argument = resolvedCallArgumentSyntax(
            expression, callee, ordinal, coreStringForwarding);
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            argument != nullptr ? argument->range() : expression->range(),
            "C variadic arguments must be by-value ABI scalars, raw pointers, enums, "
            "complete C records, or extern C delegates");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return true;
    }
    return false;
}

bool ProjectMethodBodyModel::reportCallOwnershipDiagnostic(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr) return false;
    for (const std::size_t candidateOrdinal : callee.methodCandidates)
    {
        if (candidateOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
        if (candidate.kind != ClassMemberKind::Method) continue;
        if (reportCandidateOwnershipDiagnostic(
                expression, callee, candidate, resolvedArguments,
                coreStringForwarding, diagnostics)) return true;
    }
    return false;
}

bool ProjectMethodBodyModel::reportCandidateOwnershipDiagnostic(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const ClassMemberSymbol& candidate,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const NamedArgumentMap mapping = mapArgumentsToParameters(
        expression->children(), candidate, coreStringForwarding ? 1U : 0U);
    if (!mapping.valid || mapping.explicitParameterOrdinals.size() != resolvedArguments.size()) return false;
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = mapping.explicitParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= candidate.parameters.size()) continue;
        const ProjectImplicitConversion physical = classifyImplicit(
            resolvedArguments[argumentOrdinal].typeKey,
            candidate.parameters[parameterOrdinal].typeKey,
            resolvedArguments[argumentOrdinal].integerConstant);
        if (!physical.exists()) continue;
        const ast::AstNode* argumentSyntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        if (reportArgumentOwnershipDiagnostic(
                expression, argumentSyntax, candidate.parameters[parameterOrdinal],
                resolvedArguments[argumentOrdinal], diagnostics)) return true;
    }
    return false;
}

bool ProjectMethodBodyModel::reportArgumentOwnershipDiagnostic(
    const ast::AstNode* expression,
    const ast::AstNode* argumentSyntax,
    const MethodParameterSymbol& parameter,
    const ExpressionResult& argument,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // Stage 52: mirror the same value-type bypass applied in the primary
    // candidate-matching loop above, so this diagnostic-only re-check doesn't
    // report a misleading ownership error for a generic parameter closed over
    // a plain value type.
    const bool diagnosticParameterOwnable = typeKeyIsOwnable(parameter.typeKey);
    // NSC-LIMIT-010: este re-chequeo, que existe solo para dar un mensaje bueno,
    // llevaba escrita SU PROPIA copia de la regla -- la tercera. Ahora pregunta al
    // mismo sitio que la viabilidad, asi que no puede acusar de un incumplimiento
    // que la resolucion no vio, ni callarse uno que si.
    if (parameter.ownership == sema::OwnershipKind::Owned &&
        classifyOwnershipArgument(
            diagnosticParameterOwnable, parameter.ownership, argument.ownership,
            argument.ownershipTransfer, argument.ownershipFreshOwner,
            argument.typeKey == "builtin:null") != OwnershipArgumentFit::Ok)
    {
        std::string message = "owned parameter '" + parameter.name +
            "' requires explicit ownership transfer";
        if (argumentSyntax != nullptr &&
            argumentSyntax->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            if (argument.ownership == sema::OwnershipKind::Borrowed)
                message = "cannot convert borrowed value '" + argumentSyntax->text() +
                    "' to owned parameter";
            else
                message = "cannot copy owned value '" + argumentSyntax->text() +
                    "' into owned parameter; use move(" + argumentSyntax->text() + ")";
        }
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
            std::move(message));
        return true;
    }
    if (parameter.ownership == sema::OwnershipKind::Borrowed &&
        diagnosticParameterOwnable && argument.typeKey != "string-literal" &&
        !argument.ownershipTransfer && argument.ownership == sema::OwnershipKind::None)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
            "borrowed parameter '" + parameter.name +
                "' requires a checked owner or borrowed source");
        return true;
    }
    if (argument.ownership == sema::OwnershipKind::Borrowed &&
        parameter.ownership == sema::OwnershipKind::None)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
            "borrowed argument requires a borrowed parameter; unqualified parameter '" +
                parameter.name + "' may retain the value");
        return true;
    }
    if (argument.ownershipTransfer && parameter.ownership != sema::OwnershipKind::Owned)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
            parameter.ownership == sema::OwnershipKind::Borrowed
                ? "cannot transfer ownership with move(...) into a borrowed parameter"
                : "ownership-transfer argument requires an owned parameter");
        return true;
    }
    return false;
}

bool ProjectMethodBodyModel::reportCallPassKindDiagnostic(
    const ast::AstNode* expression,
    std::span<const ExpressionResult> arguments,
    const MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr) return false;
    std::size_t arityMatches = 0;
    const MethodCallCandidate* soleCandidate = nullptr;
    for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
    {
        if (candidate.conversions.size() != arguments.size()) continue;
        ++arityMatches;
        soleCandidate = &candidate;
    }
    if (arityMatches != 1 || soleCandidate == nullptr ||
        soleCandidate->memberOrdinal >= memberModel_->members().size()) return false;
    const ClassMemberSymbol& only = memberModel_->members()[soleCandidate->memberOrdinal];
    const auto& parameterOrdinals = soleCandidate->argumentParameterOrdinals;
    if (!parameterOrdinals.empty() && parameterOrdinals.size() != arguments.size()) return false;
    bool mismatch = false;
    for (std::size_t index = 0;
         index < arguments.size() && index < only.parameters.size(); ++index)
    {
        const std::size_t parameterOrdinal = parameterOrdinals.empty() ? index : parameterOrdinals[index];
        if (parameterOrdinal < only.parameters.size() &&
            only.parameters[parameterOrdinal].passKind != arguments[index].argumentPassKind)
        {
            mismatch = true;
            break;
        }
    }
    if (!mismatch) return false;
    const std::vector<ExpressionResult> argumentValues(arguments.begin(), arguments.end());
    checkArgumentPassKinds(
        expression, call.methodName, soleCandidate->memberOrdinal, argumentValues, diagnostics, parameterOrdinals);
    return true;
}

bool ProjectMethodBodyModel::selectCallCandidate(
    std::size_t& selectedIndex,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> arguments,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool hasMatchingArity = false;
    std::vector<const ast::AstNode*> constraintRejectedSyntaxes;
    evaluateCallCandidates(
        hasMatchingArity, constraintRejectedSyntaxes, expression, callee,
        resolvedArguments, coreStringForwarding, call);
    std::vector<std::size_t> viableEvaluationOrdinals;
    if (!collectViableCallCandidates(
            viableEvaluationOrdinals, expression, callee, arguments, resolvedArguments,
            coreStringForwarding, hasMatchingArity, constraintRejectedSyntaxes,
            call, diagnostics)) return false;
    return chooseNonDominatedCallCandidate(
        selectedIndex, expression, viableEvaluationOrdinals, call, diagnostics);
}

bool ProjectMethodBodyModel::chooseNonDominatedCallCandidate(
    std::size_t& selectedIndex,
    const ast::AstNode* expression,
    std::span<const std::size_t> viableEvaluationOrdinals,
    MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<std::size_t> nonDominated;
    nonDominated.reserve(viableEvaluationOrdinals.size());
    const auto betterMemberCandidate = [&](const MethodCallCandidate& left, const MethodCallCandidate& right)
    {
        if (betterCandidate(left, right)) return true;
        if (betterCandidate(right, left)) return false;
        const auto* l = methodSymbol(left.memberOrdinal);
        const auto* r = methodSymbol(right.memberOrdinal);
        // EN: T19-006. Two C++ methods that NativeSharp ranks alike (`g(int)` and `g(long)` both take an int) are
        //     ordered by the C++ conversion of each argument to the parameters whose spellings differ, as Clang
        //     orders them; when a differing position is not arithmetic, the NativeSharp order stands.
        // ES: T19-006. Dos metodos C++ que NativeSharp ordena igual (`g(int)` y `g(long)` reciben un int) se ordenan
        //     por la conversion C++ de cada argumento a los parametros cuyas escrituras difieren, como Clang; si una
        //     posicion distinta no es aritmetica, queda el orden NativeSharp.
        if (l && r && l->isCppExternal && r->isCppExternal && l->isStatic == r->isStatic &&
            l->declaringTypeOrdinal == r->declaringTypeOrdinal && l->name == r->name &&
            l->cppParameterSpellings.size() == l->parameters.size() &&
            r->cppParameterSpellings.size() == r->parameters.size() &&
            left.argumentParameterOrdinals == right.argumentParameterOrdinals &&
            left.conversions.size() == right.conversions.size() &&
            left.conversions.size() == left.argumentParameterOrdinals.size() &&
            left.defaultedParameterCount == 0 && right.defaultedParameterCount == 0 &&
            !left.isVariadicCandidate && !right.isVariadicCandidate &&
            !left.usesExpandedParameterPack && !right.usesExpandedParameterPack)
        {
            std::vector<std::string> sources;
            for (const auto& conversion : left.conversions) sources.push_back(conversion.sourceTypeKey);
            if (const auto ranked = compareCppArithmeticSpellings(l->cppParameterSpellings, r->cppParameterSpellings,
                    left.argumentParameterOrdinals, sources)) return *ranked;
        }
        if (l && r && l->isCppExternal && r->isCppExternal && !l->isStatic && !r->isStatic &&
            l->declaringTypeOrdinal == r->declaringTypeOrdinal && l->name == r->name &&
            l->parameters.size() == 1 && r->parameters.size() == 1 &&
            left.conversions.size() == 1 && right.conversions.size() == 1 &&
            left.argumentParameterOrdinals.size() == 1 && right.argumentParameterOrdinals.size() == 1 &&
            left.argumentParameterOrdinals[0] == 0 && right.argumentParameterOrdinals[0] == 0 &&
            left.defaultedParameterCount == 0 && right.defaultedParameterCount == 0 &&
            !left.isVariadicCandidate && !right.isVariadicCandidate &&
            !left.usesExpandedParameterPack && !right.usesExpandedParameterPack)
        {
            const auto& lc = left.conversions[0];
            const auto& rc = right.conversions[0];
            if (l->parameters[0].typeKey == r->parameters[0].typeKey &&
                lc.rank == rc.rank && lc.sourceTypeKey == lc.targetTypeKey &&
                lc.targetTypeKey == rc.targetTypeKey && ProjectTypeConversion::isPointer(lc.sourceTypeKey))
            {
                const bool lp = l->parameters[0].pointerPointeeReadonly;
                const bool rp = r->parameters[0].pointerPointeeReadonly;
                return (!l->isReadonly || r->isReadonly) && (!lp || rp) &&
                    (l->isReadonly != r->isReadonly || lp != rp);
            }
        }
        if (l && r && l->isCppExternal && r->isCppExternal && !l->isStatic && !r->isStatic &&
            l->declaringTypeOrdinal == r->declaringTypeOrdinal && l->name == r->name &&
            l->parameters.size() == r->parameters.size() && l->isReadonly != r->isReadonly &&
            left.argumentParameterOrdinals == right.argumentParameterOrdinals &&
            left.defaultedParameterCount == right.defaultedParameterCount &&
            left.conversions.size() == right.conversions.size() &&
            std::equal(l->parameters.begin(), l->parameters.end(), r->parameters.begin(), [](const auto& a, const auto& b) {
                return a.typeKey == b.typeKey && a.pointerPointeeReadonly == b.pointerPointeeReadonly;
            }) && std::equal(left.conversions.begin(), left.conversions.end(), right.conversions.begin(), [](const auto& a, const auto& b) {
                return a.rank == b.rank && a.sourceTypeKey == b.sourceTypeKey && a.targetTypeKey == b.targetTypeKey;
            })) return !l->isReadonly && r->isReadonly;
        if (left.conversions.size() != 1 || right.conversions.size() != 1 ||
            left.argumentParameterOrdinals.size() != 1 || right.argumentParameterOrdinals.size() != 1 ||
            left.argumentParameterOrdinals[0] != 0 || right.argumentParameterOrdinals[0] != 0 ||
            left.defaultedParameterCount != 0 || right.defaultedParameterCount != 0 ||
            left.isVariadicCandidate || right.isVariadicCandidate ||
            left.usesExpandedParameterPack || right.usesExpandedParameterPack) return false;
        const auto& lc = left.conversions[0];
        const auto& rc = right.conversions[0];
        return l && r && l->isCppExternal && r->isCppExternal && l->isStatic == r->isStatic &&
            l->isReadonly == r->isReadonly && l->parameters.size() == 1 && r->parameters.size() == 1 &&
            lc.rank == rc.rank && lc.sourceTypeKey == lc.targetTypeKey &&
            lc.targetTypeKey == rc.targetTypeKey && ProjectTypeConversion::isPointer(lc.sourceTypeKey) &&
            !l->parameters[0].pointerPointeeReadonly && r->parameters[0].pointerPointeeReadonly;
    };
    for (const std::size_t candidateIndex : viableEvaluationOrdinals)
    {
        const bool dominated = std::ranges::any_of(
            nonDominated, [&](const std::size_t otherIndex)
        {
            return betterMemberCandidate(
                call.evaluatedCandidates[otherIndex],
                call.evaluatedCandidates[candidateIndex]);
        });
        if (dominated) continue;

        std::erase_if(nonDominated, [&](const std::size_t otherIndex)
        {
            return betterMemberCandidate(
                call.evaluatedCandidates[candidateIndex],
                call.evaluatedCandidates[otherIndex]);
        });
        nonDominated.push_back(candidateIndex);
    }

    if (nonDominated.size() != 1)
    {
        // T19-006 (T17-003): the tied C++ overloads, as notes. / Las sobrecargas C++ empatadas, como notas.
        std::vector<std::string> notes;
        if (memberModel_ != nullptr)
            notes = cppMemberCandidateNotes<MethodCallCandidate>(
                std::span<const ClassMemberSymbol>(memberModel_->members()),
                std::span<const MethodCallCandidate>(call.evaluatedCandidates), nonDominated, false,
                [](const MethodCallCandidate& candidate) {
                    return std::optional<std::size_t>(candidate.memberOrdinal);
                });
        diagnostics.reportStructured(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "ambiguous instance method call to '" + call.methodName + "'", {}, std::move(notes));
        call.resolution = MethodCallResolutionKind::Ambiguous;
        recordCallBinding(std::move(call));
        return false;
    }

    selectedIndex = nonDominated.front();
    return true;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::completeSelectedCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> arguments,
    std::vector<ExpressionResult>& resolvedArguments,
    bool coreStringForwarding,
    bool nullConditionalCall,
    std::size_t selectedIndex,
    MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    std::optional<std::size_t> provenVariantCase;
    const MethodCallCandidate& selected = call.evaluatedCandidates[selectedIndex];
    if (!validateSelectedCallReceiver(
            provenVariantCase, expression, callee, selected,
            call, scopeOrdinal, diagnostics)) return result;
    const ClassMemberSymbol* directMember = methodSymbol(selected.memberOrdinal);
    if (!coreStringForwarding && directMember != nullptr)
    {
        for (std::size_t index = 0; index < resolvedArguments.size() &&
             index < selected.argumentParameterOrdinals.size(); ++index)
        {
            const ast::AstNode* argument = resolvedCallArgumentSyntax(expression, callee, index, false);
            if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression) continue;
            const auto name = lookupScopeName(scopeOrdinal, argument->text());
            if (!name.has_value() || name->kind != ScopeNameKind::Local) continue;
            const auto environments = fnOnceEnvironments_.find(name->semanticOrdinal);
            if (environments == fnOnceEnvironments_.end()) continue;
            const auto count = directCallableParameterInvocations(
                *directMember, selected.argumentParameterOrdinals[index]);
            if (!count.has_value() || *count == 0) continue;
            bool consumed = *count > 1;
            for (const ast::AstNode* environment : environments->second)
                consumed |= consumedFnOnceEnvironments_.contains(environment);
            if (consumed)
            {
                diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error, argument->range(),
                    "FnOnce closure may be invoked more than once through a parameter");
                call.resolution = MethodCallResolutionKind::NoViableCandidate;
                recordCallBinding(std::move(call));
                return result;
            }
            consumedFnOnceEnvironments_.insert(environments->second.begin(), environments->second.end());
        }
    }
    // EN: Escape marks this call's own argument list set describe what happens during the call, not
    //     before it; lift them for this check (NSC-NEW-018).
    // ES: Las marcas de escape que fijó la lista de argumentos de esta llamada describen lo que ocurre
    //     durante ella, no antes; se retiran para esta comprobación (NSC-NEW-018).
    const std::vector<std::size_t> newlyEscaped = std::move(lastCallNewlyEscapedLocals_);
    lastCallNewlyEscapedLocals_.clear();
    const auto activeViewBeforeThisCall = [&](const ast::AstNode* argument)
    {
        for (const std::size_t ordinal : newlyEscaped)
        {
            locals_[ordinal].passedToCall = false;
            locals_[ordinal].mayAliasUnknownReference = false;
        }
        const bool active = hasActiveViewOfStorage(argument, methodMemberOrdinal, scopeOrdinal, true);
        for (const std::size_t ordinal : newlyEscaped)
        {
            locals_[ordinal].passedToCall = true;
            locals_[ordinal].mayAliasUnknownReference = true;
        }
        return active;
    };
    for (std::size_t index = 0;
         index < resolvedArguments.size() && index < selected.argumentParameterOrdinals.size(); ++index)
    {
        if (!isReferenceTypeKey(resolvedArguments[index].typeKey) &&
            !resolvedArguments[index].typeKey.ends_with("[]")) continue;
        const ast::AstNode* argument = resolvedCallArgumentSyntax(expression, callee, index, coreStringForwarding);
        if (argument == nullptr || !activeViewBeforeThisCall(argument)) continue;
        // EN: A source callee is judged after every body is analyzed (finalizeStorageViewCalls).
        // ES: Un callee de fuente se juzga al terminar todos los cuerpos (finalizeStorageViewCalls).
        if (deferStorageViewCall(selected.memberOrdinal, argument->range(),
                "cannot pass object with a borrowed field view to a potentially mutating call")) continue;
        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error, argument->range(),
            "cannot pass object with a borrowed field view to a potentially mutating call");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return result;
    }
    const std::vector<ExpressionResult> argumentValues(arguments.begin(), arguments.end());
    checkArgumentPassKinds(
        expression, call.methodName, selected.memberOrdinal, argumentValues, diagnostics,
        std::span<const std::size_t>(selected.argumentParameterOrdinals).subspan(
            coreStringForwarding ? 1U : 0U));
    invalidateCallArgumentFlow(
        expression, callee, resolvedArguments, selected, coreStringForwarding, scopeOrdinal);
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedMemberOrdinal = selected.memberOrdinal;
    updateScopedChildBorrowsForCall(
        expression, selected, call, methodMemberOrdinal, scopeOrdinal, diagnostics);
    // EN: LKF-310: MoveNext advances the iterator to a new item. A move(it.Current) made in this scope
    //     or a nested one no longer blocks moving the new item; a MoveNext inside a branch the move
    //     does not enclose leaves the mark, because that branch may not run.
    // ES: LKF-310: MoveNext avanza el iterador a un elemento nuevo. Un move(it.Current) hecho en este
    //     scope o en uno anidado ya no impide mover el elemento nuevo; un MoveNext dentro de una rama
    //     que no contiene al movimiento conserva la marca, porque esa rama puede no ejecutarse.
    if ((call.methodName == "MoveNext" || call.methodName == "MoveNextAsync") &&
        callee.receiverSyntax != nullptr && !movedIteratorItems_.empty())
    {
        const std::optional<std::string> place = storagePlaceKey(callee.receiverSyntax, scopeOrdinal);
        const auto moved = place.has_value() ? movedIteratorItems_.find(*place) : movedIteratorItems_.end();
        if (moved != movedIteratorItems_.end() && scopeIsWithin(moved->second, scopeOrdinal))
            movedIteratorItems_.erase(moved);
    }
    configureSelectedCallBinding(
        result, expression, callee, selected, coreStringForwarding, call);
    if (memberModel_ != nullptr && selected.memberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
        recordViewOutputOrigins(call.selectedArgumentSyntaxes, resolvedArguments,
            selected, selectedMember, scopeOrdinal, diagnostics,
            !selectedMember.isStatic && !coreStringForwarding ? callee.receiverSyntax : nullptr);
        recordSelectedOutArguments(
            expression, callee, resolvedArguments, selected, selectedMember,
            coreStringForwarding, methodMemberOrdinal, scopeOrdinal);
        recordSelectedOwnershipTransfers(
            expression, callee, resolvedArguments, selected, selectedMember,
            coreStringForwarding, methodMemberOrdinal, scopeOrdinal);
        recordSelectedBorrows(
            expression, callee, resolvedArguments, selected, selectedMember,
            coreStringForwarding, methodMemberOrdinal, scopeOrdinal);
        rejectSelectedNewToOrdinaryParameters(
            expression, callee, resolvedArguments, selected, selectedMember, coreStringForwarding, diagnostics);
        rejectNonTrivialImportedCArguments(
            expression, callee, resolvedArguments, selected, selectedMember, coreStringForwarding, diagnostics);
        rejectNewCallReceiver(callee, selectedMember, coreStringForwarding, diagnostics);
        applySelectedCoreOperations(
            expression, selectedMember, methodMemberOrdinal, scopeOrdinal, call, result, diagnostics);
        // EN: Freeze C-13: an async callee reports its error at the await, not at the call.
        // ES: Congelamiento C-13: un callee async reporta su error en el await, no en la llamada.
        if (!selectedMember.effects.contains(sema::Effect::Async))
            requireDeclaredErrorHandled(
                expression, selectedMember.declaredErrorTypeKey, selectedMember.name,
                methodMemberOrdinal, diagnostics);
    }
    finalizeSelectedCallResult(
        result, expression, call, nullConditionalCall, provenVariantCase, diagnostics);
    // EN: Freeze (plan item 135a): the parameters that take ownership do not lend their argument.
    // ES: Congelamiento (elemento 135a del plan): los parametros que toman ownership no prestan su argumento.
    if (memberModel_ != nullptr && selected.memberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& selectedCallable = memberModel_->members()[selected.memberOrdinal];
        markCallTemporaryBorrow(
            result, callee.receiverBorrowsOwnedTemporary, resolvedArguments,
            ownedArgumentParameters(selected.argumentParameterOrdinals, selectedCallable));
    }
    recordCallBinding(std::move(call));
    return result;
}

// EN: Freeze (plan item 135). A reference or string result that is not a new owner may point into the
//     receiver or into an argument, so it keeps their borrow of an owned temporary (`Make().GetName()`,
//     `Id(Make().Name)`). The rule is conservative: the analysis does not know which input the result came from.
// ES: Congelamiento (elemento 135 del plan). Un resultado referencia o string que no es un owner nuevo puede
//     apuntar dentro del receptor o de un argumento, asi que conserva su prestamo de un temporal owned
//     (`Make().GetName()`, `Id(Make().Name)`). La regla es conservadora: el analisis no sabe de que entrada vino
//     el resultado.
//
// EN: Freeze (plan item 135a): an argument also lends when it is itself an owned temporary of any reference
//     type handed to a parameter that does not take ownership (`Id(MakeText())`); the call keeps it only until
//     the end of the full expression. The same rule covers method, namespace function and delegate calls.
// ES: Congelamiento (elemento 135a del plan): un argumento tambien presta cuando es el propio temporal owned,
//     de cualquier tipo referencia, entregado a un parametro que no toma ownership (`Id(MakeText())`); la llamada
//     lo conserva solo hasta el final de la expresion completa. La misma regla cubre llamadas a metodos, a
//     funciones de namespace y a delegates.
void ProjectMethodBodyModel::markCallTemporaryBorrow(
    ExpressionResult& result,
    bool receiverBorrows,
    std::span<const ExpressionResult> arguments,
    const std::vector<bool>& ownedParameters) const
{
    if (!result.valid) return;
    if (!isReferenceTypeKey(result.typeKey) && result.typeKey != "builtin:string") return;
    if (result.ownership == sema::OwnershipKind::Owned && result.ownershipTransfer) return;
    bool borrows = receiverBorrows;
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const bool ownedParameter = index < ownedParameters.size() && ownedParameters[index];
        const bool lentTemporary = !ownedParameter && isOwnedTemporaryValue(arguments[index]);
        borrows = borrows || arguments[index].borrowsOwnedTemporary || lentTemporary;
    }
    result.borrowsOwnedTemporary = borrows;
}

// EN: Freeze (plan item 135a): the result of an owned-returning call or a `new` of a class, as a value of a
//     reference type that is not a delegate (a delegate cannot be what a reference result aliases).
// ES: Congelamiento (elemento 135a del plan): el resultado de una llamada con retorno owned o un `new` de una
//     clase, como valor de un tipo referencia que no es delegate (un delegate no puede ser aquello de lo que un
//     resultado referencia es alias).
bool ProjectMethodBodyModel::isOwnedTemporaryValue(const ExpressionResult& value) const
{
    if (!value.valid || isAnyDelegateTypeKey(value.typeKey)) return false;
    if (!isReferenceTypeKey(value.typeKey) && value.typeKey != "builtin:string") return false;
    const bool ownedResult = value.ownership == sema::OwnershipKind::Owned && value.ownershipTransfer &&
        !value.ownershipSourceLocalOrdinal.has_value() && !value.ownershipSourceParameterOrdinal.has_value();
    const bool newObject = value.ownershipFreshOwner &&
        value.objectLifetimeOwnership == sema::OwnershipKind::Owned &&
        value.storageOwnership == sema::OwnershipKind::Owned;
    return ownedResult || newObject;
}

std::vector<bool> ProjectMethodBodyModel::ownedArgumentParameters(
    std::span<const std::size_t> argumentParameterOrdinals,
    const ClassMemberSymbol& callable) const
{
    std::vector<bool> owned(argumentParameterOrdinals.size(), false);
    for (std::size_t index = 0; index < argumentParameterOrdinals.size(); ++index)
    {
        const std::size_t parameter = argumentParameterOrdinals[index];
        owned[index] = parameter < callable.parameters.size() &&
            callable.parameters[parameter].ownership == sema::OwnershipKind::Owned;
    }
    return owned;
}

// EN: Freeze (plan item 135a): the parameters of a declared delegate that take ownership, and whether it
//     returns an owner. A structural function type declares neither, so every parameter lends.
// ES: Congelamiento (elemento 135a del plan): los parametros de un delegate declarado que toman ownership, y
//     si devuelve un owner. Un tipo de funcion estructural no declara ninguno, asi que todo parametro presta.
std::vector<bool> ProjectMethodBodyModel::ownedDelegateArgumentParameters(
    std::size_t delegateOrdinal,
    std::size_t argumentCount,
    bool& returnsOwned) const
{
    std::vector<bool> owned(argumentCount, false);
    returnsOwned = false;
    if (nameLookup_ == nullptr || delegateOrdinal >= nameLookup_->namedTypes().size()) return owned;
    const NamedTypeDeclaration& declaration = nameLookup_->namedTypes()[delegateOrdinal];
    if (declaration.kind != NamedTypeDeclarationKind::Delegate || declaration.syntax == nullptr) return owned;
    const ast::TypeSyntax* returnType = declaration.syntax->typeSyntax();
    returnsOwned = returnType != nullptr &&
        returnType->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::Owned;
    std::size_t index = 0;
    for (const ast::AstNode* child : declaration.syntax->children())
    {
        if (child == nullptr || child->kind() != ast::AstNodeKind::ParameterDeclaration) continue;
        const ast::TypeSyntax* type = child->typeSyntax();
        if (index < argumentCount)
            owned[index] = type != nullptr && type->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::Owned;
        ++index;
    }
    return owned;
}

void ProjectMethodBodyModel::markDelegateCallTemporaryBorrow(
    ExpressionResult& result,
    std::span<const ExpressionResult> arguments,
    std::size_t delegateOrdinal) const
{
    bool returnsOwned = false;
    const std::vector<bool> ownedParameters =
        ownedDelegateArgumentParameters(delegateOrdinal, arguments.size(), returnsOwned);
    if (!returnsOwned) markCallTemporaryBorrow(result, false, arguments, ownedParameters);
    // EN: LKF-286: a delegate declared with an owned return hands its result to the caller, as an
    //     owned-returning method does; the result carried no ownership and `raii T x = d();` was refused.
    // ES: LKF-286: un delegate declarado con retorno owned entrega su resultado al llamador, como un
    //     método con retorno owned; el resultado no llevaba propiedad y `raii T x = d();` se rechazaba.
    if (returnsOwned && result.valid)
    {
        result.ownership = sema::OwnershipKind::Owned;
        result.ownershipTransfer = true;
    }
}

bool ProjectMethodBodyModel::validateSelectedCallReceiver(
    std::optional<std::size_t>& provenVariantCase,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallCandidate& selected,
    MethodCallBinding& call,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (memberModel_ == nullptr || selected.memberOrdinal >= memberModel_->members().size())
        return true;
    const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
    if (!validateActiveIterationCallReceiver(
            expression, callee, selectedMember, call, scopeOrdinal, diagnostics)) return false;
    if (!selectedMember.isStatic &&
        (!selectedMember.declaringTypeIsStruct || !selectedMember.isReadonly) &&
        ((callee.receiverSyntax == nullptr && call.implicitThis) ||
         (callee.receiverSyntax != nullptr &&
          (callee.receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression ||
           callee.receiverSyntax->kind() == ast::AstNodeKind::MemberExpression ||
           callee.receiverSyntax->kind() == ast::AstNodeKind::IndexExpression ||
           callee.receiverSyntax->kind() == ast::AstNodeKind::ThisExpression))) &&
         hasActiveViewOfStorage(callee.receiverSyntax, call.callerMethodMemberOrdinal, scopeOrdinal, true) &&
         // EN: A source class or static callee is judged after every body is analyzed
         //     (finalizeStorageViewCalls); a struct instance method keeps its `readonly` contract.
         // ES: Un callee de fuente de clase o estatico se juzga al terminar todos los cuerpos
         //     (finalizeStorageViewCalls); un metodo de instancia de struct conserva su contrato `readonly`.
         (selectedMember.declaringTypeIsStruct ||
          !deferStorageViewCall(selected.memberOrdinal, expression->range(),
              "cannot call potentially mutating method while a borrowed field view is alive")))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error, expression->range(),
            "cannot call potentially mutating method while a borrowed field view is alive");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }
    updateSelectedVariantFlow(
        provenVariantCase, expression, callee, selectedMember, scopeOrdinal);
    // NativeSharp T03. Variant payload extractors have to be public so a match in a
    // LibraryReference consumer can call code transported through NSMETA, but they are not a
    // user-callable API: reading an inactive union alternative would be undefined. The parser
    // marks only the extractor call it generates after the corresponding IsCase branch.
    if (selectedMember.name.starts_with("__nsc_variant_extract_") &&
        expression->auxiliaryText() != "variant-pattern-extract")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "variant payload extraction is available only inside a matching case arm");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }
    if (callee.readonlyReceiver && selectedMember.declaringTypeIsStruct &&
        !selectedMember.isStatic && !selectedMember.isReadonly &&
        !(selectedMember.name.starts_with("__nsc_variant_extract_") &&
          expression->auxiliaryText() == "variant-pattern-extract"))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "readonly receiver cannot invoke mutating struct member '" +
                selectedMember.name + "'");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }
    // Class methods do not yet have a checked readonly receiver contract. Do not
    // let a readonly borrowed object enter a potentially mutating method.
    if (callee.readonlyBorrowedObject && !selectedMember.declaringTypeIsStruct &&
        !selectedMember.isStatic)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "readonly borrowed object cannot invoke an unchecked class member '" +
                selectedMember.name + "'");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }
    return true;
}

bool ProjectMethodBodyModel::validateActiveIterationCallReceiver(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const ClassMemberSymbol& selectedMember,
    MethodCallBinding& call,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (selectedMember.isStatic || selectedMember.isReadonly ||
        callee.receiverSyntax == nullptr || activeIterationBorrows_.empty()) return true;
    const std::optional<std::string> receiverPlace =
        storagePlaceKey(callee.receiverSyntax, scopeOrdinal);
    if (!receiverPlace.has_value()) return true;
    for (auto iterator = activeIterationBorrows_.rbegin();
         iterator != activeIterationBorrows_.rend(); ++iterator)
    {
        if (!placesOverlap(iterator->sourcePlace, *receiverPlace)) continue;
        bool compatibleElementCall = false;
        if (iterator->mutableElements &&
            callee.receiverSyntax->kind() == ast::AstNodeKind::IndexExpression &&
            callee.receiverSyntax->first() != nullptr)
        {
            const std::optional<std::string> indexedSource =
                storagePlaceKey(callee.receiverSyntax->first(), scopeOrdinal);
            compatibleElementCall = indexedSource.has_value() &&
                *indexedSource == iterator->sourcePlace;
        }
        if (compatibleElementCall) break;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot invoke mutating member '" + selectedMember.name +
                "' on a foreach source while a lending iteration is active; "
                "the operation could invalidate yielded references");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }
    return true;
}

void ProjectMethodBodyModel::updateSelectedVariantFlow(
    std::optional<std::size_t>& provenVariantCase,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const ClassMemberSymbol& selectedMember,
    std::size_t scopeOrdinal)
{
    const std::string_view variantMarker = selectedMember.syntax != nullptr
        ? std::string_view(selectedMember.syntax->auxiliaryText()) : std::string_view{};
    if (variantMarker.starts_with("variant-case-test:"))
    {
        const auto storage = callNamedStorageKey(callee.receiverSyntax, scopeOrdinal);
        const auto activeCase = callMarkerCase(variantMarker);
        if (storage.has_value() && activeCase.has_value())
        {
            ConditionFlowStates flow{captureOwnershipFlow(), captureOwnershipFlow()};
            if (OwnershipFlowStorageFacts* facts =
                    mutableSnapshotStorageFacts(flow.whenTrue, *storage);
                facts != nullptr)
                facts->activeVariantCase = *activeCase;
            if (OwnershipFlowStorageFacts* facts =
                    mutableSnapshotStorageFacts(flow.whenFalse, *storage);
                facts != nullptr)
                facts->activeVariantCase.reset();
            conditionFlowBySyntax_.insert_or_assign(expression, std::move(flow));
        }
    }
    else if (variantMarker.starts_with("variant-case-extract:") &&
             expression->auxiliaryText() == "variant-pattern-extract")
    {
        const auto storage = callNamedStorageKey(callee.receiverSyntax, scopeOrdinal);
        const auto extractedCase = callMarkerCase(variantMarker);
        if (storage.has_value() && extractedCase.has_value())
        {
            const auto active = activeVariantCases_.find(*storage);
            if (active != activeVariantCases_.end() && active->second == *extractedCase)
                provenVariantCase = *extractedCase;
        }
    }
}

void ProjectMethodBodyModel::invalidateCallArgumentFlow(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    bool coreStringForwarding,
    std::size_t scopeOrdinal)
{
    // A ref/out call is a write through the argument. Any narrowing fact for that named
    // storage becomes stale at the call boundary; by-value calls cannot reassign it.
    if (memberModel_ == nullptr || selected.memberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& selectedMemberForFlow =
        memberModel_->members()[selected.memberOrdinal];
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMemberForFlow.parameters.size() ||
            (selectedMemberForFlow.parameters[parameterOrdinal].passKind != ParameterPassKind::Ref &&
             selectedMemberForFlow.parameters[parameterOrdinal].passKind != ParameterPassKind::Out))
            continue;
        const ast::AstNode* argumentSyntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        const ast::AstNode* targetSyntax =
            argumentSyntax != nullptr && argumentSyntax->first() != nullptr
                ? definiteAssignmentTargetRoot(argumentSyntax->first()) : nullptr;
        const auto storage = callNamedStorageKey(targetSyntax, scopeOrdinal);
        if (!storage.has_value()) continue;
        nonNullStorages_.erase(*storage);
        integerRanges_.erase(*storage);
        activeVariantCases_.erase(*storage);
    }
    const auto replacement = simpleDelegateReplacement(selectedMemberForFlow);
    if (!replacement.has_value()) return;
    const ast::AstNode* target = nullptr;
    const ast::AstNode* source = nullptr;
    for (std::size_t index = 0; index < selected.argumentParameterOrdinals.size(); ++index)
    {
        const ast::AstNode* argument = resolvedCallArgumentSyntax(expression, callee, index, coreStringForwarding);
        if (argument == nullptr) continue;
        if (selected.argumentParameterOrdinals[index] == replacement->first) target = argument->first();
        if (selected.argumentParameterOrdinals[index] == replacement->second) source = argument;
    }
    if (target == nullptr || source == nullptr || target->kind() != ast::AstNodeKind::IdentifierExpression ||
        source->kind() != ast::AstNodeKind::IdentifierExpression) return;
    const auto targetName = lookupScopeName(scopeOrdinal, target->text());
    const auto sourceName = lookupScopeName(scopeOrdinal, source->text());
    if (!targetName.has_value() || !sourceName.has_value() || targetName->kind != ScopeNameKind::Local ||
        sourceName->kind != ScopeNameKind::Local || targetName->semanticOrdinal >= locals_.size() ||
        sourceName->semanticOrdinal >= locals_.size() ||
        !isAnyDelegateTypeKey(locals_[targetName->semanticOrdinal].typeKey)) return;
    const auto sourceEnvironment = fnOnceEnvironments_.find(sourceName->semanticOrdinal);
    if (sourceEnvironment == fnOnceEnvironments_.end() || sourceEnvironment->second.size() != 1) return;
    const auto origins = sourceEnvironment->second;
    fnOnceEnvironments_[targetName->semanticOrdinal] = origins;
    invokedFnOnceLocals_.erase(targetName->semanticOrdinal);
    maybeInvokedFnOnceLocals_.erase(targetName->semanticOrdinal);
}

void ProjectMethodBodyModel::updateScopedChildBorrowsForCall(
    const ast::AstNode* expression,
    const MethodCallCandidate& selected,
    const MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T07-F06: TaskScope.Join discharges the scoped child borrows; TaskScope.Spawn of an
    // async child that captures a `ref value` registers the borrow of that source local. Block exit
    // while a borrow is still registered rejects the dangling Future.
    // EN: Only NativeSharp.Core TaskScope<T> calls take part. A Join used to clear every borrow,
    //     whichever scope or type it belonged to, and Spawn saw only a direct child call matched by
    //     argument position (LKF-289).
    // ES: Solo participan las llamadas de TaskScope<T> de NativeSharp.Core. Un Join limpiaba todos
    //     los préstamos, fuera cual fuera su scope o su tipo, y Spawn solo veía una llamada hija
    //     directa emparejada por posición (LKF-289).
    if ((call.methodName == "Join" || call.methodName == "Spawn") && !selectedCallIsCoreTaskScope(selected))
        return;
    const std::size_t scopeKey = scopedChildReceiverKey(expression, scopeOrdinal);
    if (call.methodName == "Join")
    {
        scopedChildBorrowsByScope_.erase(scopeKey);
        scopedChildBorrows_.clear();
        for (const auto& [key, borrows] : scopedChildBorrowsByScope_)
            scopedChildBorrows_.insert(borrows.begin(), borrows.end());
    }
    else if (call.methodName == "Spawn")
    {
        registerScopedChildBorrows(expression, methodMemberOrdinal, scopeOrdinal, scopeKey);
        // NS304-T07-F07: a thread executor may move the Future to another thread, so every `ref`
        // capture must be Send (a raw pointer, or a type containing one, does not derive Send).
        if (selectedCallUsesThreadExecutor(selected))
            validateThreadExecutorCaptures(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
}

void ProjectMethodBodyModel::registerScopedChildBorrows(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::size_t scopeKey)
{
    for (const ast::AstNode* argument : expression->children())
    {
        if (argument == nullptr) continue;
        // EN: `Spawn(move(f))` / `Spawn(f)` spawns a Future local; its initializer is the child call.
        // ES: `Spawn(move(f))` / `Spawn(f)` lanza un local Future; su inicializador es la llamada hija.
        const ast::AstNode* source = argument;
        if (source->kind() == ast::AstNodeKind::MoveExpression && source->first() != nullptr)
            source = source->first();
        if (source->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, source->text());
            if (name.has_value() && name->kind == ScopeNameKind::Local && name->semanticOrdinal < locals_.size() &&
                locals_[name->semanticOrdinal].syntax != nullptr)
                source = locals_[name->semanticOrdinal].syntax->first();
        }
        if (source == nullptr || source->kind() != ast::AstNodeKind::CallExpression) continue;
        registerScopedChildCallBorrows(source, methodMemberOrdinal, scopeOrdinal, scopeKey);
    }
}

void ProjectMethodBodyModel::registerScopedChildCallBorrows(
    const ast::AstNode* childCallSyntax,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::size_t scopeKey)
{
    const MethodCallBinding* childCall = callBinding(childCallSyntax, methodMemberOrdinal);
    if (childCall == nullptr || !childCall->selectedMemberOrdinal.has_value()) return;
    if (*childCall->selectedMemberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& child = memberModel_->members()[*childCall->selectedMemberOrdinal];
    // EN: Map arguments through the selected binding so named arguments reach their parameter.
    // ES: Emparejar por el binding elegido para que los argumentos con nombre lleguen a su parámetro.
    const bool bound = !childCall->selectedArgumentSyntaxes.empty() &&
        childCall->selectedArgumentSyntaxes.size() == childCall->selectedArgumentParameterOrdinals.size();
    const std::size_t count = bound ? childCall->selectedArgumentSyntaxes.size() : childCallSyntax->children().size();
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::size_t parameterOrdinal = bound ? childCall->selectedArgumentParameterOrdinals[index] : index;
        const ast::AstNode* argumentSyntax = bound
            ? childCall->selectedArgumentSyntaxes[index] : childCallSyntax->children()[index];
        if (argumentSyntax == nullptr || parameterOrdinal >= child.parameters.size() ||
            child.parameters[parameterOrdinal].passKind != ParameterPassKind::Ref)
            continue;
        const MethodEscapeOrigin origin = referenceEscapeOriginOfExpression(argumentSyntax, scopeOrdinal);
        if (origin.kind == MethodEscapeOriginKind::CurrentFrameLocal && origin.ordinal.has_value())
        {
            scopedChildBorrows_.insert(*origin.ordinal);
            scopedChildBorrowsByScope_[scopeKey].insert(*origin.ordinal);
        }
    }
}

std::size_t ProjectMethodBodyModel::scopedChildReceiverKey(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    const ast::AstNode* callee = expression != nullptr ? expression->first() : nullptr;
    const ast::AstNode* receiver = callee != nullptr && callee->kind() == ast::AstNodeKind::MemberExpression
        ? callee->first() : nullptr;
    if (receiver == nullptr || receiver->kind() != ast::AstNodeKind::IdentifierExpression)
        return std::numeric_limits<std::size_t>::max();
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, receiver->text());
    if (!name.has_value() || name->kind != ScopeNameKind::Local) return std::numeric_limits<std::size_t>::max();
    return name->semanticOrdinal;
}

bool ProjectMethodBodyModel::selectedCallIsCoreTaskScope(const MethodCallCandidate& selected) const
{
    if (genericModel_ == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr ||
        selected.memberOrdinal >= memberModel_->members().size()) return false;
    const std::size_t declaringOrdinal = memberModel_->members()[selected.memberOrdinal].declaringTypeOrdinal;
    if (declaringOrdinal >= nameLookup_->namedTypes().size()) return false;
    const GenericSpecializationRecord* specialization =
        genericModel_->specializationForTypeKey(nameLookup_->namedTypes()[declaringOrdinal].identity.key());
    if (specialization == nullptr) return false;
    const GenericTemplateRecord* source = genericModel_->templateForCanonicalKey(specialization->templateKey);
    return source != nullptr && source->namespaceName == "NativeSharp.Core" && source->ownerName.empty() &&
        source->name == "TaskScope";
}

bool ProjectMethodBodyModel::selectedCallUsesThreadExecutor(
    const MethodCallCandidate& selected) const
{
    if (genericModel_ == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr ||
        inheritanceModel_ == nullptr || interfaceModel_ == nullptr ||
        selected.memberOrdinal >= memberModel_->members().size()) return false;
    const ClassMemberSymbol& spawnMember = memberModel_->members()[selected.memberOrdinal];
    const std::size_t declaringOrdinal = spawnMember.declaringTypeOrdinal;
    if (declaringOrdinal >= nameLookup_->namedTypes().size()) return false;
    const std::string scopeKey = nameLookup_->namedTypes()[declaringOrdinal].identity.key();
    const GenericSpecializationRecord* spec =
        genericModel_->specializationForTypeKey(scopeKey);
    if (spec == nullptr || spec->typeArguments.size() != 1U) return false;
    const std::string executorKey = ProjectMemberModel::canonicalGenericArgumentKey(
        spec->typeArguments.front(), *nameLookup_);
    const std::optional<std::size_t> executorOrdinal = namedTypeOrdinalForTypeKey(executorKey);
    const std::optional<std::size_t> threadOrdinal = namedTypeOrdinalForTypeKey(
        "type:NativeSharpCore::NativeSharp.Core.IThreadExecutor");
    return executorOrdinal.has_value() && threadOrdinal.has_value() &&
        interfaceModel_->classImplementsInterface(*executorOrdinal, *threadOrdinal);
}

void ProjectMethodBodyModel::validateThreadExecutorCaptures(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const ast::AstNode* argument : expression->children())
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::CallExpression)
            continue;
        const MethodCallBinding* childCall = callBinding(argument, methodMemberOrdinal);
        if (childCall == nullptr || !childCall->selectedMemberOrdinal.has_value()) continue;
        if (*childCall->selectedMemberOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& child =
            memberModel_->members()[*childCall->selectedMemberOrdinal];
        for (std::size_t childArgOrdinal = 0;
             childArgOrdinal < argument->children().size(); ++childArgOrdinal)
        {
            if (childArgOrdinal >= child.parameters.size() ||
                child.parameters[childArgOrdinal].passKind != ParameterPassKind::Ref)
                continue;
            const MethodEscapeOrigin origin = referenceEscapeOriginOfExpression(
                argument->children()[childArgOrdinal], scopeOrdinal);
            if (origin.kind != MethodEscapeOriginKind::CurrentFrameLocal ||
                !origin.ordinal.has_value() || *origin.ordinal >= locals_.size())
                continue;
            const MethodLocalSymbol& source = locals_[*origin.ordinal];
            if (!callTypeIsSend(source.typeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCall,
                    diagnostics::DiagnosticSeverity::Error,
                    argument->range(),
                    "future submitted to a thread executor is not Send: capture '" +
                        source.name + "' has type '" + source.typeKey + "'");
            }
            else if (!callTypeIsSync(source.typeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCall,
                    diagnostics::DiagnosticSeverity::Error,
                    argument->range(),
                    "shared capture '" + source.name +
                        "' requires its referred type '" + source.typeKey + "' to be Sync");
            }
        }
    }
}

namespace
{
// The ordinary capture walk remains unchanged. This context is only used for
// an explicit conditional unsafe impl and checks both payload guarantees with
// a bounded worklist, including nested wrappers and cyclic named types.
class ConditionalCaptureThreadSafety final
{
public:
    ConditionalCaptureThreadSafety(const ProjectNameLookup* lookup, const ProjectMemberModel* members,
                                   const ProjectInheritanceModel* inheritance)
        : lookup_(lookup), members_(members), inheritance_(inheritance)
    {
        if (lookup_ == nullptr) return;
        bool ambiguous = false;
        for (const NamedTypeDeclaration& declaration : lookup_->namedTypes())
        {
            if (declaration.kind != NamedTypeDeclarationKind::Interface) continue;
            const std::string_view name = declaration.identity.fullyQualifiedNameView();
            std::string* key = name == "NativeSharp.Core.Send" ? &sendKey_
                : name == "NativeSharp.Core.Sync" ? &syncKey_ : nullptr;
            if (key == nullptr) continue;
            if (!key->empty()) { ambiguous = true; break; }
            *key = "type:" + declaration.identity.key();
        }
        if (ambiguous || sendKey_.size() < 4U || syncKey_.size() < 4U ||
            sendKey_.substr(0, sendKey_.size() - 4U) != syncKey_.substr(0, syncKey_.size() - 4U))
        {
            sendKey_.clear();
            syncKey_.clear();
        }
    }

    std::optional<std::string> payload(std::size_t ordinal, bool send) const
    {
        if (lookup_ == nullptr || members_ == nullptr || sendKey_.empty() || syncKey_.empty())
            return std::nullopt;
        const NamedTypeDeclaration* declaration = lookup_->typeForOrdinal(ordinal);
        return declaration == nullptr ? std::nullopt : members_->conditionalUnsafeThreadArgument(
            *declaration, send ? sendKey_ : syncKey_, *lookup_);
    }

    bool payloadIsSafe(const std::string& typeKey) const
    {
        if (inheritance_ == nullptr || members_ == nullptr) return false;
        struct Item { std::string key; bool send; };
        std::vector<Item> pending{{typeKey, true}, {typeKey, false}};
        std::array<std::unordered_set<std::size_t>, 2> visited;
        std::vector<std::size_t> fallback;
        while (!pending.empty())
        {
            Item item = std::move(pending.back());
            pending.pop_back();
            while (true)
            {
                if (item.key.ends_with("*")) return false;
                if (item.key.starts_with("refmut:"))
                {
                    if (!item.send) return false;
                    item.key = item.key.substr(7);
                }
                else if (item.key.starts_with("ref:"))
                {
                    item.send = false;
                    item.key = item.key.substr(4);
                }
                else break;
            }
            auto element = ProjectMemberModel::fixedArrayElementTypeKey(item.key);
            if (!element.has_value()) element = ProjectTypeConversion::nativeArrayElementType(item.key);
            if (element.has_value())
            {
                pending.push_back({std::move(*element), item.send});
                continue;
            }
            if (auto elements = ProjectTypeConversion::tupleElementTypeKeys(item.key); elements.has_value())
            {
                for (std::string& part : *elements) pending.push_back({std::move(part), item.send});
                continue;
            }
            const auto ordinal = inheritance_->typeOrdinalForTypeKey(item.key);
            if (!ordinal.has_value())
            {
                if (!item.key.starts_with("builtin:")) return false;
                continue;
            }
            if (!visited[item.send ? 1U : 0U].insert(*ordinal).second) continue;
            if (const auto nested = payload(*ordinal, item.send); nested.has_value())
            {
                pending.push_back({*nested, true});
                pending.push_back({*nested, false});
                continue;
            }
            if (const auto base = inheritance_->baseTypeOrdinal(*ordinal); base.has_value())
                pending.push_back({inheritance_->typeKeyForOrdinal(*base), item.send});
            std::span<const std::size_t> ordinals = members_->membersDeclaredByType(*ordinal);
            if (!members_->hasMembersDeclaredByTypeIndex())
            {
                fallback.clear();
                for (std::size_t index = 0; index < members_->members().size(); ++index)
                    if (members_->members()[index].declaringTypeOrdinal == *ordinal) fallback.push_back(index);
                ordinals = fallback;
            }
            for (const std::size_t index : ordinals)
                if (index < members_->members().size() && members_->members()[index].kind == ClassMemberKind::Field)
                    pending.push_back({members_->members()[index].declaredTypeKey, item.send});
        }
        return true;
    }

private:
    const ProjectNameLookup* lookup_;
    const ProjectMemberModel* members_;
    const ProjectInheritanceModel* inheritance_;
    std::string sendKey_;
    std::string syncKey_;
};
}

bool ProjectMethodBodyModel::callTypeIsSend(const std::string& typeKey) const
{
    // Depth-first walk over the field types with an explicit stack instead of recursion. The
    // answer is the conjunction of the leaf tests the recursion made; a named type already
    // walked in this query adds no new field types, so it is not walked again (the recursion
    // had no such guard, so a cycle of field types never terminated).
    std::vector<std::string> pending{typeKey};
    std::unordered_set<std::size_t> walkedTypes;
    std::vector<std::size_t> fallbackMemberOrdinals;
    const ConditionalCaptureThreadSafety conditional(nameLookup_, memberModel_, inheritanceModel_);
    while (!pending.empty())
    {
        std::string current = std::move(pending.back());
        pending.pop_back();
        // ref:/refmut: are stripped one at a time, each time after the pointer test, as the
        // recursion did.
        while (true)
        {
            if (current.ends_with("*")) return false;
            if (!current.starts_with("ref:") && !current.starts_with("refmut:")) break;
            current = current.substr(current.find(':') + 1);
        }
        // EN: LKF-290: a delegate or lambda value carries a captured environment that its type does not
        //     describe, so the walk found no fields and took it as Send. It is not provably Send.
        // ES: LKF-290: un delegate o una lambda lleva un entorno capturado que su tipo no describe, así
        //     que el recorrido no hallaba campos y lo daba por Send. No es demostrablemente Send.
        if (language::isAnonymousLambdaTypeIdentity(current) ||
            (delegateTypeOrdinalForTypeKey(current).has_value() && !isExternCDelegateTypeKey(current)))
            return false;
        if (auto element = ProjectMemberModel::fixedArrayElementTypeKey(current); element.has_value())
        {
            pending.push_back(std::move(*element));
            continue;
        }
        if (auto element = ProjectTypeConversion::nativeArrayElementType(current); element.has_value())
        {
            pending.push_back(std::move(*element));
            continue;
        }
        if (auto elements = ProjectTypeConversion::tupleElementTypeKeys(current); elements.has_value())
        {
            pending.insert(pending.end(), elements->begin(), elements->end());
            continue;
        }
        const std::optional<std::size_t> typeOrdinal = namedTypeOrdinalForTypeKey(current);
        if (!typeOrdinal.has_value() || !walkedTypes.insert(*typeOrdinal).second) continue;
        if (const auto payload = conditional.payload(*typeOrdinal, true); payload.has_value())
        {
            if (!conditional.payloadIsSafe(*payload)) return false;
            continue;
        }
        if (inheritanceModel_ != nullptr)
            if (const auto base = inheritanceModel_->baseTypeOrdinal(*typeOrdinal); base.has_value())
                pending.push_back(inheritanceModel_->typeKeyForOrdinal(*base));
        std::span<const std::size_t> memberOrdinals =
            memberModel_->membersDeclaredByType(*typeOrdinal);
        if (!memberModel_->hasMembersDeclaredByTypeIndex())
        {
            fallbackMemberOrdinals.clear();
            for (std::size_t ordinal = 0; ordinal < memberModel_->members().size(); ++ordinal)
            {
                if (memberModel_->members()[ordinal].declaringTypeOrdinal == *typeOrdinal)
                    fallbackMemberOrdinals.push_back(ordinal);
            }
            memberOrdinals = fallbackMemberOrdinals;
        }
        // Pushed in reverse so the fields are taken in declaration order, as the recursion did.
        for (auto memberOrdinal = memberOrdinals.rbegin(); memberOrdinal != memberOrdinals.rend();
             ++memberOrdinal)
        {
            if (*memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& member = memberModel_->members()[*memberOrdinal];
            if (member.kind != ClassMemberKind::Field) continue;
            pending.push_back(member.declaredTypeKey);
        }
    }
    return true;
}

bool ProjectMethodBodyModel::callTypeIsSync(const std::string& typeKey) const
{
    // Depth-first walk over the field types with an explicit stack instead of recursion. The
    // answer is the conjunction of the leaf tests the recursion made; a named type already
    // walked in this query adds no new field types, so it is not walked again (the recursion
    // had no such guard, so a cycle of field types never terminated).
    std::vector<std::string> pending{typeKey};
    std::unordered_set<std::size_t> walkedTypes;
    std::vector<std::size_t> fallbackMemberOrdinals;
    const ConditionalCaptureThreadSafety conditional(nameLookup_, memberModel_, inheritanceModel_);
    while (!pending.empty())
    {
        std::string current = std::move(pending.back());
        pending.pop_back();
        // ref: is stripped one at a time, each time after the pointer and refmut: tests, as the
        // recursion did.
        while (true)
        {
            if (current.ends_with("*")) return false;
            if (current.starts_with("refmut:")) return false;
            if (!current.starts_with("ref:")) break;
            current = current.substr(current.find(':') + 1);
        }
        // EN: LKF-290: a delegate or lambda value carries a captured environment that its type does not
        //     describe, so the walk found no fields and took it as Send. It is not provably Send or Sync.
        // ES: LKF-290: un delegate o una lambda lleva un entorno capturado que su tipo no describe, así
        //     que el recorrido no hallaba campos y lo daba por Send. No es demostrablemente Send ni Sync.
        if (language::isAnonymousLambdaTypeIdentity(current) ||
            (delegateTypeOrdinalForTypeKey(current).has_value() && !isExternCDelegateTypeKey(current)))
            return false;
        if (auto element = ProjectMemberModel::fixedArrayElementTypeKey(current); element.has_value())
        {
            pending.push_back(std::move(*element));
            continue;
        }
        if (auto element = ProjectTypeConversion::nativeArrayElementType(current); element.has_value())
        {
            pending.push_back(std::move(*element));
            continue;
        }
        if (auto elements = ProjectTypeConversion::tupleElementTypeKeys(current); elements.has_value())
        {
            pending.insert(pending.end(), elements->begin(), elements->end());
            continue;
        }
        const std::optional<std::size_t> typeOrdinal = namedTypeOrdinalForTypeKey(current);
        if (!typeOrdinal.has_value() || !walkedTypes.insert(*typeOrdinal).second) continue;
        if (const auto payload = conditional.payload(*typeOrdinal, false); payload.has_value())
        {
            if (!conditional.payloadIsSafe(*payload)) return false;
            continue;
        }
        if (inheritanceModel_ != nullptr)
            if (const auto base = inheritanceModel_->baseTypeOrdinal(*typeOrdinal); base.has_value())
                pending.push_back(inheritanceModel_->typeKeyForOrdinal(*base));
        std::span<const std::size_t> memberOrdinals =
            memberModel_->membersDeclaredByType(*typeOrdinal);
        if (!memberModel_->hasMembersDeclaredByTypeIndex())
        {
            fallbackMemberOrdinals.clear();
            for (std::size_t ordinal = 0; ordinal < memberModel_->members().size(); ++ordinal)
            {
                if (memberModel_->members()[ordinal].declaringTypeOrdinal == *typeOrdinal)
                    fallbackMemberOrdinals.push_back(ordinal);
            }
            memberOrdinals = fallbackMemberOrdinals;
        }
        // Pushed in reverse so the fields are taken in declaration order, as the recursion did.
        for (auto memberOrdinal = memberOrdinals.rbegin(); memberOrdinal != memberOrdinals.rend();
             ++memberOrdinal)
        {
            if (*memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& member = memberModel_->members()[*memberOrdinal];
            if (member.kind != ClassMemberKind::Field) continue;
            pending.push_back(member.declaredTypeKey);
        }
    }
    return true;
}

void ProjectMethodBodyModel::configureSelectedCallBinding(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallCandidate& selected,
    bool coreStringForwarding,
    MethodCallBinding& call) const
{
    call.selectedConversions = selected.conversions;
    call.selectedArgumentParameterOrdinals = selected.argumentParameterOrdinals;
    call.selectedArgumentSyntaxes.reserve(selected.argumentParameterOrdinals.size());
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
        call.selectedArgumentSyntaxes.push_back(resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding));
    call.selectedParameterPackElements = selected.parameterPackElements;
    call.selectedParameterPackOrdinal = selected.parameterPackOrdinal;
    call.selectedUsesExpandedParameterPack = selected.usesExpandedParameterPack;
    call.selectedSynthesizesEmptyParameterPack = selected.synthesizesEmptyParameterPack;
    if (memberModel_ == nullptr || selected.memberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
    for (const std::size_t parameterOrdinal : selected.defaultedParameterOrdinals)
    {
        call.selectedDefaultArguments.push_back(
            selectedMember.parameters[parameterOrdinal].defaultValueSyntax);
        call.selectedDefaultArgumentParameterOrdinals.push_back(parameterOrdinal);
    }
    call.resultTypeKey = selectedMember.declaredTypeKey;
    result.pointerPointeeReadonly = selectedMember.declaredPointerPointeeReadonly;
    result.pointerPointeeVolatile = selectedMember.declaredPointerPointeeVolatile;
    result.pointerLevelQualifiers = selectedMember.declaredPointerLevelQualifiers;
    result.cppPointerPointeeAtomic = selectedMember.cppDeclaredPointerPointeeAtomic;
    result.pointerNullable = selectedMember.declaredPointerNullable;
    result.pointerHasProvenance = selectedMember.declaredPointerHasProvenance;
    result.pointerKnownAlignment = selectedMember.declaredPointerKnownAlignment;
    result.pointerAddressSpace = selectedMember.declaredPointerAddressSpace;
    result.ownership = selectedMember.declaredOwnership;
    if (selectedMember.declaredOwnership != sema::OwnershipKind::None)
    {
        result.objectLifetimeOwnership = selectedMember.declaredOwnership;
        result.storageOwnership = selectedMember.declaredOwnership;
    }
    // EN: Freeze P3: a `ref readonly` result is an addressable place that cannot be written;
    //     like an `in` parameter it admits reads and readonly members only.
    // ES: Congelamiento P3: un resultado `ref readonly` es un lugar direccionable que no se
    //     puede escribir; como un parametro `in` solo admite lecturas y miembros readonly.
    const bool readonlyReference = isReadonlyReferenceReturn(selectedMember);
    result.assignable = selectedMember.isReferenceReturn && !readonlyReference;
    result.addressable = selectedMember.isReferenceReturn;
    result.readonlyReceiver = result.readonlyReceiver || readonlyReference;
    // Reading a class reference through a borrowed view or a non-static reference
    // return must preserve the loan rather than create an escaping class alias.
    const auto resultClass = referenceTypeOrdinalForTypeKey(selectedMember.declaredTypeKey);
    if (((!selectedMember.isStatic && typeKeyIsViewLike(call.receiverTypeKey)) ||
         (selectedMember.isReferenceReturn &&
          (readonlyReference || (!selectedMember.isStatic && selectedMember.returnLifetimeName != "static")))) &&
        resultClass.has_value() && inheritanceModel_ != nullptr && inheritanceModel_->isClass(*resultClass))
    {
        result.ownership = sema::OwnershipKind::Borrowed;
        result.objectLifetimeOwnership = sema::OwnershipKind::Borrowed;
        result.storageOwnership = sema::OwnershipKind::Borrowed;
        result.readonlyBorrowedObject = readonlyReference;
    }
    const NamedTypeDeclaration* returnedType = readonlyReference && selectedMember.isReferenceReturn
        ? namedTypeForTypeKey(selectedMember.declaredTypeKey) : nullptr;
    bool aggregateMayCarryReferences = false;
    if (returnedType != nullptr && returnedType->kind == NamedTypeDeclarationKind::Struct)
    {
        std::vector<std::string> pending{selectedMember.declaredTypeKey};
        std::unordered_set<std::size_t> visited;
        while (!pending.empty() && !aggregateMayCarryReferences)
        {
            const std::string current = std::move(pending.back());
            pending.pop_back();
            if (ProjectTypeConversion::isNumeric(current) || current == "builtin:bool" ||
                current == "builtin:char" || isEnumTypeKey(current)) continue;
            const NamedTypeDeclaration* type = namedTypeForTypeKey(current);
            const auto ordinal = namedTypeOrdinalForTypeKey(current);
            if (type == nullptr || type->kind != NamedTypeDeclarationKind::Struct ||
                !ordinal.has_value() || memberModel_ == nullptr ||
                (type->isPrecompiled && !selectedMember.isStatic && selectedMember.returnLifetimeName != "static"))
            {
                aggregateMayCarryReferences = true;
                break;
            }
            if (!visited.insert(*ordinal).second) continue;
            for (const std::size_t memberOrdinal : memberModel_->membersOfType(*ordinal))
            {
                if (memberOrdinal >= memberModel_->members().size())
                {
                    aggregateMayCarryReferences = true;
                    break;
                }
                const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
                if (member.kind == ClassMemberKind::Field && !member.isStatic)
                {
                    if (member.isReferenceReturn || member.declaredTypeKey.starts_with("refmut:") || member.isImportedC)
                    {
                        aggregateMayCarryReferences = true;
                        break;
                    }
                    pending.push_back(member.declaredTypeKey);
                }
            }
        }
    }
    if (aggregateMayCarryReferences)
    {
        result.ownership = sema::OwnershipKind::Borrowed;
        result.objectLifetimeOwnership = sema::OwnershipKind::Borrowed;
        result.storageOwnership = sema::OwnershipKind::Borrowed;
        result.readonlyBorrowedObject = true;
    }
    // A reference-returning instance method with lifetime `this` lends its
    // named move-only receiver. Preserve that owner for derived field borrows.
    if (result.readonlyBorrowedObject && selectedMember.isReferenceReturn &&
        !selectedMember.isStatic && selectedMember.returnLifetimeName == "this" &&
        callee.receiverSyntax != nullptr &&
        callee.receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const auto owner = lookupScopeName(call.scopeOrdinal, callee.receiverSyntax->text());
        if (owner.has_value() && owner->kind == ScopeNameKind::Local &&
            owner->semanticOrdinal < locals_.size() && locals_[owner->semanticOrdinal].isMoveOnly)
            result.ownershipSourceLocalOrdinal = owner->semanticOrdinal;
        const ClassMemberSymbol* caller = call.scopeOrdinal < scopes_.size()
            ? methodSymbol(scopes_[call.scopeOrdinal].methodMemberOrdinal) : nullptr;
        if (owner.has_value() && owner->kind == ScopeNameKind::Parameter && caller != nullptr &&
            owner->semanticOrdinal < caller->parameters.size() && memberModel_ != nullptr &&
            caller->parameters[owner->semanticOrdinal].passKind == ParameterPassKind::Value &&
            memberModel_->typeIsMoveOnly(caller->parameters[owner->semanticOrdinal].typeKey))
            result.ownershipSourceParameterOrdinal = owner->semanticOrdinal;
    }
    if (result.readonlyBorrowedObject && selectedMember.isReferenceReturn &&
        !selectedMember.isStatic && selectedMember.returnLifetimeName == "this" &&
        typeKeyIsViewLike(callee.receiverTypeKey) && callee.receiverSyntax != nullptr)
    {
        const auto root = viewRootLocalOfExpression(callee.receiverSyntax, call.scopeOrdinal);
        std::size_t rootScope = call.scopeOrdinal;
        if (callee.receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto receiver = lookupScopeName(call.scopeOrdinal, callee.receiverSyntax->text());
            if (receiver.has_value() && receiver->kind == ScopeNameKind::Local &&
                receiver->semanticOrdinal < locals_.size() && root.has_value() &&
                locals_[receiver->semanticOrdinal].viewRootLocalName == *root)
                rootScope = locals_[receiver->semanticOrdinal].scopeOrdinal;
        }
        const auto owner = root.has_value() ? lookupScopeName(rootScope, *root) : std::nullopt;
        if (owner.has_value() && owner->kind == ScopeNameKind::Local &&
            owner->semanticOrdinal < locals_.size() && locals_[owner->semanticOrdinal].isMoveOnly &&
            !typeKeyIsViewLike(locals_[owner->semanticOrdinal].typeKey))
            result.ownershipSourceLocalOrdinal = owner->semanticOrdinal;
        const ClassMemberSymbol* caller = call.scopeOrdinal < scopes_.size()
            ? methodSymbol(scopes_[call.scopeOrdinal].methodMemberOrdinal) : nullptr;
        if (owner.has_value() && owner->kind == ScopeNameKind::Parameter && caller != nullptr &&
            owner->semanticOrdinal < caller->parameters.size() && memberModel_ != nullptr &&
            caller->parameters[owner->semanticOrdinal].passKind == ParameterPassKind::Value &&
            memberModel_->typeIsMoveOnly(caller->parameters[owner->semanticOrdinal].typeKey))
            result.ownershipSourceParameterOrdinal = owner->semanticOrdinal;
    }
    result.ownershipTransfer = selectedMember.declaredOwnership == sema::OwnershipKind::Owned;
    // An owned return crosses the call boundary without a named owner. It is therefore a
    // fresh owner in the caller, just like `new T(...)`, and may participate in the
    // full-expression borrowing rule below.
    result.ownershipFreshOwner = result.ownershipTransfer;
    call.staticCall = selectedMember.isStatic;
    if (call.staticCall)
    {
        call.implicitThis = false;
        call.explicitThis = false;
        call.baseReceiver = false;
        call.interfaceCall = false;
        if (!call.forwardedReceiver)
        {
            call.receiverSyntax = nullptr;
            call.receiverTypeKey.clear();
            call.receiverTypeOrdinal.reset();
        }
    }
}

void ProjectMethodBodyModel::recordSelectedOutArguments(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    const ClassMemberSymbol& selectedMember,
    bool coreStringForwarding,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // NSC-LIMIT-006. Forwarding the current method's `out` parameter to another real `out`
    // parameter is a write just as surely as `value = ...`. Do this only after overload
    // resolution: syntax alone is insufficient because `callee(out value)` must not satisfy
    // the obligation when the selected parameter is actually `ref` (that mismatch is still
    // diagnosed by checkArgumentPassKinds above). Following the target root also covers the
    // member/index forms already accepted for explicit assignments.
    const ClassMemberSymbol* callerMember = methodSymbol(methodMemberOrdinal);
    for (std::size_t argumentOrdinal = 0; callerMember != nullptr &&
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMember.parameters.size() ||
            selectedMember.parameters[parameterOrdinal].passKind != ParameterPassKind::Out ||
            resolvedArguments[argumentOrdinal].argumentPassKind != ParameterPassKind::Out)
            continue;
        const ast::AstNode* argumentSyntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        if (argumentSyntax == nullptr || argumentSyntax->first() == nullptr) continue;
        const ast::AstNode* root = definiteAssignmentTargetRoot(argumentSyntax->first());
        if (root == nullptr) continue;
        const std::optional<ScopeName> target = lookupScopeName(scopeOrdinal, root->text());
        if (!target.has_value() || target->kind != ScopeNameKind::Parameter ||
            target->semanticOrdinal >= callerMember->parameters.size() ||
            callerMember->parameters[target->semanticOrdinal].passKind != ParameterPassKind::Out)
            continue;
        assignedOutParameters_.insert({methodMemberOrdinal, target->semanticOrdinal});
        activeOutParameterAssignments_.insert(target->semanticOrdinal);
    }
}

void ProjectMethodBodyModel::recordSelectedOwnershipTransfers(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    const ClassMemberSymbol& selectedMember,
    bool coreStringForwarding,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMember.parameters.size() ||
            selectedMember.parameters[parameterOrdinal].ownership != sema::OwnershipKind::Owned ||
            resolvedArguments[argumentOrdinal].typeKey == "builtin:null")
            continue;
        MethodOwnershipTransferBinding transfer;
        transfer.syntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        transfer.methodMemberOrdinal = methodMemberOrdinal;
        transfer.scopeOrdinal = scopeOrdinal;
        transfer.sourceLocalOrdinal =
            resolvedArguments[argumentOrdinal].ownershipSourceLocalOrdinal;
        transfer.sourceParameterOrdinal =
            resolvedArguments[argumentOrdinal].ownershipSourceParameterOrdinal;
        transfer.targetMemberOrdinal = selected.memberOrdinal;
        transfer.targetParameterOrdinal = parameterOrdinal;
        transfer.typeKey = selectedMember.parameters[parameterOrdinal].typeKey;
        if (transfer.sourceLocalOrdinal.has_value())
            transfer.kind = MethodOwnershipTransferKind::MoveLocalToOwnedParameter;
        else if (transfer.sourceParameterOrdinal.has_value())
            transfer.kind = MethodOwnershipTransferKind::MoveParameterToOwnedParameter;
        else
            transfer.kind = MethodOwnershipTransferKind::OwnedReturnToOwnedParameter;
        ownershipTransferBindings_.push_back(std::move(transfer));
    }
}

// EN: Freeze (item 130b). A by-value parameter of an `extern C` import must be ABI-trivial: native code
//     cannot run a destructor or release owned fields, so a struct that has either is rejected at the
//     call. Trivial structs, pointers and handles keep compiling.
// ES: Congelamiento (elemento 130b). Un parametro por valor de un import `extern C` debe ser
//     ABI-trivial: el codigo nativo no puede correr un destructor ni liberar campos owned, asi que un
//     struct que tiene alguno se rechaza en la llamada. Los structs triviales, punteros y handles siguen
//     compilando.
void ProjectMethodBodyModel::rejectNonTrivialImportedCArguments(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    const ClassMemberSymbol& selectedMember,
    bool coreStringForwarding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!selectedMember.isImportedC || memberModel_ == nullptr) return;
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMember.parameters.size()) continue;
        const MethodParameterSymbol& parameter = selectedMember.parameters[parameterOrdinal];
        if (parameter.passKind != ParameterPassKind::Value ||
            !memberModel_->typeRequiresValueDrop(parameter.typeKey)) continue;
        const ast::AstNode* argumentSyntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
            "extern C parameter '" + parameter.name + "' of '" + selectedMember.name + "' takes '" +
                parameter.typeKey + "' by value, but native code cannot run its destructor or release its "
                "owned fields; pass an ABI-trivial struct, a pointer or a handle");
    }
}

// EN: Freeze C-01 (plan item 131b). `(new T(...)).Method()` calls an instance method on an object that no
//     owner holds: the method receives a borrowed `this` and may let it escape, and nothing destroys the object
//     afterwards. The owner comes first (`raii T x = new T(...); x.Method();`).
// ES: Congelamiento C-01 (elemento 131b del plan). `(new T(...)).Metodo()` llama un metodo de instancia
//     sobre un objeto que ningun dueno tiene: el metodo recibe un `this` prestado y puede hacerlo escapar, y nada
//     destruye el objeto despues. Primero va el dueno (`raii T x = new T(...); x.Metodo();`).
void ProjectMethodBodyModel::rejectNewCallReceiver(
    const ExpressionResult& callee,
    const ClassMemberSymbol& selectedMember,
    bool coreStringForwarding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ast::AstNode* receiver = callee.receiverSyntax;
    if (receiver == nullptr || selectedMember.isStatic || coreStringForwarding ||
        receiver->kind() != ast::AstNodeKind::ObjectCreationExpression ||
        !typeKeyIsOwnable(callee.receiverTypeKey))
        return;
    reportLostOwnership(
        receiver, "the receiver of '" + selectedMember.name + "', which does not take ownership of it",
        diagnostics);
}

void ProjectMethodBodyModel::rejectSelectedNewToOrdinaryParameters(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    const ClassMemberSymbol& selectedMember,
    bool coreStringForwarding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMember.parameters.size()) continue;
        const ast::AstNode* argumentSyntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        const ExpressionResult& argument = resolvedArguments[argumentOrdinal];
        if (newReachesOrdinaryParameter(
                selectedMember.parameters[parameterOrdinal], argumentSyntax,
                argument.ownershipFreshOwner, argument.ownershipTransfer, argument.typeKey))
            reportNewToOrdinaryParameter(
                argumentSyntax, selectedMember.parameters[parameterOrdinal], selectedMember.name, diagnostics);
    }
}

void ProjectMethodBodyModel::recordSelectedBorrows(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    const ClassMemberSymbol& selectedMember,
    bool coreStringForwarding,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMember.parameters.size()) continue;
        const bool freshTemporaryBorrow = callIsFullExpressionBorrow(
            selectedMember, parameterOrdinal, resolvedArguments[argumentOrdinal]);
        if (selectedMember.parameters[parameterOrdinal].ownership !=
                sema::OwnershipKind::Borrowed &&
            !freshTemporaryBorrow)
            continue;
        MethodBorrowBinding borrow;
        borrow.syntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        borrow.methodMemberOrdinal = methodMemberOrdinal;
        borrow.scopeOrdinal = scopeOrdinal;
        borrow.sourceLocalOrdinal = resolvedArguments[argumentOrdinal].borrowedLocalOrdinal.has_value()
            ? resolvedArguments[argumentOrdinal].borrowedLocalOrdinal
            : resolvedArguments[argumentOrdinal].ownershipSourceLocalOrdinal;
        borrow.sourceParameterOrdinal =
            resolvedArguments[argumentOrdinal].ownershipSourceParameterOrdinal;
        borrow.targetMemberOrdinal = selected.memberOrdinal;
        borrow.targetParameterOrdinal = parameterOrdinal;
        borrow.typeKey = selectedMember.parameters[parameterOrdinal].typeKey;
        if (freshTemporaryBorrow)
            borrow.kind = MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter;
        else if (resolvedArguments[argumentOrdinal].borrowedLocalOrdinal.has_value())
            borrow.kind = MethodBorrowKind::BorrowedLocalToBorrowedParameter;
        else if (borrow.sourceLocalOrdinal.has_value())
            borrow.kind = MethodBorrowKind::OwnerLocalToBorrowedParameter;
        else if (borrow.sourceParameterOrdinal.has_value())
        {
            const ClassMemberSymbol* callerMember = methodSymbol(methodMemberOrdinal);
            if (callerMember != nullptr &&
                *borrow.sourceParameterOrdinal < callerMember->parameters.size() &&
                callerMember->parameters[*borrow.sourceParameterOrdinal].ownership ==
                    sema::OwnershipKind::Owned)
                borrow.kind = MethodBorrowKind::OwnedParameterToBorrowedParameter;
            else
                borrow.kind = MethodBorrowKind::BorrowedParameterToBorrowedParameter;
        }
        else
            borrow.kind = MethodBorrowKind::BorrowedReturnToBorrowedParameter;
        borrowBindings_.push_back(std::move(borrow));
    }
}

void ProjectMethodBodyModel::finalizeSelectedCallResult(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const MethodCallBinding& call,
    bool nullConditionalCall,
    std::optional<std::size_t> provenVariantCase,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    result.typeKey = call.resultTypeKey;
    result.valid = !isErrorType(result.typeKey);
    result.flowActiveVariantCase = provenVariantCase;
    bool defaultValueReceiver = false;
    if (call.receiverSyntax != nullptr &&
        call.receiverSyntax->kind() == ast::AstNodeKind::DefaultValueExpression)
    {
        const auto ordinal = namedTypeOrdinalForTypeKey(call.receiverTypeKey);
        const NamedTypeDeclaration* receiver = ordinal.has_value() && nameLookup_ != nullptr
            ? nameLookup_->typeForOrdinal(*ordinal) : nullptr;
        defaultValueReceiver = receiver != nullptr && receiver->kind == NamedTypeDeclarationKind::Struct;
    }
    if (result.valid && !call.staticCall && call.receiverSyntax != nullptr &&
        (typeKeyIsViewLike(result.typeKey) || tupleTypeKeyContainsView(result.typeKey)) &&
        (defaultValueReceiver || call.receiverSyntax->kind() == ast::AstNodeKind::NewExpression ||
         call.receiverSyntax->kind() == ast::AstNodeKind::ObjectCreationExpression ||
         (call.receiverSyntax->kind() == ast::AstNodeKind::CallExpression &&
          !typeKeyIsViewLike(call.receiverTypeKey))))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error, expression->range(),
            "a view cannot borrow from a temporary receiver; bind its owner to a local first");
        result.valid = false;
        return;
    }
    if (!result.valid || !nullConditionalCall) return;
    if (callResultIsNullable(result.typeKey)) return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "a null-conditional method must return a nullable pointer/reference carrier; "
        "value and void results require Optional<T>");
    result.valid = false;
}

bool ProjectMethodBodyModel::callResultIsNullable(std::string_view typeKey) const
{
    if (ProjectTypeConversion::isPointer(typeKey) ||
        ProjectTypeConversion::isNativeArray(typeKey) ||
        typeKey == "builtin:string" || isAnyDelegateTypeKey(typeKey))
        return true;
    // EN: The diagnostic below recommends Optional<T>; accept it. A skipped call yields the zeroed
    //     carrier, which is the empty Optional (LKF-291).
    // ES: El diagnóstico de abajo recomienda Optional<T>; se acepta. Una llamada omitida produce el
    //     carrier en ceros, que es el Optional vacío (LKF-291).
    if (resolveOptionalCarrier(typeKey).has_value()) return true;
    const std::optional<std::size_t> ordinal = referenceTypeOrdinalForTypeKey(typeKey);
    return ordinal.has_value() && inheritanceModel_ != nullptr &&
        (inheritanceModel_->isClass(*ordinal) ||
         (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)));
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::prepareCallCallee(
    const ast::AstNode* expression,
    MethodCallBinding& call,
    ExpressionResult& callee,
    bool& nullConditionalCall,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    call.syntax = expression;
    call.callerMethodMemberOrdinal = methodMemberOrdinal;
    call.scopeOrdinal = scopeOrdinal;
    nullConditionalCall = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression;
    call.explicitThis = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::MemberExpression &&
        expression->first()->first() != nullptr &&
        expression->first()->first()->kind() == ast::AstNodeKind::ThisExpression;

    callee = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics, true, true);
    call.implicitThis = callee.methodGroup && callee.receiverSyntax == nullptr;
    call.baseReceiver = callee.baseReceiver;
    call.receiverSyntax = callee.receiverSyntax;
    call.receiverTypeKey = callee.receiverTypeKey;
    call.receiverTypeOrdinal = callee.receiverTypeOrdinal;
    call.interfaceCall = call.receiverTypeOrdinal.has_value() && interfaceModel_ != nullptr &&
        interfaceModel_->isInterface(*call.receiverTypeOrdinal);
    if (expression->first() != nullptr)
    {
        call.methodName = expression->first()->kind() == ast::AstNodeKind::IdentifierExpression ||
                expression->first()->kind() == ast::AstNodeKind::MemberExpression ||
                expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression
            ? expression->first()->text()
            : "<expression>";
    }

    if (callee.extensionMethodGroup && namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceFunctionReferenceBinding* extension =
                namespaceFunctionModel_->extensionBinding(expression->first());
            extension != nullptr)
        {
            return analyzeNamespaceFunctionCall(
                expression, *extension, methodMemberOrdinal, scopeOrdinal, diagnostics, &callee);
        }
    }
    // NS304-T08-F03: a static extension member (StaticMethodGroup with no receiver) resolves as a
    // direct namespace call, not the instance method resolution.
    if (!callee.extensionMethodGroup && callee.methodGroup && callee.receiverSyntax == nullptr &&
        namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceFunctionReferenceBinding* extension =
                namespaceFunctionModel_->extensionBinding(expression->first());
            extension != nullptr)
        {
            return analyzeNamespaceFunctionCall(
                expression, *extension, methodMemberOrdinal, scopeOrdinal, diagnostics, nullptr);
        }
    }

    if (const auto handled = analyzeEnumRootCall(
            expression, callee, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return *handled;
    if (const auto handled = analyzeIteratorFacadeCall(
            expression, callee, call, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return *handled;
    if (const auto handled = analyzeStructRootCall(
            expression, callee, call, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return *handled;
    return std::nullopt;
}

void ProjectMethodBodyModel::configureCallLambdaTargets(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    MethodCallBinding& call,
    bool& coreStringForwarding,
    std::vector<std::string>& lambdaTargetTypes)
{
    (void)call;
    // Native string instance syntax forwards to a static Core.String method whose first
    // parameter is the receiver.  Named source arguments must never be allowed to bind that
    // compiler-supplied parameter.
    coreStringForwarding = false;
    if (callee.receiverSyntax != nullptr && !callee.receiverTypeKey.empty() &&
        memberModel_ != nullptr && nameLookup_ != nullptr && !callee.methodCandidates.empty())
    {
        coreStringForwarding = std::ranges::all_of(callee.methodCandidates, [&](std::size_t ordinal)
        {
            if (ordinal >= memberModel_->members().size()) return false;
            const ClassMemberSymbol& member = memberModel_->members()[ordinal];
            return member.kind == ClassMemberKind::Method && member.isStatic &&
                coreStringTypeOrdinal_.has_value() &&
                member.declaringTypeOrdinal == *coreStringTypeOrdinal_;
        });
    }

    const bool hasLambdaArguments = std::ranges::any_of(
        expression->children(), [](const ast::AstNode* argument)
        {
            return argument != nullptr && argument->kind() == ast::AstNodeKind::LambdaExpression;
        });
    if (hasLambdaArguments) lambdaTargetTypes.resize(expression->children().size());

    if (hasLambdaArguments && callee.eventMemberOrdinal.has_value() && memberModel_ != nullptr &&
        *callee.eventMemberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& event = memberModel_->members()[*callee.eventMemberOrdinal];
        std::string ignoredReturnType;
        std::vector<std::string> parameterTypes;
        std::size_t ignoredDelegateOrdinal = 0;
        if (delegateSignature(event.declaredTypeKey, ignoredReturnType, parameterTypes, ignoredDelegateOrdinal))
        seedCallLambdaTargets(lambdaTargetTypes, expression, parameterTypes);
    }
    else if (hasLambdaArguments && !callee.methodGroup && isDelegateTypeKey(callee.typeKey))
    {
        std::string ignoredReturnType;
        std::vector<std::string> parameterTypes;
        std::size_t ignoredDelegateOrdinal = 0;
        if (delegateSignature(callee.typeKey, ignoredReturnType, parameterTypes, ignoredDelegateOrdinal))
        seedCallLambdaTargets(lambdaTargetTypes, expression, parameterTypes);
    }
    else if (hasLambdaArguments && callee.methodGroup && memberModel_ != nullptr)
    {
        configureMethodGroupLambdaTargets(
            expression, callee, coreStringForwarding, lambdaTargetTypes);
    }

}

void ProjectMethodBodyModel::configureMethodGroupLambdaTargets(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    bool coreStringForwarding,
    std::vector<std::string>& lambdaTargetTypes)
{
    std::vector<std::pair<const ClassMemberSymbol*, NamedArgumentMap>> arityMatches;
    for (const std::size_t candidateOrdinal : callee.methodCandidates)
    {
        if (candidateOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
        const NamedArgumentMap mapping = mapArgumentsToParameters(
            expression->children(), candidate, coreStringForwarding ? 1U : 0U);
        if (candidate.kind == ClassMemberKind::Method && mapping.valid)
            arityMatches.emplace_back(&candidate, mapping);
    }
    if (arityMatches.empty()) return;
    for (std::size_t index = 0; index < expression->children().size(); ++index)
    {
        if (expression->children()[index]->kind() != ast::AstNodeKind::LambdaExpression) continue;
        const std::size_t firstParameter =
            arityMatches.front().second.explicitParameterOrdinals[index];
        if (firstParameter >= arityMatches.front().first->parameters.size()) continue;
        const std::string& firstType = arityMatches.front().first->parameters[firstParameter].typeKey;
        const bool agreed = std::ranges::all_of(arityMatches, [&](const auto& candidate)
        {
            const std::size_t parameter = candidate.second.explicitParameterOrdinals[index];
            return parameter < candidate.first->parameters.size() &&
                candidate.first->parameters[parameter].typeKey == firstType;
        });
        if (agreed && isDelegateTypeKey(firstType))
        {
            lambdaTargetTypes[index] = firstType;
            continue;
        }

        // E11/R5: a lambda can only convert to a delegate, so non-delegate overloads
        // cannot make its target ambiguous. Explicit parameter types narrow compatible
        // delegate signatures further. An untyped lambda still does not guess between
        // two distinct delegate targets: it receives a context only when exactly one
        // delegate type remains.
        std::vector<std::string> compatibleDelegateTypes;
        const ast::AstNode* lambda = expression->children()[index];
        for (const auto& candidate : arityMatches)
        {
            const std::size_t parameter = candidate.second.explicitParameterOrdinals[index];
            if (parameter >= candidate.first->parameters.size()) continue;
            const std::string& candidateType = candidate.first->parameters[parameter].typeKey;
            std::string ignoredReturn;
            std::vector<std::string> candidateParameterTypes;
            std::size_t ignoredOrdinal = 0;
            if (!delegateSignature(
                    candidateType, ignoredReturn, candidateParameterTypes, ignoredOrdinal) ||
                candidateParameterTypes.size() != lambda->children().size())
                continue;

            bool matches = true;
            for (std::size_t lambdaParameter = 0;
                 lambdaParameter < lambda->children().size(); ++lambdaParameter)
            {
                const ast::AstNode* parameterSyntax = lambda->children()[lambdaParameter];
                if (parameterSyntax == nullptr || parameterSyntax->typeSyntax() == nullptr) continue;
                const std::string explicitType = memberModel_->canonicalTypeKey(
                    parameterSyntax->typeSyntax(), *nameLookup_);
                if (explicitType != candidateParameterTypes[lambdaParameter])
                {
                    matches = false;
                    break;
                }
            }
            if (matches && std::ranges::find(
                    compatibleDelegateTypes, candidateType) == compatibleDelegateTypes.end())
                compatibleDelegateTypes.push_back(candidateType);
        }
        if (compatibleDelegateTypes.size() == 1)
            lambdaTargetTypes[index] = compatibleDelegateTypes.front();
    }
}

std::vector<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::prepareResolvedCallArguments(
    const ExpressionResult& callee,
    std::span<const ExpressionResult> arguments,
    bool coreStringForwarding) const
{
    std::vector<ExpressionResult> resolvedArguments;
    resolvedArguments.reserve(arguments.size() + (coreStringForwarding ? 1U : 0U));
    if (coreStringForwarding)
    {
        ExpressionResult receiverArgument;
        receiverArgument.typeKey = callee.receiverTypeKey;
        receiverArgument.valid = true;
        receiverArgument.integerConstant = callee.integerConstant;
        // Treat the receiver as a non-retained view for trusted NativeSharp.Core.String methods.
        receiverArgument.ownership = sema::OwnershipKind::None;
        receiverArgument.ownershipTransfer = false;
        resolvedArguments.push_back(std::move(receiverArgument));
    }
    resolvedArguments.insert(resolvedArguments.end(), arguments.begin(), arguments.end());
    return resolvedArguments;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (const auto handled = analyzePreCallKinds(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return *handled;
    MethodCallBinding call;
    ExpressionResult callee;
    bool nullConditionalCall = false;
    if (const auto handled = prepareCallCallee(
            expression, call, callee, nullConditionalCall,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return *handled;
    bool coreStringForwarding = false;
    std::vector<std::string> lambdaTargetTypes;
    configureCallLambdaTargets(
        expression, callee, call, coreStringForwarding, lambdaTargetTypes);
    std::vector<ExpressionResult> arguments;
    arguments.reserve(expression->children().size());
    bool allArgumentsValid = true;
    if (nullConditionalCall)
    {
        const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
        const OwnershipFlowSnapshot argumentState = analyzeBranchState(branchEntry, [&]
        {
            analyzeCallArguments(
                arguments, allArgumentsValid, expression, callee, call, lambdaTargetTypes,
                coreStringForwarding, methodMemberOrdinal, scopeOrdinal, diagnostics);
        });
        commitBranchJoin(branchEntry, argumentState);
    }
    else
    {
        analyzeCallArguments(
            arguments, allArgumentsValid, expression, callee, call, lambdaTargetTypes,
            coreStringForwarding, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }

    if (const auto handled = analyzeSpecialCallKinds(
            expression, callee, arguments, allArgumentsValid, call,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return *handled;
    if (!callee.methodGroup)
    {
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        if (callee.valid)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "call target is not an instance method group");
        }
        return result;
    }

    if (!allArgumentsValid)
    {
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return result;
    }

    // Stage 51 native string/primitive instance facade.  A call such as text.Trim()
    // resolves to the static Core.String signature Trim(string).  The receiver is
    // parameter zero for overload resolution and lowering, but is never an ownership
    // transfer merely because instance syntax was used.
    call.forwardedReceiver = coreStringForwarding;
    if (coreStringForwarding) call.interfaceCall = false;

    std::vector<ExpressionResult> resolvedArguments = prepareResolvedCallArguments(
        callee, arguments, coreStringForwarding);
    // NSC-LIMIT-021 / T41. A fresh string owner may be observed through the same string parameter
    // that already accepts a named RAII string. The hidden owner lives through the complete call
    // expression and is destroyed afterwards (or during unwind). This applies uniformly to
    // methods, interface dispatch and free functions; otherwise merely changing the callable
    // container changes whether `Log("prefix " + value)` is legal.
    // A literal's bytes have process lifetime. Passing that value to an explicitly borrowed
    // native-string parameter creates no owner and therefore needs neither a move nor a hidden
    // cleanup. Keep the exception tied to the literal type and an explicit borrowed contract.

    std::size_t selectedIndex = 0;
    if (!selectCallCandidate(
            selectedIndex, expression, callee, arguments, resolvedArguments,
            coreStringForwarding, call, diagnostics)) return result;
    return completeSelectedCall(
        expression, callee, arguments, resolvedArguments, coreStringForwarding,
        nullConditionalCall, selectedIndex, call, methodMemberOrdinal, scopeOrdinal, diagnostics);
}

bool ProjectMethodBodyModel::evaluateSyntheticCandidates(
    MethodCallBinding& call,
    const std::vector<ExpressionResult>& arguments)
{
    bool hasMatchingArity = false;
    if (memberModel_ == nullptr) return hasMatchingArity;
    const auto resultCarriesReference = [&](std::string_view resultType) {
        std::vector<std::string> pending{std::string(resultType)};
        std::unordered_set<std::string> visited;
        while (!pending.empty())
        {
            std::string key = std::move(pending.back());
            pending.pop_back();
            if (!visited.insert(key).second) continue;
            if (typeKeyCarriesLifetime(key)) return true;
            if (auto elements = ProjectTypeConversion::tupleElementTypeKeys(key))
            {
                for (std::string& element : *elements) pending.push_back(std::move(element));
                continue;
            }
            const auto ordinal = namedTypeOrdinalForTypeKey(key);
            const NamedTypeDeclaration* declaration = ordinal.has_value() && nameLookup_ != nullptr
                ? nameLookup_->typeForOrdinal(*ordinal) : nullptr;
            if (declaration == nullptr || declaration->kind != NamedTypeDeclarationKind::Struct) continue;
            for (const std::size_t memberOrdinal : memberModel_->membersOfType(*ordinal))
            {
                if (memberOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
                if (member.kind == ClassMemberKind::Field && !member.isStatic)
                    pending.push_back(member.declaredTypeKey);
            }
        }
        return false;
    };
    for (const std::size_t candidateOrdinal : call.candidateMemberOrdinals)
    {
        const ClassMemberSymbol* candidatePtr = methodSymbol(candidateOrdinal);
        if (candidatePtr == nullptr || candidatePtr->kind != ClassMemberKind::Method) continue;
        const ClassMemberSymbol& candidate = *candidatePtr;
        MethodCallCandidate candidateBinding;
        candidateBinding.memberOrdinal = candidateOrdinal;
        if (candidate.parameters.size() != arguments.size())
        {
            call.evaluatedCandidates.push_back(std::move(candidateBinding));
            continue;
        }
        hasMatchingArity = true;
        candidateBinding.viable = true;
        for (std::size_t argumentOrdinal = 0; argumentOrdinal < arguments.size(); ++argumentOrdinal)
        {
            const MethodParameterSymbol& parameter = candidate.parameters[argumentOrdinal];
            const bool implicitOperatorReference = candidate.isUserOperator &&
                (parameter.passKind == ParameterPassKind::Ref ||
                 parameter.passKind == ParameterPassKind::In);
            // EN: T19-007. A C++ operator's `const T&` operand binds a temporary, as in C++: the value is
            //     materialized for the call (TemporaryAddress). A `T&` operand still needs an assignable place.
            // ES: T19-007. Un operando `const T&` de un operador C++ enlaza un temporal, como en C++: el valor se
            //     materializa para la llamada (TemporaryAddress). Un operando `T&` sigue exigiendo un lugar asignable.
            const bool cppTemporaryOperand = implicitOperatorReference &&
                (parameter.passKind == ParameterPassKind::In || parameter.cppRvalueReference ||
                 parameter.cppImplicitObject) &&
                namespaceFunctionModel_ != nullptr &&
                candidateOrdinal >= namespaceCallableBase_ &&
                candidateOrdinal - namespaceCallableBase_ < namespaceFunctionModel_->functions().size() &&
                !namespaceFunctionModel_->functions()[candidateOrdinal - namespaceCallableBase_]
                     .cppCanonicalIdentity.empty();
            const std::string_view conversionTarget = implicitOperatorReference
                ? std::string_view(parameter.pointeeTypeKey)
                : std::string_view(parameter.typeKey);
            ProjectImplicitConversion conversion = classifyImplicit(
                arguments[argumentOrdinal].typeKey,
                conversionTarget,
                arguments[argumentOrdinal].integerConstant);
            if (!conversion.exists() ||
                (arguments[argumentOrdinal].readonlyBorrowedObject &&
                 !(implicitOperatorReference && parameter.passKind == ParameterPassKind::In)) ||
                (implicitOperatorReference && conversion.kind != ProjectConversionKind::Identity) ||
                (candidate.isUserOperator && parameter.passKind == ParameterPassKind::In &&
                 !arguments[argumentOrdinal].addressable &&
                 (typeKeyIsViewLike(candidate.declaredTypeKey) ||
                  tupleTypeKeyContainsView(candidate.declaredTypeKey))) ||
                (candidate.isUserOperator && parameter.passKind == ParameterPassKind::In &&
                 !arguments[argumentOrdinal].addressable && memberModel_ != nullptr &&
                 candidate.declaredOwnership != sema::OwnershipKind::Owned &&
                 resultCarriesReference(candidate.declaredTypeKey) &&
                 memberModel_->typeTransitivelyOwnsStorage(arguments[argumentOrdinal].typeKey)) ||
                (candidate.isUserOperator && candidate.isPrecompiled && !candidate.operatorInOperandsNoEscape &&
                 parameter.passKind == ParameterPassKind::In && !arguments[argumentOrdinal].addressable &&
                 memberModel_->typeTransitivelyOwnsStorage(arguments[argumentOrdinal].typeKey)) ||
                (implicitOperatorReference && parameter.passKind != ParameterPassKind::In &&
                 !arguments[argumentOrdinal].addressable && !cppTemporaryOperand) ||
                // T19-007: a `T&&` operand binds only a temporary. / Un operando `T&&` solo enlaza un temporal.
                (parameter.cppRvalueReference && arguments[argumentOrdinal].addressable) ||
                // T19-007: a temporary binds the object operand of a non-const member operator; a const place
                //     does not. / Un temporal enlaza el operando objeto de un operador miembro no const; un lugar
                //     const no.
                (parameter.passKind == ParameterPassKind::Ref && !parameter.cppRvalueReference &&
                 !(cppTemporaryOperand && parameter.cppImplicitObject && !arguments[argumentOrdinal].addressable) &&
                 (!arguments[argumentOrdinal].assignable ||
                  arguments[argumentOrdinal].readonlyReceiver)) ||
                (candidate.isUserOperator && parameter.passKind == ParameterPassKind::Out))
            {
                candidateBinding.viable = false;
            }
            candidateBinding.conversions.push_back(std::move(conversion));
        }
        if (candidateBinding.viable)
        {
            call.viableCandidateMemberOrdinals.push_back(candidateOrdinal);
        }
        call.evaluatedCandidates.push_back(std::move(candidateBinding));
    }
    return hasMatchingArity;
}

std::vector<std::size_t> ProjectMethodBodyModel::viableSyntheticCandidates(
    const MethodCallBinding& call) const
{
    std::vector<std::size_t> viable;
    for (std::size_t index = 0; index < call.evaluatedCandidates.size(); ++index)
    {
        if (call.evaluatedCandidates[index].viable) viable.push_back(index);
    }
    return viable;
}

void ProjectMethodBodyModel::diagnoseSyntheticNoViable(
    const ast::AstNode* syntax,
    const MethodCallBinding& call,
    const std::vector<ExpressionResult>& arguments,
    bool implicitOperatorCall,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // C4: when exactly one overload has the right arity and the ONLY thing wrong is the
    // missing (or extra) `ref`/`out`/`in`, say that instead of "no viable overload" --
    // the type mismatch the programmer sees (`int` vs `int*`) is an artifact of how a
    // by-reference parameter is represented, not something they wrote.
    std::size_t arityMatches = 0;
    std::size_t soleCandidate = 0;
    for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
    {
        if (candidate.conversions.size() != arguments.size()) continue;
        ++arityMatches;
        soleCandidate = candidate.memberOrdinal;
    }
    bool reportedPassKind = false;
    if (arityMatches == 1 && memberModel_ != nullptr &&
        soleCandidate < memberModel_->members().size())
    {
        const ClassMemberSymbol& only = memberModel_->members()[soleCandidate];
        for (std::size_t index = 0;
             !implicitOperatorCall && !only.isUserOperator && index < arguments.size() &&
                 index < only.parameters.size();
             ++index)
        {
            if (only.parameters[index].passKind == arguments[index].argumentPassKind) continue;
            reportedPassKind = true;
            break;
        }
        if (reportedPassKind)
        {
            checkArgumentPassKinds(
                syntax, call.methodName, soleCandidate, arguments, diagnostics);
        }
    }
    if (!reportedPassKind)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "no viable Stage 38 overload of '" + call.methodName +
                "' accepts the operand/argument types");
    }
}

std::vector<std::size_t> ProjectMethodBodyModel::nonDominatedSyntheticCandidates(
    const MethodCallBinding& call,
    const std::vector<std::size_t>& viableEvaluationOrdinals) const
{
    std::vector<std::size_t> nonDominated;
    for (const std::size_t candidateIndex : viableEvaluationOrdinals)
    {
        bool dominated = false;
        for (const std::size_t otherIndex : viableEvaluationOrdinals)
        {
            if (candidateIndex == otherIndex) continue;
            if (!betterCandidate(
                    call.evaluatedCandidates[otherIndex],
                    call.evaluatedCandidates[candidateIndex])) continue;
            dominated = true;
            break;
        }
        if (!dominated) nonDominated.push_back(candidateIndex);
    }
    return nonDominated;
}

bool ProjectMethodBodyModel::validateSyntheticReadonlyReceiver(
    const ast::AstNode* syntax,
    MethodCallBinding& call,
    const MethodCallCandidate& selected,
    bool diagnoseFailure,
    bool readonlyReceiver,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (memberModel_ == nullptr || selected.memberOrdinal >= memberModel_->members().size())
    {
        return true;
    }
    const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
    if (!readonlyReceiver || !selectedMember.declaringTypeIsStruct ||
        selectedMember.isStatic || selectedMember.isReadonly) return true;
    if (diagnoseFailure)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "readonly receiver cannot invoke mutating struct member '" +
                selectedMember.name + "'");
    }
    call.resolution = MethodCallResolutionKind::NoViableCandidate;
    recordCallBinding(std::move(call));
    return false;
}

// EN: Freeze P3: an indexer accessor of a closed generic type may carry a member `where`
//     (`get where T : copy`). The synthetic call that selects it reports the deferred constraint
//     failure at the access, exactly as an ordinary call does. The getter bound only for the
//     target of a plain `=` (activeUnionWriteTarget_, see analyzeAssignmentLeft) is exempt: that
//     store runs the setter, unless the getter returns a reference and the store writes through it.
// ES: Congelamiento P3: un accesor de indexador de un tipo generico cerrado puede llevar un
//     `where` de miembro (`get where T : copy`). La llamada sintetica que lo elige reporta en el
//     acceso el constraint aplazado que falla, igual que una llamada ordinaria. El getter ligado
//     solo para el destino de un `=` simple (activeUnionWriteTarget_, ver analyzeAssignmentLeft)
//     queda exento: ese store ejecuta el setter, salvo que el getter devuelva una referencia y el
//     store escriba por ella.
bool ProjectMethodBodyModel::validateSyntheticConstraints(
    const ast::AstNode* syntax,
    MethodCallBinding& call,
    const MethodCallCandidate& selected,
    bool diagnoseFailure,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (genericConstraintModel_ == nullptr || memberModel_ == nullptr ||
        selected.memberOrdinal >= memberModel_->members().size()) return true;
    const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
    if (syntax != nullptr && syntax == activeUnionWriteTarget_ && !selectedMember.isReferenceReturn)
        return true;
    const ast::AstNode* member = selectedMember.syntax;
    if (genericConstraintModel_->specializationSatisfiesConstraints(member)) return true;
    if (diagnoseFailure)
        genericConstraintModel_->reportSpecializationConstraintViolations(
            member, syntax != nullptr ? syntax->range() : SourceRange{}, diagnostics);
    call.resolution = MethodCallResolutionKind::NoViableCandidate;
    recordCallBinding(std::move(call));
    return false;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::finishSyntheticCall(
    const ast::AstNode* syntax,
    MethodCallBinding& call,
    const MethodCallCandidate& selected,
    const std::vector<ExpressionResult>& arguments,
    bool diagnoseFailure,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (diagnoseFailure && memberModel_ != nullptr &&
        selected.memberOrdinal < memberModel_->members().size() &&
        !memberModel_->members()[selected.memberOrdinal].isUserOperator)
    {
        checkArgumentPassKinds(
            syntax, call.methodName, selected.memberOrdinal, arguments, diagnostics);
    }
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedMemberOrdinal = selected.memberOrdinal;
    call.selectedConversions = selected.conversions;
    const ClassMemberSymbol* memberPtr = methodSymbol(selected.memberOrdinal);
    if (memberPtr != nullptr)
    {
        call.resultTypeKey = memberPtr->declaredTypeKey;
        call.staticCall = memberPtr->isStatic;
        // EN: Freeze P3: an accessor declared `ref readonly` (see configureSelectedCallBinding).
        // ES: Congelamiento P3: un accesor declarado `ref readonly` (ver
        //     configureSelectedCallBinding).
        const bool readonlyReference = isReadonlyReferenceReturn(*memberPtr);
        result.assignable = memberPtr->isReferenceReturn && !readonlyReference;
        result.addressable = memberPtr->isReferenceReturn;
        result.readonlyReceiver = readonlyReference;
        // EN: Freeze C-01: a user operator or conversion that declares an `owned` return
        //     transfers ownership exactly like an ordinary owned-returning call, so
        //     `raii T sum = a + b;` takes ownership of the operator's new object.
        // ES: Congelamiento C-01: un operador o conversion de usuario con retorno `owned`
        //     transfiere la propiedad igual que una llamada ordinaria, asi que
        //     `raii T sum = a + b;` toma la propiedad del objeto nuevo del operador.
        if (memberPtr->declaredOwnership == sema::OwnershipKind::Owned)
        {
            result.ownership = sema::OwnershipKind::Owned;
            result.ownershipTransfer = true;
            result.ownershipFreshOwner = true;
        }
        // EN: Freeze C-13: an accessor or a user operator is a call to its `throws E` member.
        // ES: Congelamiento C-13: un accesor o un operador de usuario es una llamada a su miembro
        //     `throws E`.
        if (diagnoseFailure)
            requireDeclaredErrorHandled(
                syntax, memberPtr->declaredErrorTypeKey, memberPtr->name,
                call.callerMethodMemberOrdinal, diagnostics);
        if (call.staticCall)
        {
            call.implicitThis = false;
            call.explicitThis = false;
            call.baseReceiver = false;
            call.receiverSyntax = nullptr;
            call.receiverTypeKey.clear();
            call.receiverTypeOrdinal.reset();
            call.interfaceCall = false;
        }
    }
    result.typeKey = call.resultTypeKey;
    result.valid = !isErrorType(result.typeKey);
    recordCallBinding(std::move(call));
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::resolveSyntheticCall(
    const ast::AstNode* syntax,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::string methodName,
    std::vector<std::size_t> candidates,
    const std::vector<ExpressionResult>& arguments,
    const ast::AstNode* receiverSyntax,
    std::string receiverTypeKey,
    std::optional<std::size_t> receiverTypeOrdinal,
    bool baseReceiver,
    bool interfaceCall,
    diagnostics::DiagnosticEngine& diagnostics,
    bool diagnoseFailure,
    bool readonlyReceiver)
{
    ExpressionResult result;
    MethodCallBinding call;
    call.syntax = syntax;
    call.callerMethodMemberOrdinal = methodMemberOrdinal;
    call.scopeOrdinal = scopeOrdinal;
    call.methodName = std::move(methodName);
    call.receiverSyntax = receiverSyntax;
    call.receiverTypeKey = std::move(receiverTypeKey);
    call.receiverTypeOrdinal = receiverTypeOrdinal;
    call.baseReceiver = baseReceiver;
    call.interfaceCall = interfaceCall;
    call.implicitThis = receiverSyntax == nullptr && !candidates.empty();
    call.candidateMemberOrdinals = std::move(candidates);

    const bool implicitOperatorCall =
        language::operatorTraitContractForMethod(call.methodName, arguments.size()) != nullptr;
    if (!evaluateSyntheticCandidates(call, arguments))
    {
        if (diagnoseFailure)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                syntax != nullptr ? syntax->range() : SourceRange{},
                "no Stage 38 overload of '" + call.methodName + "' accepts " +
                    std::to_string(arguments.size()) + " argument(s)");
        }
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return result;
    }

    const std::vector<std::size_t> viable = viableSyntheticCandidates(call);
    if (viable.empty())
    {
        if (diagnoseFailure)
        {
            diagnoseSyntheticNoViable(
                syntax, call, arguments, implicitOperatorCall, diagnostics);
        }
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return result;
    }

    const std::vector<std::size_t> nonDominated =
        nonDominatedSyntheticCandidates(call, viable);
    if (nonDominated.size() != 1)
    {
        if (diagnoseFailure)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                syntax != nullptr ? syntax->range() : SourceRange{},
                "ambiguous Stage 38 call to '" + call.methodName + "'");
        }
        call.resolution = MethodCallResolutionKind::Ambiguous;
        recordCallBinding(std::move(call));
        return result;
    }

    const MethodCallCandidate& selected = call.evaluatedCandidates[nonDominated.front()];
    if (!validateSyntheticReadonlyReceiver(
            syntax, call, selected, diagnoseFailure, readonlyReceiver, diagnostics) ||
        !validateSyntheticConstraints(syntax, call, selected, diagnoseFailure, diagnostics))
        return result;
    return finishSyntheticCall(
        syntax, call, selected, arguments, diagnoseFailure, diagnostics);
}

}
