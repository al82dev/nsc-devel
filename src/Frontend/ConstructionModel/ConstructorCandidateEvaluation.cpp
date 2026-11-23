#include "nativesharp/Language/PointerLevelQualifiers.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "../Internal/NamedConstructorArgumentMap.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/CppOverloadRanking.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using internal::NamedConstructorArgumentMap;

NamedConstructorArgumentMap mapConstructorArguments(
    std::span<ast::AstNode* const> arguments,
    const ClassMemberSymbol& constructor)
{
    NamedConstructorArgumentMap result;
    if (!constructor.parameters.empty() && constructor.parameters.back().isParameterPack)
        result.parameterPackOrdinal = constructor.parameters.size() - 1U;
    std::vector<bool> assigned(constructor.parameters.size(), false);
    std::size_t nextPositional = 0;
    bool reachedExpandedParameterPack = false;
    bool assignedParameterPackDirectly = false;
    for (const ast::AstNode* argument : arguments)
    {
        if (argument == nullptr) return result;
        std::optional<std::size_t> parameterOrdinal;
        bool parameterPackElement = false;
        if (!argument->argumentName().empty())
        {
            if (reachedExpandedParameterPack) return result;
            for (std::size_t index = 0; index < constructor.parameters.size(); ++index)
            {
                if (constructor.parameters[index].name == argument->argumentName())
                {
                    parameterOrdinal = index;
                    break;
                }
            }
            if (parameterOrdinal == result.parameterPackOrdinal)
            {
                if (assignedParameterPackDirectly) return result;
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
                    if (assignedParameterPackDirectly) return result;
                    parameterPackElement = true;
                    reachedExpandedParameterPack = true;
                }
            }
        }
        if (!parameterOrdinal.has_value() ||
            (assigned[*parameterOrdinal] && !parameterPackElement)) return result;
        if (!parameterPackElement) assigned[*parameterOrdinal] = true;
        result.explicitParameterOrdinals.push_back(*parameterOrdinal);
        result.parameterPackElements.push_back(parameterPackElement);
        result.usesExpandedParameterPack =
            result.usesExpandedParameterPack || parameterPackElement;
        while (nextPositional < assigned.size() && assigned[nextPositional]) ++nextPositional;
    }
    for (std::size_t index = 0; index < constructor.parameters.size(); ++index)
    {
        if (index == result.parameterPackOrdinal)
        {
            result.synthesizesEmptyParameterPack =
                !result.usesExpandedParameterPack && !assignedParameterPackDirectly;
            continue;
        }
        if (assigned[index]) continue;
        if (constructor.parameters[index].defaultValueSyntax == nullptr) return result;
        result.defaultedParameterOrdinals.push_back(index);
    }
    result.valid = true;
    return result;
}

bool isStringLiteralBorrow(
    const MethodParameterSymbol& parameter,
    const MethodExpressionTypeBinding& argument) noexcept
{
    return parameter.ownership == sema::OwnershipKind::Borrowed &&
        parameter.typeKey == "builtin:string" && argument.typeKey == "string-literal";
}

bool isFreshOwnedStringBorrow(
    const MethodParameterSymbol& parameter,
    const MethodExpressionTypeBinding& argument) noexcept
{
    return parameter.ownership == sema::OwnershipKind::Borrowed &&
        parameter.typeKey == "builtin:string" &&
        argument.ownership == sema::OwnershipKind::Owned &&
        argument.ownershipTransfer && argument.ownershipFreshOwner;
}

}

void ProjectConstructionModel::preferWholeParameterPackConversion(
    const ClassMemberSymbol& constructor,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    const ProjectConversionClassifier& conversions,
    ConstructorCandidateBinding& candidate) const
{
    if (!candidate.usesExpandedParameterPack ||
        std::ranges::count(candidate.parameterPackElements, true) != 1 ||
        !candidate.parameterPackOrdinal.has_value())
    {
        return;
    }
    const auto element = std::ranges::find(candidate.parameterPackElements, true);
    const std::size_t argumentOrdinal = static_cast<std::size_t>(
        std::distance(candidate.parameterPackElements.begin(), element));
    const MethodParameterSymbol& pack =
        constructor.parameters[*candidate.parameterPackOrdinal];
    if (conversions.classifyImplicit(
            argumentTypes[argumentOrdinal].typeKey,
            pack.typeKey,
            argumentTypes[argumentOrdinal].integerConstant).exists())
    {
        candidate.parameterPackElements[argumentOrdinal] = false;
        candidate.usesExpandedParameterPack = false;
    }
}

void ProjectConstructionModel::evaluateConstructorArgument(
    const ClassMemberSymbol& constructor,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    std::size_t argumentOrdinal,
    const ProjectConversionClassifier& conversions,
    ConstructorCandidateBinding& candidate) const
{
    const std::size_t parameterOrdinal = candidate.argumentParameterOrdinals[argumentOrdinal];
    const bool packElement = argumentOrdinal < candidate.parameterPackElements.size() &&
        candidate.parameterPackElements[argumentOrdinal];
    const MethodParameterSymbol& parameter = constructor.parameters[parameterOrdinal];
    // EN: T19-007. For a C++ `T&&` parameter, an argument that is not the referent's address (`ref x`) is a value:
    //     it converts to the referent type and is materialized; an lvalue of that very type is rejected, as in C++.
    // ES: T19-007. Para un parametro C++ `T&&`, un argumento que no es la direccion del referente (`ref x`) es un
    //     valor: se convierte al tipo del referente y se materializa; un lvalue de ese mismo tipo se rechaza.
    const bool rvalueArgument = !packElement && parameter.cppRvalueReference &&
        argumentTypes[argumentOrdinal].typeKey != parameter.typeKey;
    std::string conversionTarget = rvalueArgument ? parameter.pointeeTypeKey : parameter.typeKey;
    if (packElement)
    {
        if (parameter.parameterPackElementTypeKey.empty()) candidate.viable = false;
        else conversionTarget = parameter.parameterPackElementTypeKey;
    }
    ProjectImplicitConversion conversion = conversions.classifyImplicit(
        argumentTypes[argumentOrdinal].typeKey,
        conversionTarget,
        argumentTypes[argumentOrdinal].integerConstant);
    if (!conversion.exists()) candidate.viable = false;
    if (rvalueArgument && conversion.exists() && conversion.kind == ProjectConversionKind::Identity &&
        sema::isPlaceCategory(argumentTypes[argumentOrdinal].valueCategory))
        candidate.viable = false;
    if (argumentTypes[argumentOrdinal].pointerPointeeVolatile &&
        (packElement || !parameter.pointerPointeeVolatile)) candidate.viable = false;
    // EN: X2-04. An `in` argument is readonly: it never binds a `ref` parameter (`T&`, a move constructor's `T&&`),
    //     as calls already check; `new T(in m)` of a move-only T moved from a readonly object.
    // ES: X2-04. Un argumento `in` es de solo lectura: nunca enlaza un parametro `ref` (`T&`, el `T&&` de un
    //     constructor de movimiento), como ya lo comprueban las llamadas.
    if (!packElement && argumentTypes[argumentOrdinal].argumentPassKind == ParameterPassKind::In &&
        parameter.passKind == ParameterPassKind::Ref) candidate.viable = false;
    // C1-02: a chain keeps every level qualifier, as for calls. / Una cadena conserva cada calificador de nivel.
    if (!packElement && language::pointerDepthOfTypeKey(argumentTypes[argumentOrdinal].typeKey) >= 2 &&
        language::pointerDepthOfTypeKey(parameter.typeKey) >= 2 &&
        language::classifyPointerQualificationConversion(
            {argumentTypes[argumentOrdinal].pointerLevelQualifiers, argumentTypes[argumentOrdinal].pointerPointeeReadonly,
             argumentTypes[argumentOrdinal].pointerPointeeVolatile},
            {parameter.pointerLevelQualifiers, parameter.pointerPointeeReadonly, parameter.pointerPointeeVolatile},
            language::pointerDepthOfTypeKey(parameter.typeKey)) >
            language::PointerQualificationConversion::Allowed)
        candidate.viable = false;
    if (!packElement && !rvalueArgument && argumentTypes[argumentOrdinal].pointerPointeeReadonly &&
        ProjectTypeConversion::isPointer(parameter.typeKey) && !parameter.pointerPointeeReadonly)
    {
        candidate.viable = false;
    }
    // EN: T08-D17: a `T**` argument does not bind to a `T const**` parameter.
    // ES: T08-D17: un argumento `T**` no se enlaza a un parametro `T const**`.
    if (!packElement && argumentTypes[argumentOrdinal].typeKey.size() > 2 &&
        argumentTypes[argumentOrdinal].typeKey.ends_with("**") && parameter.typeKey.ends_with("**") &&
        !argumentTypes[argumentOrdinal].pointerPointeeReadonly && parameter.pointerPointeeReadonly)
        candidate.viable = false;
    // NSC-LIMIT-010. PRIMERO LA FISICA, DESPUES EL CONTRATO, y en ese orden.
    //
    // Un candidato fisicamente imposible no llega a tener opinion sobre propiedad: si la
    // conversion no existe, el argumento ni siquiera cabe en ese parametro, y dejar que
    // hable el calificador produciria un diagnostico sobre `move` cuando el problema es el
    // tipo. El veredicto se calcula igualmente para que `ownershipFits` tenga exactamente
    // un elemento por argumento --el diagnostico indexa por posicion-- pero solo tumba al
    // candidato cuando la fisica ya lo habia aceptado.
    const bool literalBorrow = !packElement &&
        isStringLiteralBorrow(parameter, argumentTypes[argumentOrdinal]);
    const bool freshBorrow = !packElement &&
        isFreshOwnedStringBorrow(parameter, argumentTypes[argumentOrdinal]);
    const OwnershipArgumentFit fit = packElement
        ? (argumentTypes[argumentOrdinal].ownership == sema::OwnershipKind::None &&
           !argumentTypes[argumentOrdinal].ownershipTransfer &&
           !argumentTypes[argumentOrdinal].ownershipFreshOwner
               ? OwnershipArgumentFit::Ok
               : OwnershipArgumentFit::UnqualifiedRejectsOwnership)
        : literalBorrow || freshBorrow
        ? OwnershipArgumentFit::Ok
        : classifyOwnershipArgument(
            methodBodyModel_->typeKeyIsOwnable(parameter.typeKey),
            parameter.ownership,
            argumentTypes[argumentOrdinal].ownership,
            argumentTypes[argumentOrdinal].ownershipTransfer,
            argumentTypes[argumentOrdinal].ownershipFreshOwner,
            argumentTypes[argumentOrdinal].typeKey == "builtin:null");
    candidate.ownershipFits.push_back(fit);
    const bool moveOnlyCopy = !packElement && conversion.exists() &&
        parameter.passKind == ParameterPassKind::Value &&
        memberModel_->typeIsMoveOnly(conversionTarget) &&
        sema::isPlaceCategory(argumentTypes[argumentOrdinal].valueCategory) &&
        !argumentTypes[argumentOrdinal].ownershipTransfer;
    candidate.moveOnlyCopyRejects.push_back(moveOnlyCopy);
    if (freshBorrow) candidate.fullExpressionBorrowArgumentOrdinals.push_back(argumentOrdinal);
    if (conversion.exists() && fit != OwnershipArgumentFit::Ok) candidate.viable = false;
    if (moveOnlyCopy) candidate.viable = false;
    if (fit == OwnershipArgumentFit::Ok && !freshBorrow &&
        argumentTypes[argumentOrdinal].ownershipTransfer)
    {
        ConstructorOwnershipTransfer transfer;
        transfer.parameterOrdinal = parameterOrdinal;
        transfer.sourceLocalOrdinal = argumentTypes[argumentOrdinal].ownershipSourceLocalOrdinal;
        transfer.sourceParameterOrdinal = argumentTypes[argumentOrdinal].ownershipSourceParameterOrdinal;
        candidate.ownershipTransfers.push_back(transfer);
    }
    candidate.conversions.push_back(std::move(conversion));
}

ConstructorCandidateBinding ProjectConstructionModel::evaluateConstructorCandidate(
    const ClassMemberSymbol& constructor,
    std::size_t constructorOrdinal,
    bool accessible,
    std::span<ast::AstNode* const> arguments,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    const ProjectConversionClassifier& conversions,
    bool& matchingArity) const
{
    ConstructorCandidateBinding candidate;
    candidate.memberOrdinal = constructorOrdinal;
    candidate.accessible = accessible;
    const NamedConstructorArgumentMap argumentMapping =
        mapConstructorArguments(arguments, constructor);
    matchingArity = argumentMapping.valid;
    if (!candidate.accessible || !matchingArity)
    {
        return candidate;
    }
    candidate.viable = true;
    candidate.argumentParameterOrdinals = argumentMapping.explicitParameterOrdinals;
    candidate.defaultedParameterOrdinals = argumentMapping.defaultedParameterOrdinals;
    candidate.parameterPackElements = argumentMapping.parameterPackElements;
    candidate.parameterPackOrdinal = argumentMapping.parameterPackOrdinal;
    candidate.usesExpandedParameterPack = argumentMapping.usesExpandedParameterPack;
    candidate.synthesizesEmptyParameterPack = argumentMapping.synthesizesEmptyParameterPack;
    preferWholeParameterPackConversion(constructor, argumentTypes, conversions, candidate);
    candidate.defaultedParameterCount = argumentMapping.defaultedParameterOrdinals.size();
    for (std::size_t argumentOrdinal = 0; argumentOrdinal < argumentTypes.size(); ++argumentOrdinal)
    {
        evaluateConstructorArgument(
            constructor, argumentTypes, argumentOrdinal, conversions, candidate);
    }
    return candidate;
}

std::vector<std::size_t> ProjectConstructionModel::viableConstructorCandidates(
    const std::vector<ConstructorCandidateBinding>& candidates) const
{
    std::vector<std::size_t> viable;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (candidates[index].viable)
        {
            viable.push_back(index);
        }
    }
    return viable;
}

std::vector<std::size_t> ProjectConstructionModel::nonDominatedConstructorCandidates(
    const std::vector<ConstructorCandidateBinding>& candidates,
    std::span<const std::size_t> viable) const
{
    // EN: T19-006. Two C++ constructors that NativeSharp ranks alike (`S(int)` and `S(long)` both take an int)
    //     are ordered by the C++ conversion of each argument, as for C++ methods.
    // ES: T19-006. Dos constructores C++ que NativeSharp ordena igual (`S(int)` y `S(long)` reciben un int) se
    //     ordenan por la conversion C++ de cada argumento, como los metodos C++.
    const auto cppBetter = [&](const ConstructorCandidateBinding& left, const ConstructorCandidateBinding& right) {
        if (memberModel_ == nullptr || !left.memberOrdinal || !right.memberOrdinal ||
            *left.memberOrdinal >= memberModel_->members().size() ||
            *right.memberOrdinal >= memberModel_->members().size()) return false;
        const ClassMemberSymbol& l = memberModel_->members()[*left.memberOrdinal];
        const ClassMemberSymbol& r = memberModel_->members()[*right.memberOrdinal];
        if (!l.isCppExternal || !r.isCppExternal || l.declaringTypeOrdinal != r.declaringTypeOrdinal ||
            l.cppParameterSpellings.size() != l.parameters.size() ||
            r.cppParameterSpellings.size() != r.parameters.size() ||
            left.argumentParameterOrdinals != right.argumentParameterOrdinals ||
            left.conversions.size() != left.argumentParameterOrdinals.size() ||
            right.conversions.size() != right.argumentParameterOrdinals.size() ||
            left.defaultedParameterCount != 0 || right.defaultedParameterCount != 0 ||
            left.usesExpandedParameterPack || right.usesExpandedParameterPack) return false;
        std::vector<std::string> sources;
        for (const auto& conversion : left.conversions) sources.push_back(conversion.sourceTypeKey);
        const auto ranked = compareCppArithmeticSpellings(l.cppParameterSpellings, r.cppParameterSpellings,
                                                          left.argumentParameterOrdinals, sources);
        return ranked.value_or(false);
    };
    std::vector<std::size_t> nonDominated;
    for (const std::size_t index : viable)
    {
        bool dominated = false;
        for (const std::size_t other : viable)
        {
            if (index != other && (betterCandidate(candidates[other], candidates[index]) ||
                (!betterCandidate(candidates[index], candidates[other]) &&
                 cppBetter(candidates[other], candidates[index]))))
            {
                dominated = true;
                break;
            }
        }
        if (!dominated)
        {
            nonDominated.push_back(index);
        }
    }
    return nonDominated;
}

std::optional<MethodExpressionTypeBinding> ProjectConstructionModel::expressionType(
    const ast::AstNode* syntax,
    std::size_t callerMemberOrdinal) const
{
    if (methodBodyModel_ == nullptr || syntax == nullptr)
    {
        return std::nullopt;
    }
    const MethodExpressionTypeBinding* binding =
        methodBodyModel_->expressionTypeBinding(syntax, callerMemberOrdinal);
    return binding != nullptr
        ? std::optional<MethodExpressionTypeBinding>(*binding)
        : std::nullopt;
}

bool ProjectConstructionModel::explainOwnershipRejection(
    const std::vector<ConstructorCandidateBinding>& candidates,
    std::size_t argumentCount,
    const ast::AstNode* argumentListSyntax,
    const ast::AstNode* fallbackSyntax,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr) return false;
    const ConstructorCandidateBinding* unico = nullptr;
    std::size_t conAridad = 0;
    for (const ConstructorCandidateBinding& candidato : candidates)
    {
        if (!candidato.accessible || candidato.ownershipFits.size() != argumentCount ||
            candidato.moveOnlyCopyRejects.size() != argumentCount) continue;
        ++conAridad;
        unico = &candidato;
    }
    if (conAridad != 1 || unico == nullptr || !unico->memberOrdinal.has_value()) return false;
    if (*unico->memberOrdinal >= memberModel_->members().size()) return false;

    // La FISICA tiene que estar completa. Si alguna conversion no existe, el estorbo son los
    // tipos y hablar de `move` mandaria al lector al sitio equivocado.
    for (const ProjectImplicitConversion& conversion : unico->conversions)
    {
        if (!conversion.exists()) return false;
    }
    bool algunaObjecion = false;
    for (const OwnershipArgumentFit fit : unico->ownershipFits)
    {
        algunaObjecion = algunaObjecion || fit != OwnershipArgumentFit::Ok;
    }
    algunaObjecion = algunaObjecion || std::ranges::any_of(
        unico->moveOnlyCopyRejects, [](bool rejected) { return rejected; });
    if (!algunaObjecion) return false;

    const ClassMemberSymbol& elegido = memberModel_->members()[*unico->memberOrdinal];
    if (unico->argumentParameterOrdinals.size() != argumentCount) return false;
    for (const std::size_t parameterOrdinal : unico->argumentParameterOrdinals)
    {
        if (parameterOrdinal >= elegido.parameters.size()) return false;
    }
    for (std::size_t index = 0; index < unico->ownershipFits.size(); ++index)
    {
        if (unico->ownershipFits[index] == OwnershipArgumentFit::Ok &&
            !unico->moveOnlyCopyRejects[index]) continue;
        const ast::AstNode* argumento =
            argumentListSyntax != nullptr && index < argumentListSyntax->children().size()
                ? argumentListSyntax->children()[index]
                : nullptr;
        const SourceRange range = argumento != nullptr
            ? argumento->range()
            : (fallbackSyntax != nullptr ? fallbackSyntax->range() : SourceRange{});
        const auto& parameter = elegido.parameters[unico->argumentParameterOrdinals[index]];
        diagnostics.report(
            unico->moveOnlyCopyRejects[index]
                ? diagnostics::DiagnosticId::NS3020InvalidValueCategory
                : diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            range,
            unico->moveOnlyCopyRejects[index]
                ? "argument " + std::to_string(index + 1) +
                    ": move-only value cannot be copied into constructor parameter '" +
                    parameter.name + "'; use move(argument)"
                : "argument " + std::to_string(index + 1) + ": " +
                    ownershipArgumentFitMessage(
                        unico->ownershipFits[index],
                        parameter.name,
                        parameter.typeKey));
    }
    return true;
}

bool ProjectConstructionModel::betterCandidate(
    const ConstructorCandidateBinding& left,
    const ConstructorCandidateBinding& right) const noexcept
{
    if (!left.viable || !right.viable || left.conversions.size() != right.conversions.size())
    {
        return false;
    }
    bool strictlyBetter = false;
    for (std::size_t index = 0; index < left.conversions.size(); ++index)
    {
        if (left.conversions[index].rank > right.conversions[index].rank)
        {
            return false;
        }
        if (left.conversions[index].rank < right.conversions[index].rank)
        {
            strictlyBetter = true;
        }
    }
    if (!strictlyBetter && left.conversions.size() == 1 && memberModel_ &&
        left.memberOrdinal && right.memberOrdinal &&
        *left.memberOrdinal < memberModel_->members().size() &&
        *right.memberOrdinal < memberModel_->members().size())
    {
        const auto& lhs = memberModel_->members()[*left.memberOrdinal];
        const auto& rhs = memberModel_->members()[*right.memberOrdinal];
        const auto& lc = left.conversions[0];
        const auto& rc = right.conversions[0];
        if (lhs.isCppExternal && rhs.isCppExternal && lhs.parameters.size() == 1 && rhs.parameters.size() == 1 &&
            left.argumentParameterOrdinals.size() == 1 && right.argumentParameterOrdinals.size() == 1 &&
            left.argumentParameterOrdinals[0] == 0 && right.argumentParameterOrdinals[0] == 0 &&
            lc.rank == rc.rank && lc.sourceTypeKey == lc.targetTypeKey && lc.targetTypeKey == rc.targetTypeKey &&
            ProjectTypeConversion::isPointer(lc.sourceTypeKey) &&
            !lhs.parameters[0].pointerPointeeReadonly && rhs.parameters[0].pointerPointeeReadonly)
            return true;
    }
    if (!strictlyBetter && left.defaultedParameterCount != right.defaultedParameterCount)
    {
        return left.defaultedParameterCount < right.defaultedParameterCount;
    }
    if (!strictlyBetter &&
        left.usesExpandedParameterPack != right.usesExpandedParameterPack)
    {
        return !left.usesExpandedParameterPack;
    }
    return strictlyBetter;
}

}
