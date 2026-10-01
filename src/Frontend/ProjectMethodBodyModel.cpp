#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kGenericRawNewAuxiliary = "stage52-generic-raw-new";
}

// NSC-LIMIT-010. LA REGLA DE COMPATIBILIDAD DE PROPIEDAD, ESCRITA UNA SOLA VEZ.
//
// Estaba solo dentro de `analyzeCall`, y `ProjectConstructionModel::bindConstruction` elegia
// constructor mirando nada mas tipos y conversiones fisicas. Por eso `new Caja(move(duenio))` no
// encontraba el constructor `owned`: fisicamente todos los candidatos valian, y no habia con que
// desempatarlos. Sacada aqui, los dos resolutores contestan lo mismo por construccion.
//
// El orden de las comprobaciones NO es libre. `parameterOwnable` va PRIMERO: un `owned T`
// monomorfizado sobre un tipo valor lleva el calificador escrito y no tiene nada que poseer, asi
// que cualquier forma de argumento vale. Comprobar el calificador antes que la ownabilidad
// rechazaba `LinkedList<int>.AddFirst`, que pasa un `move` de un `int` a un parametro `owned T` --
// medido en Stage 52 y arreglado entonces; aqui se conserva ese orden intacto.
OwnershipArgumentFit classifyOwnershipArgument(
    bool parameterOwnable,
    sema::OwnershipKind parameterOwnership,
    sema::OwnershipKind argumentOwnership,
    bool argumentTransfer,
    bool argumentIsFreshOwner) noexcept
{
    // LA HISTORIA COMPLETA DE ESE ORDEN, medida en Stage 52 y conservada aqui porque no esta
    // escrita en ningun otro sitio:
    //
    // Stage 52 fix: the three branches below must be mutually exclusive on
    // parameterOwnable itself, not just on parameterOwnership -- the ORIGINAL
    // shape of this chain gated the Owned and Borrowed cases on
    // "parameterOwnership == X && parameterOwnable", but then fell through to a
    // plain "else if" for everything else, which included the (Owned,
    // !parameterOwnable) and (Borrowed, !parameterOwnable) combinations too --
    // i.e. a value-typed "owned T"/"borrowed T" parameter (T monomorphized to
    // int, bool, a struct, ...) whose caller passed an EXPLICIT move(...)
    // argument (ownershipTransfer=true) landed in the catch-all
    // "unqualified parameter forbids ownership-transfer argument" branch and was
    // wrongly rejected as non-viable -- discovered via LinkedList<int>.AddFirst's
    // "created[0].SetValue(move(value))" (a plain value(int)-returning move on an
    // owned int parameter, calling another owned-int method) reporting "no viable
    // overload of 'SetValue'". Gating on !parameterOwnable FIRST, uniformly
    // bypassing every ownership check, fixes this for Owned AND Borrowed alike.
    if (!parameterOwnable)
    {
        // Estructuralmente inerte: no hay propiedad que transferir ni que prestar.
        return OwnershipArgumentFit::Ok;
    }
    if (parameterOwnership == sema::OwnershipKind::Owned)
    {
        // DOS FORMAS DE SATISFACER UN `owned`, y la segunda faltaba.
        //
        //   1. una TRANSFERENCIA de un owner con nombre: `move(local)`, `move(p.campo)`;
        //   2. un owner RECIEN CREADO que todavia no es de nadie: `new T(...)`.
        //
        // Exigir solo la primera hacia imposible `new Caja(new Carga(7))`, y no por una regla del
        // lenguaje sino por una que no contemplaba el caso: no hay de donde mover, asi que `move`
        // ni siquiera se puede escribir ahi. El parametro `owned` se queda con el valor en las
        // dos formas, que es lo unico que la regla tiene que garantizar.
        const bool transfers =
            argumentOwnership == sema::OwnershipKind::Owned && argumentTransfer;
        if (transfers || argumentIsFreshOwner) return OwnershipArgumentFit::Ok;
        return OwnershipArgumentFit::OwnedRequiresTransfer;
    }
    if (parameterOwnership == sema::OwnershipKind::Borrowed)
    {
        const bool checkedBorrowSource =
            (argumentOwnership == sema::OwnershipKind::Owned ||
             argumentOwnership == sema::OwnershipKind::Borrowed) &&
            !argumentTransfer;
        return checkedBorrowSource
            ? OwnershipArgumentFit::Ok
            : OwnershipArgumentFit::BorrowedRequiresCheckedSource;
    }
    // Parametro sin calificador. Lo que no puede colarse dentro es un CONTRATO: una transferencia
    // o un prestamo se perderian en silencio, que es justo lo que Stage 50 existe para impedir.
    //
    // UN OWNER RECIEN CREADO NO ES UN CONTRATO, y por eso NO se rechaza aqui. Se midio: anadir
    // `argumentIsFreshOwner` a esta condicion --que es lo que el comentario de
    // ExpressionResult::ownershipFreshOwner llego a afirmar que pasaba-- rompe cinco de los
    // ejemplos del libro con `no viable overload of 'Insert'`, sobre la forma mas ordinaria que
    // hay en este lenguaje:
    //
    //     public void Insert(Student value)   // parametro sin calificador
    //     tree.Insert(new Student(50, "Ana", 90));
    //
    // Y rechazarlo seria ademas arbitrario: `Student s = new Student(...); tree.Insert(s);` hace
    // exactamente lo mismo con el mismo objeto y siempre fue legal. Lo que un parametro sin
    // calificador no hace es RASTREAR la vida del valor; eso vale igual para un objeto recien
    // creado que para uno que ya existia, y no es algo que esta funcion pueda arreglar.
    //
    // `ownershipFreshOwner` existe para RELAJAR el requisito de un parametro `owned` --que no
    // podia recibir un `new` porque no hay de donde mover-- y no para endurecer este.
    if (argumentTransfer || argumentOwnership == sema::OwnershipKind::Borrowed)
    {
        return OwnershipArgumentFit::UnqualifiedRejectsOwnership;
    }
    return OwnershipArgumentFit::Ok;
}

std::string ownershipArgumentFitMessage(
    OwnershipArgumentFit fit,
    std::string_view parameterName,
    std::string_view parameterTypeKey)
{
    const std::string parameter =
        "parameter '" + std::string(parameterName) + "' of type '" + std::string(parameterTypeKey) + "'";
    switch (fit)
    {
        case OwnershipArgumentFit::OwnedRequiresTransfer:
            return "owned " + parameter +
                " requires an ownership transfer: pass move(...) or a value that already owns "
                "its storage";
        case OwnershipArgumentFit::BorrowedRequiresCheckedSource:
            return "borrowed " + parameter +
                " requires an owned or borrowed source; a moved value cannot be borrowed";
        case OwnershipArgumentFit::UnqualifiedRejectsOwnership:
            return "unqualified " + parameter +
                " cannot receive an ownership transfer or a borrowed value";
        case OwnershipArgumentFit::Ok:
            break;
    }
    return {};
}

namespace
{
using lexer::TokenKind;

sema::OwnershipKind ownershipKindFromSyntax(const ast::TypeSyntax* syntax) noexcept
{
    if (syntax == nullptr)
    {
        return sema::OwnershipKind::None;
    }

    switch (syntax->ownershipQualifier())
    {
        case ast::OwnershipQualifierSyntaxKind::None: return sema::OwnershipKind::None;
        case ast::OwnershipQualifierSyntaxKind::Owned: return sema::OwnershipKind::Owned;
        case ast::OwnershipQualifierSyntaxKind::Borrowed: return sema::OwnershipKind::Borrowed;
    }
    return sema::OwnershipKind::None;
}

// C8 "type root", struct half: the marker an ExpressionResult carries when a member access
// resolved to one of the three members the compiler synthesizes for a struct. It never escapes
// analyzeCall, which is where the core binding is recorded -- see the facade in analyzeMember.
constexpr const char* kStructRootFacadeTypeKey = "<struct-root-facade>";
// ID 129. El gemelo del de arriba para el `ToString()` sintetizado de un enum.
constexpr const char* kEnumRootFacadeTypeKey = "<enum-root-facade>";

struct NamedArgumentMap final
{
    bool valid = false;
    std::vector<std::size_t> explicitParameterOrdinals;
    std::vector<std::size_t> defaultedParameterOrdinals;
    std::vector<bool> parameterPackElements;
    std::optional<std::size_t> parameterPackOrdinal;
    bool usesExpandedParameterPack = false;
    bool synthesizesEmptyParameterPack = false;
};

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

    std::vector<bool> assigned(candidate.parameters.size(), false);
    for (std::size_t index = 0; index < reservedLeadingParameters; ++index) assigned[index] = true;

    std::size_t nextPositional = reservedLeadingParameters;
    std::size_t variadicOrdinal = 0;
    bool reachedVariadicArguments = false;
    bool reachedExpandedParameterPack = false;
    bool assignedParameterPackDirectly = false;
    for (const ast::AstNode* argument : arguments)
    {
        if (argument == nullptr) return result;
        std::optional<std::size_t> parameterOrdinal;
        bool parameterPackElement = false;
        if (!argument->argumentName().empty())
        {
            // The ellipsis has neither a name nor a parameter symbol. Once a positional
            // argument has crossed the fixed prefix, a later named argument would otherwise
            // make the physical C argument order ambiguous.
            if (reachedVariadicArguments || reachedExpandedParameterPack) return result;
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
            else if (candidate.isVariadic)
            {
                parameterOrdinal = candidate.parameters.size() + variadicOrdinal++;
                reachedVariadicArguments = true;
            }
        }
        if (!parameterOrdinal.has_value()) return result;
        if (*parameterOrdinal < assigned.size())
        {
            if (assigned[*parameterOrdinal] && !parameterPackElement) return result;
            if (!parameterPackElement) assigned[*parameterOrdinal] = true;
        }
        result.explicitParameterOrdinals.push_back(*parameterOrdinal);
        result.parameterPackElements.push_back(parameterPackElement);
        result.usesExpandedParameterPack =
            result.usesExpandedParameterPack || parameterPackElement;
        while (nextPositional < assigned.size() && assigned[nextPositional]) ++nextPositional;
    }

    for (std::size_t index = reservedLeadingParameters; index < candidate.parameters.size(); ++index)
    {
        if (index == result.parameterPackOrdinal)
        {
            result.synthesizesEmptyParameterPack =
                !result.usesExpandedParameterPack && !assignedParameterPackDirectly;
            continue;
        }
        if (assigned[index]) continue;
        if (candidate.parameters[index].defaultValueSyntax == nullptr) return result;
        result.defaultedParameterOrdinals.push_back(index);
    }
    result.valid = true;
    return result;
}

std::string typeCandidateList(
    std::span<const std::size_t> candidates,
    const ProjectNameLookup& lookup)
{
    std::string result;
    for (const std::size_t ordinal : candidates)
    {
        if (ordinal >= lookup.namedTypes().size()) continue;
        if (!result.empty()) result += ", ";
        result += lookup.namedTypes()[ordinal].identity.key();
    }
    return result;
}

bool isComparison(TokenKind kind) noexcept
{
    return kind == TokenKind::EqualEqual || kind == TokenKind::BangEqual ||
        kind == TokenKind::Less || kind == TokenKind::LessEqual ||
        kind == TokenKind::Greater || kind == TokenKind::GreaterEqual;
}

bool isLogical(TokenKind kind) noexcept
{
    return kind == TokenKind::AmpAmp || kind == TokenKind::PipePipe;
}

bool isArithmetic(TokenKind kind) noexcept
{
    return kind == TokenKind::Plus || kind == TokenKind::Minus || kind == TokenKind::Star ||
        kind == TokenKind::Slash || kind == TokenKind::Percent || kind == TokenKind::Amp ||
        kind == TokenKind::Pipe || kind == TokenKind::Caret || kind == TokenKind::LessLess ||
        kind == TokenKind::GreaterGreater;
}

bool isShift(TokenKind kind) noexcept
{
    return kind == TokenKind::LessLess || kind == TokenKind::GreaterGreater;
}

// The promoted type of a shift's LEFT operand, which is also the type of the whole shift.
// Anything narrower than `int` promotes to `int`; everything else keeps its own type. This is
// the C and C# rule, and it is deliberately NOT a function of the right operand -- see the shift
// branch in analyzeBinary for what happened when it was.
std::string shiftResultType(std::string_view leftTypeKey)
{
    // ID 89: `sbyte` promociona a `int` en un shift igual que los otros tres estrechos.
    if (leftTypeKey == "builtin:sbyte" || leftTypeKey == "builtin:byte" ||
        leftTypeKey == "builtin:char" || leftTypeKey == "builtin:short" ||
        leftTypeKey == "builtin:ushort")
    {
        return "builtin:int";
    }
    return std::string(leftTypeKey);
}

std::string stage38OperatorMethodName(TokenKind kind, std::size_t arity)
{
    switch (kind)
    {
        case TokenKind::Plus: return arity == 1 ? "op_UnaryPlus" : "op_Addition";
        case TokenKind::Minus: return arity == 1 ? "op_UnaryNegation" : "op_Subtraction";
        case TokenKind::Star: return "op_Multiply";
        case TokenKind::Slash: return "op_Division";
        case TokenKind::Percent: return "op_Modulus";
        case TokenKind::Bang: return "op_LogicalNot";
        case TokenKind::Tilde: return "op_OnesComplement";
        case TokenKind::Amp: return "op_BitwiseAnd";
        case TokenKind::Pipe: return "op_BitwiseOr";
        case TokenKind::Caret: return "op_ExclusiveOr";
        case TokenKind::LessLess: return "op_LeftShift";
        case TokenKind::GreaterGreater: return "op_RightShift";
        case TokenKind::EqualEqual: return "op_Equality";
        case TokenKind::BangEqual: return "op_Inequality";
        case TokenKind::Less: return "op_LessThan";
        case TokenKind::LessEqual: return "op_LessThanOrEqual";
        case TokenKind::Greater: return "op_GreaterThan";
        case TokenKind::GreaterEqual: return "op_GreaterThanOrEqual";
        default: return {};
    }
}

bool isErrorType(std::string_view key) noexcept
{
    return key.empty() || key == "<error>";
}

// La REGLA vive en include/nativesharp/Frontend/IntegerLiteralTyping.h, compartida con el
// evaluador de constantes; aqui solo queda sacarle el texto y el valor al nodo. `negated` es true
// cuando este literal es el operando directo de un menos unario -- ver analyzeUnary.
std::string integerLiteralType(const ast::AstNode* node, bool negated = false)
{
    return std::string(builtins::integerLiteralTypeKey(node->text(), node->integerValue(), negated));
}

bool isUnsignedIntegerLiteral(const ast::AstNode* node) noexcept
{
    for (const char value : node->text())
    {
        if (value == 'u' || value == 'U')
        {
            return true;
        }
    }
    return false;
}

enum class Stage37ConstantKind
{
    Integer,
    Floating,
    Boolean,
    String
};

struct Stage37ConstantValue final
{
    Stage37ConstantValue() = default;
    Stage37ConstantValue(
        Stage37ConstantKind valueKind,
        std::string valueTypeKey,
        std::uint64_t integerValue,
        double floatingValue,
        bool booleanValue,
        std::string stringValue = {})
        : kind(valueKind),
          typeKey(std::move(valueTypeKey)),
          integer(integerValue),
          floating(floatingValue),
          boolean(booleanValue),
          string(std::move(stringValue))
    {
    }

    Stage37ConstantKind kind = Stage37ConstantKind::Integer;
    std::string typeKey;
    std::uint64_t integer = 0;
    double floating = 0.0;
    bool boolean = false;
    std::string string;
};

bool isSignedIntegralType(std::string_view typeKey) noexcept
{
    // ID 89: `sbyte` entra aqui y `byte` no. Es toda la diferencia entre los dos tipos.
    return typeKey == "builtin:sbyte" || typeKey == "builtin:short" || typeKey == "builtin:int" ||
        typeKey == "builtin:long" || typeKey == "builtin:nint";
}

unsigned integralBitWidth(std::string_view typeKey) noexcept
{
    if (typeKey == "builtin:sbyte" || typeKey == "builtin:byte" ||
        typeKey == "builtin:char") return 8;
    if (typeKey == "builtin:short" || typeKey == "builtin:ushort") return 16;
    if (typeKey == "builtin:int" || typeKey == "builtin:uint") return 32;
    return 64;
}

std::uint64_t integralMask(unsigned width) noexcept
{
    return width >= 64 ? std::numeric_limits<std::uint64_t>::max()
                       : ((std::uint64_t{1} << width) - 1U);
}

std::uint64_t normalizeIntegral(std::uint64_t value, std::string_view typeKey) noexcept
{
    return value & integralMask(integralBitWidth(typeKey));
}

std::int64_t signedIntegral(std::uint64_t bits, std::string_view typeKey) noexcept
{
    const unsigned width = integralBitWidth(typeKey);
    bits &= integralMask(width);
    if (width >= 64) return static_cast<std::int64_t>(bits);
    const std::uint64_t sign = std::uint64_t{1} << (width - 1U);
    if ((bits & sign) == 0) return static_cast<std::int64_t>(bits);
    return static_cast<std::int64_t>(bits | ~integralMask(width));
}

double normalizeFloatingConstant(double value, std::string_view typeKey) noexcept
{
    return typeKey == "builtin:float"
        ? static_cast<double>(static_cast<float>(value))
        : value;
}
}

bool ProjectMethodBodyModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectGenericModel* genericModel,
    diagnostics::DiagnosticEngine& diagnostics,
    const ProjectGenericConstraintModel* genericConstraintModel,
    std::optional<std::size_t> publicSurfaceProjectOrdinal)
{
    genericConstraintModel_ = genericConstraintModel;
    genericModel_ = genericModel;
    if (built_)
    {
        return !diagnostics.hasErrors();
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !inheritanceModel.isBuilt() ||
        !interfaceModel.isBuilt())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            SourceRange(),
            "project method-body binding requires completed name, member and inheritance models");
        built_ = true;
        return false;
    }

    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    namespaceFunctionModel_ = &namespaceFunctionModel;
    namespaceCallableBase_ = memberModel.members().size();
    namespaceCallableAdapters_.clear();
    namespaceCallableAdapters_.reserve(namespaceFunctionModel.functions().size());
    for (std::size_t functionOrdinal = 0;
         functionOrdinal < namespaceFunctionModel.functions().size(); ++functionOrdinal)
    {
        const NamespaceFunctionSymbol& function = namespaceFunctionModel.functions()[functionOrdinal];
        ClassMemberSymbol adapter;
        adapter.kind = ClassMemberKind::Method;
        adapter.accessibility = function.accessibility == NamespaceFunctionAccessibility::Public
            ? ClassMemberAccessibility::Public : ClassMemberAccessibility::Internal;
        adapter.declaringTypeOrdinal = nameLookup.namedTypes().size();
        adapter.sourceOrdinal = function.sourceOrdinal;
        adapter.declarationOrdinal = functionOrdinal;
        adapter.fileId = function.fileId;
        adapter.range = function.range;
        adapter.syntax = function.definitionSyntax;
        for (const NamespaceFunctionPart& part : function.parts)
        {
            if (!part.hasBody) continue;
            adapter.sourceOrdinal = part.sourceOrdinal;
            adapter.fileId = part.fileId;
            adapter.range = part.range;
            break;
        }
        adapter.name = function.name;
        adapter.declaredTypeSyntax = function.declaredTypeSyntax;
        adapter.declaredTypeKey = function.declaredTypeKey;
        adapter.declaredOwnership = function.declaredOwnership;
        adapter.parameters = function.parameters;
        adapter.signatureKey = function.signatureKey;
        adapter.hasImplicitThis = false;
        adapter.isStatic = true;
        adapter.isReferenceReturn = function.isReferenceReturn;
        adapter.isVariadic = function.isVariadic;
        namespaceCallableAdapters_.push_back(std::move(adapter));
    }
    inheritanceModel_ = &inheritanceModel;
    interfaceModel_ = &interfaceModel;

    // NSC-LIMIT-018. A closed generic type materializes all of its member SIGNATURES so normal
    // lookup and overload resolution can see the complete API. Its ordinary method BODIES remain
    // deferred for executables until a real call selects them. A library has external callers,
    // however, so its public/protected closed surface is seeded explicitly below. Constructors/
    // destructors are lifetime machinery and virtual methods can be referenced by a vtable
    // without a syntactic call, so those remain eager for every product.
    const auto isDeferredGenericMethod = [&](std::size_t ordinal) -> bool
    {
        if (ordinal >= memberModel.members().size()) return false;
        const ClassMemberSymbol& candidate = memberModel.members()[ordinal];
        if (candidate.kind != ClassMemberKind::Method || candidate.isVirtual || candidate.isOverride ||
            candidate.isAbstract)
        {
            return false;
        }
        constexpr std::string_view prefix = "stage44-specialization:";
        if (candidate.syntax != nullptr && candidate.syntax->auxiliaryText().starts_with(prefix))
        {
            return true;
        }
        return candidate.declaringTypeOrdinal < nameLookup.namedTypes().size() &&
            nameLookup.namedTypes()[candidate.declaringTypeOrdinal].syntax != nullptr &&
            nameLookup.namedTypes()[candidate.declaringTypeOrdinal].syntax->auxiliaryText().starts_with(prefix);
    };

    std::unordered_set<std::size_t> demandedGenericMethods;
    // A library publishes the complete callable surface of every public closed generic type it
    // owns. Demand those bodies even when the producer never calls them itself: a consumer can.
    // Executables pass no public-surface ordinal and retain the ordinary reachability-only policy.
    // Private/internal helpers are discovered transitively from these roots by the call/delegate
    // closure below, exactly as they are for roots demanded by source code.
    const auto declaringTypeIsPubliclyReachable = [&](std::size_t typeOrdinal) {
        if (!publicSurfaceProjectOrdinal.has_value()) return false;
        while (typeOrdinal < nameLookup.namedTypes().size())
        {
            const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
            if (type.projectOrdinal != *publicSurfaceProjectOrdinal ||
                type.accessibility != NamedTypeAccessibility::Public)
            {
                return false;
            }
            if (!type.containingTypeOrdinal.has_value()) return true;
            typeOrdinal = *type.containingTypeOrdinal;
        }
        return false;
    };
    if (publicSurfaceProjectOrdinal.has_value())
    {
        for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
        {
            const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
            const bool callableAccessibility =
                member.accessibility == ClassMemberAccessibility::Public ||
                member.accessibility == ClassMemberAccessibility::Protected;
            if (callableAccessibility && isDeferredGenericMethod(memberOrdinal) &&
                declaringTypeIsPubliclyReachable(member.declaringTypeOrdinal))
            {
                demandedGenericMethods.insert(memberOrdinal);
            }
        }
    }
    // A concrete interface table is an executable reference just like a direct call or delegate
    // conversion. A method cloned from `Box<T>` is otherwise deferred, and an invocation through
    // `IValue` selects the interface contract member rather than the concrete `Box<int>.Get`
    // member. Waiting for the call/delegate closure below therefore leaves the implementation
    // without bindings: Stage 36 can see the semantic slot but HIR has no function to put in it.
    // Seed every concrete implementation selected by ProjectInterfaceModel before analyzing the
    // transitive call graph. Constraint-invalid specializations remain protected by the normal
    // specializationSatisfiesConstraints guard inside the loop.
    for (const ClassInterfaceTablePlan& plan : interfaceModel.tablePlans())
    {
        for (const std::optional<std::size_t> implementation : plan.implementationMemberOrdinals)
        {
            if (implementation.has_value() && isDeferredGenericMethod(*implementation))
            {
                demandedGenericMethods.insert(*implementation);
            }
        }
    }
    std::vector<std::size_t> callableWorklist;
    callableWorklist.reserve(memberModel.members().size());
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
        if (member.isPrecompiled ||
            ((member.kind != ClassMemberKind::Method && member.kind != ClassMemberKind::Constructor &&
              member.kind != ClassMemberKind::Destructor) ||
             member.syntax == nullptr || member.syntax->first() == nullptr) ||
            (isDeferredGenericMethod(memberOrdinal) &&
             !demandedGenericMethods.contains(memberOrdinal)))
        {
            continue;
        }
        callableWorklist.push_back(memberOrdinal);
    }

    for (std::size_t functionOrdinal = 0;
         functionOrdinal < namespaceCallableAdapters_.size(); ++functionOrdinal)
    {
        const ClassMemberSymbol& callable = namespaceCallableAdapters_[functionOrdinal];
        if (callable.syntax != nullptr && callable.syntax->first() != nullptr)
            callableWorklist.push_back(namespaceCallableBase_ + functionOrdinal);
    }

    std::size_t workCursor = 0;
    std::size_t callDemandCursor = 0;
    std::size_t delegateDemandCursor = 0;
    std::size_t optionalDemandCursor = 0;
    while (workCursor < callableWorklist.size())
    {
        const std::size_t waveEnd = callableWorklist.size();
        for (; workCursor < waveEnd; ++workCursor)
        {
            const std::size_t memberOrdinal = callableWorklist[workCursor];
            if (analyzedCallableMembers_.contains(memberOrdinal)) continue;
            const ClassMemberSymbol* memberPointer = methodSymbol(memberOrdinal);
            if (memberPointer == nullptr) continue;
            const ClassMemberSymbol& member = *memberPointer;
        // NSC-LIMIT-007. UNA ESPECIALIZACION QUE YA INCUMPLIO SU `where` NO SE ANALIZA.
        //
        // Su cuerpo se sustituyo con un `T` que la clausula prohibe, de modo que analizarlo
        // produce errores sobre codigo que el programa no usa: `F<int>(1)` con una sobrecarga
        // `F<T>(T, int) where T : IComponente` fallaba dentro de un `x.Peso()` que nadie llama.
        //
        // No se BORRA el registro: la fase de constraints lo necesita para poder explicar, si al
        // final ninguna candidata sirve, POR QUE ninguna servia. Lo que no se hace es tratarlo
        // como codigo del programa. HIR lo salta por la misma pregunta y en el mismo sentido.
        //
        // Se preguntan DOS nodos, y hacen falta los dos. Para un METODO generico la invalidez
        // esta en su propio nodo; para un TIPO generico --`Caja<int>` con `where T : class`--
        // esta en el nodo del tipo, y sus metodos son hijos suyos, asi que preguntar solo por
        // `member.syntax` no la veria nunca.
            if (memberOrdinal < namespaceCallableBase_ && genericConstraintModel_ != nullptr &&
                (!genericConstraintModel_->specializationSatisfiesConstraints(member.syntax) ||
                 (member.declaringTypeOrdinal < nameLookup.namedTypes().size() &&
                  !genericConstraintModel_->specializationSatisfiesConstraints(
                      nameLookup.namedTypes()[member.declaringTypeOrdinal].syntax))))
            {
                continue;
            }

            analyzedCallableMembers_.insert(memberOrdinal);

        const MethodScopeKind rootKind = member.kind == ClassMemberKind::Constructor
            ? MethodScopeKind::Constructor
            : member.kind == ClassMemberKind::Destructor
                ? MethodScopeKind::Destructor
                : MethodScopeKind::Method;
        movedRaiiLocals_.clear();
        maybeMovedRaiiLocals_.clear();
        movedOwnedParameters_.clear();
        maybeMovedOwnedParameters_.clear();
        invalidLocalBorrows_.clear();
        maybeInvalidLocalBorrows_.clear();
        assignedLocals_.clear();
        definiteAssignmentWriteTarget_ = nullptr;
        ownershipFlowReachable_ = true;
        throwFlowStates_.clear();
        const std::size_t rootScopeOrdinal = createScope(
            rootKind,
            memberOrdinal,
            std::nullopt);
        seedParameters(memberOrdinal, rootScopeOrdinal, diagnostics);

        // Stage 50.8: an `owned` parameter is an owner whose lifetime belongs to the callee.
        // Keep its cleanup active for the entire callable so returns and exceptional exits
        // destroy it exactly once. A later move nulls the parameter storage in LLVM, making
        // this cleanup naturally idempotent.
        std::vector<std::size_t> ownedParameterCleanupOrdinals;
        for (std::size_t parameterOrdinal = 0; parameterOrdinal < member.parameters.size(); ++parameterOrdinal)
        {
            if (member.parameters[parameterOrdinal].ownership != sema::OwnershipKind::Owned)
            {
                continue;
            }
            const std::size_t cleanupOrdinal = createCleanupRegion(
                MethodCleanupRegionKind::OwnedParameterDestroy,
                member.syntax,
                member.syntax,
                memberOrdinal,
                rootScopeOrdinal);
            cleanupRegions_[cleanupOrdinal].resourceParameterOrdinal = parameterOrdinal;
            activeCleanupRegions_.push_back(cleanupOrdinal);
            ownedParameterCleanupOrdinals.push_back(cleanupOrdinal);
        }

        if (member.kind == ClassMemberKind::Constructor && member.syntax->second() != nullptr &&
            (member.syntax->second()->kind() == ast::AstNodeKind::BaseConstructorInitializer ||
             member.syntax->second()->kind() == ast::AstNodeKind::ThisConstructorInitializer))
        {
            // NSC-LIMIT-010: `this(...)` y `base(...)` son llamadas a constructor, asi que sus
            // argumentos se analizan con `allowMove` como los de un `new` y los de una llamada
            // ordinaria. Sin esto, delegar la propiedad --`: this(move(entrante))`, que es la
            // forma en cadena natural-- se rechazaba aunque el constructor destino declarara el
            // parametro `owned`.
            for (const ast::AstNode* argument : member.syntax->second()->children())
            {
                analyzeExpression(
                    argument, memberOrdinal, rootScopeOrdinal, diagnostics,
                    /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
            }
        }
        analyzeBlock(
            member.syntax->first(),
            memberOrdinal,
            rootScopeOrdinal,
            false,
            diagnostics);

        for (std::size_t index = ownedParameterCleanupOrdinals.size(); index > 0; --index)
        {
            recordNormalCleanupContinuation(
                ownedParameterCleanupOrdinals[index - 1],
                member.syntax->first(),
                memberOrdinal,
                rootScopeOrdinal);
        }
            activeCleanupRegions_.clear();
        }

        std::vector<std::size_t> newlyDemanded;
        for (; callDemandCursor < calls_.size(); ++callDemandCursor)
        {
            const MethodCallBinding& call = calls_[callDemandCursor];
            if (call.resolution != MethodCallResolutionKind::Resolved ||
                !call.selectedMemberOrdinal.has_value() ||
                !isDeferredGenericMethod(*call.selectedMemberOrdinal))
            {
                continue;
            }
            if (demandedGenericMethods.insert(*call.selectedMemberOrdinal).second)
                newlyDemanded.push_back(*call.selectedMemberOrdinal);
        }
        for (; delegateDemandCursor < delegateBindings_.size(); ++delegateDemandCursor)
        {
            const MethodDelegateBinding& binding = delegateBindings_[delegateDemandCursor];
            if (binding.kind != MethodDelegateOperationKind::Create ||
                !binding.selectedMemberOrdinal.has_value() ||
                !isDeferredGenericMethod(*binding.selectedMemberOrdinal))
            {
                continue;
            }
            if (demandedGenericMethods.insert(*binding.selectedMemberOrdinal).second)
                newlyDemanded.push_back(*binding.selectedMemberOrdinal);
        }
        for (; optionalDemandCursor < optionalCarrierBindings_.size(); ++optionalDemandCursor)
        {
            const MethodOptionalCarrierBinding& binding =
                optionalCarrierBindings_[optionalDemandCursor];
            for (const std::size_t target :
                 {binding.hasValueMemberOrdinal, binding.uncheckedValueMemberOrdinal})
            {
                if (isDeferredGenericMethod(target) &&
                    demandedGenericMethods.insert(target).second)
                {
                    newlyDemanded.push_back(target);
                }
            }
        }
        std::ranges::sort(newlyDemanded);
        newlyDemanded.erase(std::unique(newlyDemanded.begin(), newlyDemanded.end()), newlyDemanded.end());
        callableWorklist.insert(
            callableWorklist.end(), newlyDemanded.begin(), newlyDemanded.end());
    }

    // Stage 37: field initializers are semantic executable contexts too.  They deliberately have
    // no implicit-this context: an initializer may use literals, locals do not exist, and static
    // members may be referenced, but partially-constructed instance state is not addressable.
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& field = memberModel.members()[memberOrdinal];
        if (field.kind != ClassMemberKind::Field || field.syntax == nullptr || field.syntax->first() == nullptr)
        {
            continue;
        }
        // Stage 51.x: an enum constant's value was already validated and computed by
        // ProjectMemberModel::collectEnumMembers (plain integer literal or auto-increment,
        // range-checked against NativeSharp int) -- this generic field-initializer typechecking
        // loop does not apply: its declared type is deliberately the enum's own type, not an
        // integral/bool/float/double builtin, so the checks below would reject it by construction.
        if (field.isEnumConstant)
        {
            continue;
        }

        const std::size_t rootScopeOrdinal = createScope(
            MethodScopeKind::FieldInitializer,
            memberOrdinal,
            std::nullopt);
        ExpressionResult initializer;
        if (field.syntax->first()->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            // C7 "aggregate initializer": a field is filled against its declared type exactly the
            // way a local is. Same binder, so `V field = {1, 2};` and `V local = {1, 2};` cannot
            // disagree about arity, element types or partial fills.
            initializer = bindAggregateInitializer(
                field.syntax->first(), field.declaredTypeKey, memberOrdinal, rootScopeOrdinal, diagnostics);
        }
        else if (field.syntax->first()->kind() == ast::AstNodeKind::LambdaExpression &&
            isDelegateTypeKey(field.declaredTypeKey))
        {
            initializer = bindLambdaToDelegate(
                field.syntax->first(), field.declaredTypeKey, memberOrdinal, rootScopeOrdinal, diagnostics);
        }
        else
        {
            initializer = analyzeExpression(
                field.syntax->first(), memberOrdinal, rootScopeOrdinal, diagnostics, isDelegateTypeKey(field.declaredTypeKey));
            if (initializer.methodGroup && isDelegateTypeKey(field.declaredTypeKey))
            {
                initializer = bindMethodGroupToDelegate(
                    field.syntax->first(), initializer, field.declaredTypeKey, memberOrdinal, rootScopeOrdinal, diagnostics);
            }
        }
        if (!initializer.valid || initializer.methodGroup)
        {
            continue;
        }

        const ProjectImplicitConversion conversion = classifyImplicit(
            initializer.typeKey,
            field.declaredTypeKey,
            initializer.integerConstant);
        if (!conversion.exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                field.syntax->first()->range(),
                "cannot implicitly convert field initializer from '" + initializer.typeKey +
                    "' to '" + field.declaredTypeKey + "'");
        }
        if (field.isConst &&
            !ProjectTypeConversion::isIntegral(field.declaredTypeKey) &&
            field.declaredTypeKey != "builtin:bool" &&
            field.declaredTypeKey != "builtin:float" &&
            field.declaredTypeKey != "builtin:double" &&
            field.declaredTypeKey != "builtin:string")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                field.syntax->range(),
                "const fields are limited to integral, bool, float, double, and string types");
        }
    }

    // NSC-LIMIT-019. Validate after every initializer has been analyzed. ProjectMemberModel orders
    // types by stable identity rather than source order, so a transitive evaluator needs all name
    // and expression bindings to exist before it follows qualified dependencies.
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& field = memberModel.members()[memberOrdinal];
        if (field.kind != ClassMemberKind::Field || !field.isConst || field.isEnumConstant ||
            field.syntax == nullptr || field.syntax->first() == nullptr)
        {
            continue;
        }
        if (!isCompileTimeConstantExpression(field.syntax->first(), memberOrdinal))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                field.syntax->first()->range(),
                "const field initializer must be a compile-time constant expression");
        }
    }

    verifyOutParameterAssignments(diagnostics);

    // A required inlining contract cannot be honored by an all-required recursive cycle.  LLVM
    // deliberately refuses to expand recursive alwaysinline call sites, so diagnose the source
    // contract here instead of silently leaving a call behind.  Edges through an automatic or
    // preferred method are not rejected: after expanding the required callee, that ordinary call
    // remains a valid recursion boundary.
    std::vector<std::vector<std::size_t>> requiredCallGraph(memberModel.members().size());
    for (const MethodCallBinding& call : calls_)
    {
        if (call.resolution != MethodCallResolutionKind::Resolved ||
            !call.selectedMemberOrdinal.has_value() ||
            call.callerMethodMemberOrdinal >= memberModel.members().size() ||
            *call.selectedMemberOrdinal >= memberModel.members().size())
        {
            continue;
        }
        const ClassMemberSymbol& caller = memberModel.members()[call.callerMethodMemberOrdinal];
        const ClassMemberSymbol& callee = memberModel.members()[*call.selectedMemberOrdinal];
        if (caller.inliningPolicy == InliningPolicy::Required &&
            callee.inliningPolicy == InliningPolicy::Required)
        {
            requiredCallGraph[call.callerMethodMemberOrdinal].push_back(*call.selectedMemberOrdinal);
        }
    }
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
        if (member.inliningPolicy != InliningPolicy::Required || member.syntax == nullptr)
        {
            continue;
        }
        std::vector<std::size_t> pending(requiredCallGraph[memberOrdinal].begin(),
                                         requiredCallGraph[memberOrdinal].end());
        std::vector<bool> visited(memberModel.members().size(), false);
        bool recursive = false;
        while (!pending.empty() && !recursive)
        {
            const std::size_t candidate = pending.back();
            pending.pop_back();
            if (candidate == memberOrdinal)
            {
                recursive = true;
                break;
            }
            if (candidate >= visited.size() || visited[candidate]) continue;
            visited[candidate] = true;
            pending.insert(pending.end(), requiredCallGraph[candidate].begin(),
                           requiredCallGraph[candidate].end());
        }
        if (recursive)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                member.syntax->range(),
                "forceinline method '" + member.name +
                    "' participates in a recursive forceinline call cycle and cannot be "
                    "expanded at every call site");
        }
    }

    built_ = true;
    return !diagnostics.hasErrors();
}

// C4 "language references": an `out` parameter is an OBLIGATION, not a permission -- the caller
// is allowed to pass an uninitialized variable precisely because the callee promises to fill it.
// This is the check that makes the promise mean something.
//
// PRECISION about what this does and does not prove, because the difference matters: it verifies
// that the body assigns the parameter AT LEAST ONCE somewhere. It does NOT prove that every
// return path assigns it -- that is path-sensitive definite assignment, which needs the CFG and
// is a piece of work of its own (this compiler has a CFG, but the body model runs before it).
// So `out int x` with `if (c) { x = 1; }` and no else is accepted here and would be rejected by
// a full analysis. What the check DOES catch is the mistake people actually make: declaring an
// out parameter and never writing it at all, which would hand the caller whatever bytes the
// stack slot happened to hold. Tightening this to full path coverage is a descendant item, and
// it is named as such in the round's documentation rather than left implied.
void ProjectMethodBodyModel::verifyOutParameterAssignments(
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr) return;
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel_->members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Method || member.isAbstract) continue;
        if (member.syntax == nullptr || member.syntax->first() == nullptr) continue;
        if (!analyzedCallableMembers_.contains(memberOrdinal)) continue;
        for (std::size_t index = 0; index < member.parameters.size(); ++index)
        {
            if (member.parameters[index].passKind != ParameterPassKind::Out) continue;
            if (assignedOutParameters_.contains({memberOrdinal, index})) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                member.parameters[index].range,
                "out parameter '" + member.parameters[index].name +
                    "' is never assigned in the body of '" + member.name + "'");
        }
    }
}

// ---------------------------------------------------------------------------------------------
// C7 "definite assignment"
//
// The hole this closes: before it, `V v;` followed by a read of `v` compiled to an `alloca` with
// no store, and the read returned whatever was on the stack. No diagnostic, no zeroing, no
// warning -- for EVERY local type, not just aggregates. The fix has two halves that must be read
// together, because each one alone would be the wrong language:
//
//  1. The BACKEND gives every local without an initializer defined storage: it zeroes the slot at
//     the declaration point (see ProjectOopLlvmLowerer's LocalDeclaration case). NativeSharp
//     therefore has no uninitialized-read undefined behaviour left anywhere, at any optimization
//     level. In a program this analysis accepts every zeroing store is dead and SROA deletes it,
//     so the guarantee is free in exactly the programs that do not need it.
//
//  2. This analysis reports, as a hard error, the reads where the zero is CERTAINLY not what the
//     programmer meant -- a scalar, a pointer, a reference or a delegate read on a path where
//     nothing ever wrote it. That is the C# rule and it catches real bugs.
//
// The one deliberate difference from C#: an aggregate local (a struct value or a fixed `T[N]`
// array) is never reported. That is not a weakening, it is the language Alberto asked for --
// "struct sin metodos se comporta como en C". `Color c; c.tag = 1;` is a legitimate C idiom, and
// piecewise filling of an aggregate through loops and branches is precisely the shape a
// path-sensitive MUST-analysis cannot follow without rejecting correct programs. Half 1 is what
// makes that safe: the unwritten fields read as zero, deterministically, rather than as garbage.
bool ProjectMethodBodyModel::typeKeyHasDefinedDefaultStorage(std::string_view typeKey) const noexcept
{
    // A fixed-size array `T[N]` is storage, not a reference: `int[4] values;` reserves sixteen
    // bytes exactly like C does, and the backend zeroes them. A DYNAMIC array `T[]` is a heap
    // object behind a reference and is tracked like any other reference -- which is precisely
    // what makes `int[] a; a[0] = 1;` reportable.
    if (ProjectMemberModel::fixedArrayElementTypeKey(typeKey).has_value())
    {
        return true;
    }
    if (ProjectTypeConversion::isTuple(typeKey)) return true;
    if (!typeKey.starts_with("type:") || nameLookup_ == nullptr)
    {
        return false;
    }
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() == identity)
        {
            return type.kind == NamedTypeDeclarationKind::Struct;
        }
    }
    return false;
}

// NSC-LIMIT-004: la clave nombra un struct y nada mas. Misma busqueda que aggregateIsUnion,
// justo debajo, pero mirando la CLASE de la declaracion en vez de su texto auxiliar.
bool ProjectMethodBodyModel::typeKeyIsStruct(std::string_view typeKey) const noexcept
{
    if (ProjectTypeConversion::isTuple(typeKey)) return true;
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != identity) continue;
        return type.kind == NamedTypeDeclarationKind::Struct;
    }
    return false;
}

// C8: does this type key name a union? Reads the same layout marker the parser wrote onto the
// declaration and ProjectObjectLayoutModel reads back (see parseStructLayoutOptions there).
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

// SHR-013: una vista, o un arreglo de vistas de cualquier rango. `StringView[]` es lo que un
// lexer quiere de verdad -- una lista de lexemas sin copiar ni uno -- y precisamente por eso
// tiene que contar como vista alla donde una vista no puede ir.
bool ProjectMethodBodyModel::typeKeyIsViewLike(std::string_view typeKey) const noexcept
{
    std::string_view key = typeKey;
    while (key.ends_with("[]")) key = key.substr(0, key.size() - 2);
    return aggregateIsBorrowedView(key);
}

bool ProjectMethodBodyModel::typeKeyCarriesLifetime(std::string_view typeKey) const noexcept
{
    if (typeKey.empty() || typeKey == "<error>") return false;
    if (typeKey.ends_with("*")) return true;
    if (typeKey.ends_with("[]")) return true;
    if (typeKey == "builtin:string") return true;
    if (typeKeyIsViewLike(typeKey)) return true;
    return isReferenceTypeKey(typeKey);
}

// SHR-013 "vistas y tiempo de vida" -- EL ANALISIS DE ENRAIZAMIENTO.
//
// La pregunta: esta expresion toma prestado de un local de ESTE metodo? Si la respuesta es que
// si, el valor no puede salir del metodo, porque ese local muere al volver.
//
// Como contesta: recorre el arbol de la expresion y se queda con el PRIMER identificador que
// nombre un local cuyo tipo lleve un tiempo de vida -- una cadena, un arreglo, un puntero, un
// duenno `raii`, u otra vista ya enraizada en un local. Un `int` no cuenta: un indice no es un
// prestamo. Un parametro, un campo y un literal tampoco cuentan, y ese es justo el punto: los
// tres sobreviven a la llamada, asi que una vista sacada de ellos puede salir.
//
// POR QUE UNA VISTA LOCAL SE CONSULTA Y NO SE ASUME. `StringView v = StringView.Of(texto);
// return v;` con `texto` de parametro es CORRECTO, y una regla que dijera "un local de tipo
// vista contamina" lo rechazaria. Por eso cada local de tipo vista lleva grabado de que se
// enraiza (`viewRootLocalName`), y aqui se le pregunta en vez de suponerlo.
//
// LO QUE NO HACE, dicho para que nadie lo lea como mas de lo que es: no sigue el puntero. Una
// vista que pasa por una llamada cuyo argumento es un local se considera enraizada en ese local
// aunque la llamada no lo use. Es conservador a proposito: de los dos errores posibles, rechazar
// un programa correcto se ve al compilar y se arregla; aceptar uno incorrecto es un
// uso-despues-de-liberar que no se ve nunca.
std::optional<std::string> ProjectMethodBodyModel::viewRootLocalOfExpression(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    if (expression == nullptr) return std::nullopt;

    if (expression->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, expression->text());
        if (name.has_value() && name->kind == ScopeNameKind::Local &&
            name->semanticOrdinal < locals_.size())
        {
            const MethodLocalSymbol& local = locals_[name->semanticOrdinal];
            if (typeKeyIsViewLike(local.typeKey))
            {
                if (!local.viewRootLocalName.empty()) return local.viewRootLocalName;
            }
            else if (typeKeyCarriesLifetime(local.typeKey))
            {
                return local.name;
            }
        }
        return std::nullopt;
    }

    const auto probe = [&](const ast::AstNode* child) -> std::optional<std::string>
    {
        return viewRootLocalOfExpression(child, scopeOrdinal);
    };
    if (auto found = probe(expression->first()); found.has_value()) return found;
    if (auto found = probe(expression->second()); found.has_value()) return found;
    if (auto found = probe(expression->third()); found.has_value()) return found;
    for (const ast::AstNode* child : expression->children())
    {
        if (auto found = probe(child); found.has_value()) return found;
    }
    return std::nullopt;
}

std::optional<std::size_t> ProjectMethodBodyModel::variableArrayRootLocalOfExpression(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    if (expression == nullptr) return std::nullopt;
    if (expression->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, expression->text());
        if (name.has_value() && name->kind == ScopeNameKind::Local &&
            name->semanticOrdinal < locals_.size())
        {
            return locals_[name->semanticOrdinal].variableLengthArrayRootLocalOrdinal;
        }
        return std::nullopt;
    }
    const auto probe = [&](const ast::AstNode* child) -> std::optional<std::size_t>
    {
        return variableArrayRootLocalOfExpression(child, scopeOrdinal);
    };
    if (auto root = probe(expression->first()); root.has_value()) return root;
    if (auto root = probe(expression->second()); root.has_value()) return root;
    if (auto root = probe(expression->third()); root.has_value()) return root;
    for (const ast::AstNode* child : expression->children())
    {
        if (auto root = probe(child); root.has_value()) return root;
    }
    return std::nullopt;
}

MethodEscapeOrigin ProjectMethodBodyModel::referenceEscapeOriginOfExpression(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    if (expression == nullptr) return {};

    const auto typeKeyOf = [&](const ast::AstNode* syntax) -> std::string_view
    {
        for (auto iterator = expressionTypes_.rbegin(); iterator != expressionTypes_.rend(); ++iterator)
            if (iterator->syntax == syntax) return iterator->typeKey;
        return {};
    };
    const auto resolvedCall = [&](const ast::AstNode* syntax) -> const MethodCallBinding*
    {
        for (auto iterator = calls_.rbegin(); iterator != calls_.rend(); ++iterator)
            if (iterator->syntax == syntax &&
                iterator->resolution == MethodCallResolutionKind::Resolved &&
                iterator->selectedMemberOrdinal.has_value()) return &*iterator;
        return nullptr;
    };

    if (expression->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, expression->text());
        if (!name.has_value()) return {};
        if (name->kind == ScopeNameKind::Local && name->semanticOrdinal < locals_.size())
        {
            const MethodLocalSymbol& local = locals_[name->semanticOrdinal];
            if (local.variableLengthArrayRootLocalOrdinal.has_value())
            {
                const std::size_t root = *local.variableLengthArrayRootLocalOrdinal;
                return {MethodEscapeOriginKind::VariableLengthArray,
                    local.variableLengthArrayRootLocalOrdinal,
                    root < locals_.size()
                        ? std::optional<std::size_t>(locals_[root].scopeOrdinal)
                        : std::nullopt};
            }
            if (local.isReference) return local.referenceEscapeOrigin;
            return {MethodEscapeOriginKind::CurrentFrameLocal, name->semanticOrdinal,
                local.scopeOrdinal};
        }
        if (name->kind == ScopeNameKind::Parameter)
        {
            const std::size_t methodOrdinal = scopeOrdinal < scopes_.size()
                ? scopes_[scopeOrdinal].methodMemberOrdinal : 0U;
            const ClassMemberSymbol* method = methodSymbol(methodOrdinal);
            if (method != nullptr && name->semanticOrdinal < method->parameters.size())
            {
                const ParameterPassKind passKind = method->parameters[name->semanticOrdinal].passKind;
                if (passKind == ParameterPassKind::Ref || passKind == ParameterPassKind::Out)
                {
                    std::size_t rootScope = scopeOrdinal;
                    while (rootScope < scopes_.size() &&
                        scopes_[rootScope].parentScopeOrdinal.has_value())
                        rootScope = *scopes_[rootScope].parentScopeOrdinal;
                    return {MethodEscapeOriginKind::ExternalStorage, name->semanticOrdinal, rootScope};
                }
                std::size_t rootScope = scopeOrdinal;
                while (rootScope < scopes_.size() &&
                    scopes_[rootScope].parentScopeOrdinal.has_value())
                    rootScope = *scopes_[rootScope].parentScopeOrdinal;
                return {MethodEscapeOriginKind::CurrentFrameParameter, name->semanticOrdinal, rootScope};
            }
        }
        return {};
    }

    if (expression->kind() == ast::AstNodeKind::MemberExpression)
    {
        const ast::AstNode* receiver = expression->first();
        if (typeKeyIsStruct(typeKeyOf(receiver)))
            return referenceEscapeOriginOfExpression(receiver, scopeOrdinal);
        return {};
    }

    if (expression->kind() == ast::AstNodeKind::IndexExpression)
    {
        if (const MethodCallBinding* call = resolvedCall(expression);
            call != nullptr && memberModel_ != nullptr &&
            *call->selectedMemberOrdinal < memberModel_->members().size() &&
            memberModel_->members()[*call->selectedMemberOrdinal].isReferenceReturn)
        {
            if (typeKeyIsStruct(call->receiverTypeKey))
                return referenceEscapeOriginOfExpression(call->receiverSyntax, scopeOrdinal);
            return {};
        }
        if (const std::optional<std::size_t> vla =
                variableArrayRootLocalOfExpression(expression->first(), scopeOrdinal);
            vla.has_value())
            return {MethodEscapeOriginKind::VariableLengthArray, vla,
                *vla < locals_.size()
                    ? std::optional<std::size_t>(locals_[*vla].scopeOrdinal)
                    : std::nullopt};
        const std::string_view baseType = typeKeyOf(expression->first());
        if (ProjectMemberModel::fixedArrayElementTypeKey(baseType).has_value() ||
            ProjectMemberModel::flexibleArrayElementTypeKey(baseType).has_value())
            return referenceEscapeOriginOfExpression(expression->first(), scopeOrdinal);
        return {};
    }

    if (expression->kind() == ast::AstNodeKind::CallExpression)
    {
        const MethodCallBinding* call = resolvedCall(expression);
        if (call == nullptr || memberModel_ == nullptr ||
            *call->selectedMemberOrdinal >= memberModel_->members().size()) return {};
        const ClassMemberSymbol& callee = memberModel_->members()[*call->selectedMemberOrdinal];
        if (!callee.isReferenceReturn) return {};
        if (!call->staticCall && typeKeyIsStruct(call->receiverTypeKey))
        {
            const MethodEscapeOrigin receiver =
                referenceEscapeOriginOfExpression(call->receiverSyntax, scopeOrdinal);
            if (receiver.kind != MethodEscapeOriginKind::ExternalStorage) return receiver;
        }
        for (std::size_t index = 0; index < expression->children().size(); ++index)
        {
            const std::size_t parameterIndex = index < call->selectedArgumentParameterOrdinals.size()
                ? call->selectedArgumentParameterOrdinals[index] : index;
            if (parameterIndex >= callee.parameters.size()) continue;
            const ParameterPassKind passKind = callee.parameters[parameterIndex].passKind;
            if (passKind != ParameterPassKind::Ref && passKind != ParameterPassKind::Out) continue;
            const MethodEscapeOrigin argument =
                referenceEscapeOriginOfExpression(expression->children()[index], scopeOrdinal);
            if (argument.kind != MethodEscapeOriginKind::ExternalStorage) return argument;
        }
        return {};
    }

    if (expression->kind() == ast::AstNodeKind::UnaryExpression &&
        expression->tokenKind() == lexer::TokenKind::Star)
    {
        if (const std::optional<std::size_t> vla =
                variableArrayRootLocalOfExpression(expression->first(), scopeOrdinal);
            vla.has_value())
            return {MethodEscapeOriginKind::VariableLengthArray, vla,
                *vla < locals_.size()
                    ? std::optional<std::size_t>(locals_[*vla].scopeOrdinal)
                    : std::nullopt};
        // The pointer slot may live in this frame while its pointee does not. Without explicit
        // pointer provenance, treating the slot as the referent would be an unsound false claim.
        return {};
    }

    // Parentheses, ref/out wrappers, and other address-preserving shells keep the operand origin.
    // Non-addressable expressions never reach a valid ref binding.
    return referenceEscapeOriginOfExpression(expression->first(), scopeOrdinal);
}

bool ProjectMethodBodyModel::definiteAssignmentTracksLocal(std::size_t localOrdinal) const noexcept
{
    if (localOrdinal >= locals_.size())
    {
        return false;
    }
    const MethodLocalSymbol& local = locals_[localOrdinal];
    if (local.typeKey.empty() || local.typeKey == "<error>")
    {
        return false;
    }
    return !typeKeyHasDefinedDefaultStorage(local.typeKey);
}

// Walks an assignment target down to the identifier the write is rooted at, exactly the way
// verifyOutParameterAssignments walks one: `x = v`, `x.left = 0`, `x.rows[2].a = 1` and `*x = v`
// are all writes rooted at `x`. Returns nullptr when the target is not rooted at a plain name.
const ast::AstNode* ProjectMethodBodyModel::definiteAssignmentTargetRoot(
    const ast::AstNode* target) const noexcept
{
    while (target != nullptr && target->kind() != ast::AstNodeKind::IdentifierExpression)
    {
        switch (target->kind())
        {
            case ast::AstNodeKind::MemberExpression:
            case ast::AstNodeKind::IndexExpression:
            case ast::AstNodeKind::UnaryExpression:
                target = target->first();
                continue;
            default:
                return nullptr;
        }
    }
    return target;
}

// Records that writing `target` assigns the local it is rooted at, and -- when the whole local is
// the target -- suppresses the read diagnostic for that one identifier node, since an assignment
// analyzes its own left-hand side through the ordinary expression path.
//
// The distinction between the two cases is the entire subtlety here. Writing the WHOLE local
// (`x = v`) always assigns it. Writing THROUGH it only assigns it when the local is the storage
// being written -- an aggregate. `T[] items; items[0] = 1;` writes through a reference that was
// never given a value, so `items` stays unassigned and the read of it is still reported; that is
// a null dereference, and treating the store as an assignment would hide it. Aggregates are not
// tracked at all (see typeKeyHasDefinedDefaultStorage), so for them this is a no-op either way.
void ProjectMethodBodyModel::markAssignmentTarget(const ast::AstNode* target, std::size_t scopeOrdinal)
{
    const ast::AstNode* root = definiteAssignmentTargetRoot(target);
    if (root == nullptr)
    {
        return;
    }
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, root->text());
    if (!name.has_value() || name->kind != ScopeNameKind::Local)
    {
        return;
    }
    if (root != target)
    {
        return;
    }
    assignedLocals_.insert(name->semanticOrdinal);
    definiteAssignmentWriteTarget_ = root;
}

void ProjectMethodBodyModel::checkDefiniteAssignment(
    const ast::AstNode* expression,
    std::size_t localOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (expression == nullptr || expression == definiteAssignmentWriteTarget_) return;
    if (!definiteAssignmentTracksLocal(localOrdinal)) return;
    if (assignedLocals_.contains(localOrdinal)) return;
    if (!reportedUnassignedReads_.insert(expression).second) return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "use of unassigned local variable '" + locals_[localOrdinal].name + "'");
    // Treat it as assigned from here on. The program is already rejected; repeating the same
    // message at every later use of the same variable only buries the first one.
    assignedLocals_.insert(localOrdinal);
}

// ---------------------------------------------------------------------------------------------
// C7 "aggregate initializer"
//
// The element list an aggregate target expects, in the order a braced initializer fills it:
// N copies of T for a fixed array `T[N]`, and the instance fields in declaration order for a
// struct. A union is the one shape that is not a list: C initializes its FIRST member and nothing
// else, and so does this, which is why it reports its own arity instead of the field count.
//
// Returns nullopt when the type is not an aggregate at all -- the caller turns that into the
// "requires a struct or fixed-array target" diagnostic.
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
    diagnostics::DiagnosticEngine& diagnostics)
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
    if (binding.generalized && !elementTypeHasTrivialLifetime(targetTypeKey))
    {
        // T01-F05 must never silently leak a field that was initialized before a later element
        // throws. NativeSharp's current value-type lifetime model cannot yet register per-field
        // drop flags, so accept generalized initialization only where failure has nothing to
        // destroy. Constructors remain the explicit, cleanup-aware path for owning aggregates.
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "generalized initializer for '" + targetTypeKey +
                "' requires a trivially destructible target; use a constructor when fields own "
                "resources or the type declares a destructor");
        valid = false;
    }
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
            if (!elementTypeHasTrivialLifetime(targetTypeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    element->range(),
                    "spread initialization of '" + targetTypeKey +
                        "' requires a trivially copyable value; move-only fields require explicit construction");
                valid = false;
            }
            const ast::AstNode* source = element->first();
            const ExpressionResult sourceResult = analyzeExpression(
                source, methodMemberOrdinal, scopeOrdinal, diagnostics);
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
                "aggregate initializer for '" + targetTypeKey + "' has more values than elements");
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
        assigned[*target] = true;
        entry.targetElementOrdinals.push_back(*target);
        const std::string& elementTypeKey = (*elements)[*target];
        if (element->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            const ExpressionResult nested = bindAggregateInitializer(
                element, elementTypeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
            valid = valid && nested.valid;
            binding.entries.push_back(std::move(entry));
            continue;
        }
        const ExpressionResult value = analyzeExpression(
            element, methodMemberOrdinal, scopeOrdinal, diagnostics);
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
        binding.entries.push_back(std::move(entry));
    }

    std::vector<std::size_t> missing;
    for (std::size_t index = 0; index < assigned.size(); ++index)
        if (!assigned[index]) missing.push_back(index);
    if (spreadEntry.has_value() && *spreadEntry < binding.entries.size())
    {
        binding.entries[*spreadEntry].targetElementOrdinals = missing;
        for (const std::size_t index : missing) assigned[index] = true;
        missing.clear();
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

std::span<const MethodScope> ProjectMethodBodyModel::scopes() const noexcept
{
    return scopes_;
}

std::span<const MethodLocalSymbol> ProjectMethodBodyModel::locals() const noexcept
{
    return locals_;
}

std::span<const MethodNameBinding> ProjectMethodBodyModel::nameBindings() const noexcept
{
    return nameBindings_;
}

std::span<const MethodShadowingEvent> ProjectMethodBodyModel::shadowingEvents() const noexcept
{
    return shadowingEvents_;
}

std::span<const MethodExpressionTypeBinding> ProjectMethodBodyModel::expressionTypes() const noexcept
{
    return expressionTypes_;
}

const MethodAggregateInitializerBinding* ProjectMethodBodyModel::aggregateInitializerBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = aggregateInitializerBindingBySyntax_.find(syntax);
    return found == aggregateInitializerBindingBySyntax_.end()
        ? nullptr : &aggregateInitializerBindings_[found->second];
}

const std::string* ProjectMethodBodyModel::staticInterpolationValue(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = staticInterpolationValues_.find(syntax);
    return found == staticInterpolationValues_.end() ? nullptr : &found->second;
}

std::span<const MethodRuntimeTypeBinding> ProjectMethodBodyModel::runtimeTypeBindings() const noexcept
{
    return runtimeTypeBindings_;
}

std::span<const MethodCoreBinding> ProjectMethodBodyModel::coreBindings() const noexcept
{
    return coreBindings_;
}

const MethodVariableArrayIndexBinding* ProjectMethodBodyModel::variableArrayIndexBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = std::find_if(
        variableArrayIndexBindings_.begin(),
        variableArrayIndexBindings_.end(),
        [&](const MethodVariableArrayIndexBinding& binding)
        {
            return binding.syntax == syntax;
        });
    return found == variableArrayIndexBindings_.end() ? nullptr : &*found;
}

std::span<const MethodExceptionBinding> ProjectMethodBodyModel::exceptionBindings() const noexcept
{
    return exceptionBindings_;
}

std::span<const MethodCleanupRegion> ProjectMethodBodyModel::cleanupRegions() const noexcept
{
    return cleanupRegions_;
}

std::span<const MethodUsingBinding> ProjectMethodBodyModel::usingBindings() const noexcept
{
    return usingBindings_;
}

std::span<const MethodMoveBinding> ProjectMethodBodyModel::moveBindings() const noexcept
{
    return moveBindings_;
}

std::span<const MethodOwnershipTransferBinding> ProjectMethodBodyModel::ownershipTransferBindings() const noexcept
{
    return ownershipTransferBindings_;
}

std::span<const MethodBorrowBinding> ProjectMethodBodyModel::borrowBindings() const noexcept
{
    return borrowBindings_;
}

std::span<const MethodLocalBorrowBinding> ProjectMethodBodyModel::localBorrowBindings() const noexcept
{
    return localBorrowBindings_;
}

std::span<const MethodSwitchBinding> ProjectMethodBodyModel::switchBindings() const noexcept
{
    return switchBindings_;
}

std::span<const MethodStructuredExitBinding> ProjectMethodBodyModel::structuredExits() const noexcept
{
    return structuredExits_;
}

std::span<const MethodCleanupContinuation> ProjectMethodBodyModel::cleanupContinuations() const noexcept
{
    return cleanupContinuations_;
}

std::span<const MethodLambdaBinding> ProjectMethodBodyModel::lambdaBindings() const noexcept
{
    return lambdaBindings_;
}

std::span<const MethodLambdaValueBinding> ProjectMethodBodyModel::lambdaValueBindings() const noexcept
{
    return lambdaValueBindings_;
}

std::span<const MethodDelegateBinding> ProjectMethodBodyModel::delegateBindings() const noexcept
{
    return delegateBindings_;
}

std::span<const MethodEventBinding> ProjectMethodBodyModel::eventBindings() const noexcept
{
    return eventBindings_;
}

std::span<const MethodOptionalCarrierBinding>
ProjectMethodBodyModel::optionalCarrierBindings() const noexcept
{
    return optionalCarrierBindings_;
}

std::span<const MethodCallBinding> ProjectMethodBodyModel::calls() const noexcept
{
    return calls_;
}

std::span<const NamespaceFunctionCallBinding>
ProjectMethodBodyModel::namespaceFunctionCalls() const noexcept
{
    return namespaceFunctionCalls_;
}

const NamespaceFunctionCallBinding* ProjectMethodBodyModel::namespaceFunctionCall(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = namespaceFunctionCallBySyntax_.find(syntax);
    return found == namespaceFunctionCallBySyntax_.end()
        ? nullptr : &namespaceFunctionCalls_[found->second];
}

bool ProjectMethodBodyModel::callableBodyAnalyzed(std::size_t memberOrdinal) const noexcept
{
    return analyzedCallableMembers_.contains(memberOrdinal);
}

bool ProjectMethodBodyModel::namespaceFunctionBodyAnalyzed(
    std::size_t functionOrdinal) const noexcept
{
    return analyzedCallableMembers_.contains(namespaceCallableBase_ + functionOrdinal);
}

bool ProjectMethodBodyModel::isBuilt() const noexcept
{
    return built_;
}

std::size_t ProjectMethodBodyModel::createScope(
    MethodScopeKind kind,
    std::size_t methodMemberOrdinal,
    std::optional<std::size_t> parentScopeOrdinal)
{
    MethodScope scope;
    scope.kind = kind;
    scope.methodMemberOrdinal = methodMemberOrdinal;
    scope.parentScopeOrdinal = parentScopeOrdinal;
    if (parentScopeOrdinal.has_value() && *parentScopeOrdinal < scopes_.size())
    {
        scope.depth = scopes_[*parentScopeOrdinal].depth + 1;
    }

    const std::size_t ordinal = scopes_.size();
    scopes_.push_back(std::move(scope));
    scopeNames_.emplace_back();
    return ordinal;
}

bool ProjectMethodBodyModel::scopeContains(
    std::size_t ancestorScopeOrdinal,
    std::size_t scopeOrdinal) const noexcept
{
    if (ancestorScopeOrdinal >= scopes_.size() || scopeOrdinal >= scopes_.size())
    {
        return false;
    }

    std::optional<std::size_t> current = scopeOrdinal;
    while (current.has_value())
    {
        if (*current == ancestorScopeOrdinal)
        {
            return true;
        }
        current = scopes_[*current].parentScopeOrdinal;
    }
    return false;
}

ProjectMethodBodyModel::OwnershipFlowSnapshot ProjectMethodBodyModel::captureOwnershipFlow() const
{
    OwnershipFlowSnapshot state;
    state.reachable = ownershipFlowReachable_;
    state.movedRaiiLocals = movedRaiiLocals_;
    state.maybeMovedRaiiLocals = maybeMovedRaiiLocals_;
    state.movedOwnedParameters = movedOwnedParameters_;
    state.maybeMovedOwnedParameters = maybeMovedOwnedParameters_;
    state.invalidLocalBorrows = invalidLocalBorrows_;
    state.maybeInvalidLocalBorrows = maybeInvalidLocalBorrows_;
    state.assignedLocals = assignedLocals_;
    state.localCount = locals_.size();
    return state;
}

void ProjectMethodBodyModel::restoreOwnershipFlow(const OwnershipFlowSnapshot& state)
{
    ownershipFlowReachable_ = state.reachable;
    movedRaiiLocals_ = state.movedRaiiLocals;
    maybeMovedRaiiLocals_ = state.maybeMovedRaiiLocals;
    movedOwnedParameters_ = state.movedOwnedParameters;
    maybeMovedOwnedParameters_ = state.maybeMovedOwnedParameters;
    invalidLocalBorrows_ = state.invalidLocalBorrows;
    maybeInvalidLocalBorrows_ = state.maybeInvalidLocalBorrows;
    // C7: a snapshot cannot say anything about a local that did not exist when it was taken, and
    // restoring one must not silently un-assign such a local. Catch clauses are the case that
    // forced this: analyzeTry captures its catch-entry state, THEN declares the exception local
    // (which the handler itself binds, so it starts out assigned), THEN restores -- and a plain
    // assignment of the set would have thrown that binding away and reported every use of a
    // caught exception as a read of an unassigned variable. Local ordinals only ever grow, so
    // "declared after this snapshot" is exactly "ordinal >= its local count". Carrying those bits
    // across cannot mask a real error: a local declared after the snapshot point is, by lexical
    // scoping, not nameable on the path being restored.
    std::vector<std::size_t> declaredAfterSnapshot;
    for (const std::size_t ordinal : assignedLocals_)
    {
        if (ordinal >= state.localCount)
        {
            declaredAfterSnapshot.push_back(ordinal);
        }
    }
    assignedLocals_ = state.assignedLocals;
    assignedLocals_.insert(declaredAfterSnapshot.begin(), declaredAfterSnapshot.end());
}

// CC1-200. El patron de rama, en un solo sitio. Ver la declaracion en el header para el porque.
//
// Corre `work` partiendo EXACTAMENTE de `entry`, devuelve el estado en el que quedo, y deja el
// estado activo de vuelta en `entry`. El llamador combina dos de estos con commitBranchJoin.
ProjectMethodBodyModel::OwnershipFlowSnapshot ProjectMethodBodyModel::analyzeBranchState(
    const OwnershipFlowSnapshot& entry, const std::function<void()>& work)
{
    restoreOwnershipFlow(entry);
    work();
    OwnershipFlowSnapshot exitState = captureOwnershipFlow();
    restoreOwnershipFlow(entry);
    return exitState;
}

void ProjectMethodBodyModel::commitBranchJoin(
    const OwnershipFlowSnapshot& left, const OwnershipFlowSnapshot& right)
{
    restoreOwnershipFlow(joinOwnershipFlow(left, right));
}

ProjectMethodBodyModel::OwnershipFlowSnapshot ProjectMethodBodyModel::joinOwnershipFlow(
    const OwnershipFlowSnapshot& left,
    const OwnershipFlowSnapshot& right) const
{
    if (!left.reachable)
    {
        return right;
    }
    if (!right.reachable)
    {
        return left;
    }

    OwnershipFlowSnapshot result;
    result.reachable = true;

    const auto joinTriState = [](
        const std::unordered_set<std::size_t>& leftDefinite,
        const std::unordered_set<std::size_t>& leftMaybe,
        const std::unordered_set<std::size_t>& rightDefinite,
        const std::unordered_set<std::size_t>& rightMaybe,
        std::unordered_set<std::size_t>& outDefinite,
        std::unordered_set<std::size_t>& outMaybe)
    {
        for (const std::size_t ordinal : leftDefinite)
        {
            if (rightDefinite.contains(ordinal))
            {
                outDefinite.insert(ordinal);
            }
        }

        std::unordered_set<std::size_t> maySet = leftDefinite;
        maySet.insert(leftMaybe.begin(), leftMaybe.end());
        maySet.insert(rightDefinite.begin(), rightDefinite.end());
        maySet.insert(rightMaybe.begin(), rightMaybe.end());
        for (const std::size_t ordinal : maySet)
        {
            if (!outDefinite.contains(ordinal))
            {
                outMaybe.insert(ordinal);
            }
        }
    };

    joinTriState(
        left.movedRaiiLocals,
        left.maybeMovedRaiiLocals,
        right.movedRaiiLocals,
        right.maybeMovedRaiiLocals,
        result.movedRaiiLocals,
        result.maybeMovedRaiiLocals);
    joinTriState(
        left.movedOwnedParameters,
        left.maybeMovedOwnedParameters,
        right.movedOwnedParameters,
        right.maybeMovedOwnedParameters,
        result.movedOwnedParameters,
        result.maybeMovedOwnedParameters);
    joinTriState(
        left.invalidLocalBorrows,
        left.maybeInvalidLocalBorrows,
        right.invalidLocalBorrows,
        right.maybeInvalidLocalBorrows,
        result.invalidLocalBorrows,
        result.maybeInvalidLocalBorrows);
    // C7 "definite assignment": the only MUST set in this snapshot, so the join is an
    // intersection and not a union. `int x; if (c) { x = 1; }` leaves x unassigned afterwards
    // precisely because the else-path -- which here is the empty implicit one -- never wrote it.
    // The one exception is the same one restoreOwnershipFlow makes: a local the other side never
    // saw (it was declared after that side's snapshot was taken) is not evidence of anything, so
    // it is carried rather than intersected away.
    result.localCount = std::max(left.localCount, right.localCount);
    for (const std::size_t ordinal : left.assignedLocals)
    {
        if (right.assignedLocals.contains(ordinal) || ordinal >= right.localCount)
        {
            result.assignedLocals.insert(ordinal);
        }
    }
    for (const std::size_t ordinal : right.assignedLocals)
    {
        if (ordinal >= left.localCount)
        {
            result.assignedLocals.insert(ordinal);
        }
    }
    return result;
}

ProjectMethodBodyModel::OwnershipFlowSnapshot ProjectMethodBodyModel::joinOwnershipFlows(
    const OwnershipFlowSnapshot& seed,
    const std::vector<OwnershipFlowSnapshot>& states) const
{
    OwnershipFlowSnapshot result = seed;
    for (const OwnershipFlowSnapshot& state : states)
    {
        result = joinOwnershipFlow(result, state);
    }
    return result;
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
        if (localBorrowBindings_[ordinal].sourceOwnerLocalOrdinal == localOrdinal)
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
        if (localBorrowBindings_[ordinal].sourceOwnerParameterOrdinal == parameterOrdinal)
        {
            maybeInvalidLocalBorrows_.erase(ordinal);
            invalidLocalBorrows_.insert(ordinal);
        }
    }
}

bool ProjectMethodBodyModel::hasActiveBorrowOfOwnerLocal(
    std::size_t localOrdinal,
    std::size_t scopeOrdinal) const noexcept
{
    for (std::size_t ordinal = 0; ordinal < localBorrowBindings_.size(); ++ordinal)
    {
        const MethodLocalBorrowBinding& borrow = localBorrowBindings_[ordinal];
        if (borrow.sourceOwnerLocalOrdinal == localOrdinal &&
            scopeContains(borrow.creationScopeOrdinal, scopeOrdinal) &&
            localBorrowFlowState(ordinal) != MethodBorrowState::Invalidated)
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
    std::size_t scopeOrdinal) const noexcept
{
    for (std::size_t ordinal = 0; ordinal < localBorrowBindings_.size(); ++ordinal)
    {
        const MethodLocalBorrowBinding& borrow = localBorrowBindings_[ordinal];
        if (borrow.sourceOwnerParameterOrdinal == parameterOrdinal &&
            scopeContains(borrow.creationScopeOrdinal, scopeOrdinal) &&
            localBorrowFlowState(ordinal) != MethodBorrowState::Invalidated)
        {
            return true;
        }
    }
    return false;
}

void ProjectMethodBodyModel::seedParameters(
    std::size_t methodMemberOrdinal,
    std::size_t rootScopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr || rootScopeOrdinal >= scopeNames_.size())
    {
        return;
    }

    for (std::size_t parameterOrdinal = 0; parameterOrdinal < method->parameters.size(); ++parameterOrdinal)
    {
        const MethodParameterSymbol& parameter = method->parameters[parameterOrdinal];
        const std::optional<ScopeName> previous = lookupScopeNameInScope(rootScopeOrdinal, parameter.name);
        if (previous.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range,
                "duplicate parameter/local name '" + parameter.name + "' in method root scope");
            reportPrevious(previous->range, diagnostics, "previous declaration is here");
            continue;
        }

        scopeNames_[rootScopeOrdinal].push_back(ScopeName{
            ScopeNameKind::Parameter,
            parameter.name,
            parameterOrdinal,
            parameter.range});

        if (!memberCandidates(methodMemberOrdinal, parameter.name).empty())
        {
            shadowingEvents_.push_back(MethodShadowingEvent{
                MethodShadowingKind::ParameterShadowsMember,
                methodMemberOrdinal,
                rootScopeOrdinal,
                parameter.name,
                parameter.range});
        }
    }
}

void ProjectMethodBodyModel::analyzeBlock(
    const ast::AstNode* block,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    bool createNestedScope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (block == nullptr)
    {
        return;
    }

    std::size_t activeScope = scopeOrdinal;
    if (createNestedScope)
    {
        activeScope = createScope(MethodScopeKind::Block, methodMemberOrdinal, scopeOrdinal);
    }

    const std::size_t cleanupDepthAtEntry = activeCleanupRegions_.size();
    for (const ast::AstNode* statement : block->children())
    {
        // A switch section keeps its labels in the same child list as its statements, and the
        // labels were already analyzed and folded by analyzeSwitch before this runs. Skipping
        // them here is what lets a section reuse this routine wholesale -- nested scope, cleanup
        // continuations and the unreachable-code probe included -- instead of growing a second,
        // slightly-different copy of it.
        if (statement != nullptr && statement->kind() == ast::AstNodeKind::SwitchLabel)
        {
            continue;
        }
        if (ownershipFlowReachable_)
        {
            analyzeStatement(statement, methodMemberOrdinal, activeScope, diagnostics);
            continue;
        }

        // Unreachable source still needs semantic/type binding because later HIR/MIR
        // construction walks the full AST. Analyze it in a disposable reachable probe,
        // then restore the terminated ownership state so dead code never contributes to
        // a subsequent control-flow join.
        const OwnershipFlowSnapshot terminatedState = captureOwnershipFlow();
        OwnershipFlowSnapshot probeState = terminatedState;
        probeState.reachable = true;
        restoreOwnershipFlow(probeState);
        analyzeStatement(statement, methodMemberOrdinal, activeScope, diagnostics);
        restoreOwnershipFlow(terminatedState);
    }
    for (std::size_t index = activeCleanupRegions_.size(); index > cleanupDepthAtEntry; --index)
    {
        recordNormalCleanupContinuation(
            activeCleanupRegions_[index - 1], block, methodMemberOrdinal, activeScope);
    }
    activeCleanupRegions_.resize(cleanupDepthAtEntry);
}

void ProjectMethodBodyModel::analyzeStatement(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr)
    {
        return;
    }

    switch (statement->kind())
    {
        case ast::AstNodeKind::BlockStatement:
            analyzeBlock(statement, methodMemberOrdinal, scopeOrdinal, true, diagnostics);
            break;
        case ast::AstNodeKind::VariableDeclarationStatement:
            analyzeVariable(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::DeconstructionDeclarationStatement:
        {
            const ExpressionResult source = analyzeExpression(
                statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            const std::optional<std::vector<std::string>> elements =
                ProjectTypeConversion::tupleElementTypeKeys(source.typeKey);
            if (!source.valid) break;
            if (!elements.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first() != nullptr ? statement->first()->range() : statement->range(),
                    "a deconstruction declaration requires a tuple value");
                break;
            }
            if (elements->size() != statement->children().size())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidArgumentCount,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->range(),
                    "deconstruction declaration has " +
                        std::to_string(statement->children().size()) +
                        " binding(s), but the tuple has " + std::to_string(elements->size()));
                break;
            }
            if (!elementTypeHasTrivialLifetime(source.typeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->range(),
                    "deconstruction declaration of owning or move-only elements requires "
                    "per-element move bindings and drop flags");
                break;
            }
            std::unordered_set<std::string> declaredHere;
            for (std::size_t index = 0; index < statement->children().size(); ++index)
            {
                const ast::AstNode* binding = statement->children()[index];
                if (binding == nullptr || binding->text() == "_") continue;
                if (!declaredHere.insert(binding->text()).second)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        binding->range(),
                        "duplicate deconstruction binding '" + binding->text() + "'");
                    continue;
                }
                const std::optional<ScopeName> sameScope =
                    lookupScopeNameInScope(scopeOrdinal, binding->text());
                if (sameScope.has_value())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        binding->range(),
                        "duplicate local/parameter name '" + binding->text() +
                            "' in the same method scope");
                    reportPrevious(sameScope->range, diagnostics, "previous declaration is here");
                    continue;
                }
                MethodLocalSymbol local;
                local.methodMemberOrdinal = methodMemberOrdinal;
                local.scopeOrdinal = scopeOrdinal;
                local.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
                if (const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
                    method != nullptr) local.fileId = method->fileId;
                local.range = binding->range();
                local.syntax = binding;
                local.name = binding->text();
                local.typeKey = (*elements)[index];
                local.lambdaOrdinal = currentLambdaOrdinal_;
                if (const std::optional<ScopeName> outer =
                        lookupScopeName(scopeOrdinal, binding->text());
                    outer.has_value())
                {
                    MethodShadowingKind kind = outer->kind == ScopeNameKind::Parameter
                        ? MethodShadowingKind::LocalShadowsParameter
                        : MethodShadowingKind::LocalShadowsLocal;
                    shadowingEvents_.push_back(MethodShadowingEvent{
                        kind, methodMemberOrdinal, scopeOrdinal, binding->text(), binding->range()});
                }
                else if (!memberCandidates(methodMemberOrdinal, binding->text()).empty())
                {
                    shadowingEvents_.push_back(MethodShadowingEvent{
                        MethodShadowingKind::LocalShadowsMember,
                        methodMemberOrdinal,
                        scopeOrdinal,
                        binding->text(),
                        binding->range()});
                }
                const std::size_t localOrdinal = locals_.size();
                locals_.push_back(std::move(local));
                scopes_[scopeOrdinal].localOrdinals.push_back(localOrdinal);
                scopeNames_[scopeOrdinal].push_back(ScopeName{
                    ScopeNameKind::Local,
                    binding->text(),
                    localOrdinal,
                    binding->range()});
                assignedLocals_.insert(localOrdinal);
            }
            break;
        }
        case ast::AstNodeKind::IfStatement:
        {
            const ExpressionResult condition = analyzeExpression(
                statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (condition.valid && condition.typeKey != "builtin:bool")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCondition,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "if condition must have type bool");
            }

            // CC1-200. Este case ya hacia lo correcto a mano; ahora lo hace a traves de las dos
            // funciones compartidas. El cambio no altera su comportamiento -- es literalmente el
            // mismo capture / restore(entry) / analizar / capture / restore(join) -- y ese es el
            // punto: `?:`, `&&` y `||` estaban rotos porque tenian que ACORDARSE de repetir este
            // patron, y no se acordaron. Reescribirlo aqui es lo que demuestra que la pieza
            // compartida es equivalente a la version que ya era correcta.
            const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
            const OwnershipFlowSnapshot thenState = analyzeBranchState(
                branchEntry,
                [&]
                {
                    analyzeBlock(
                        statement->second(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
                });
            const OwnershipFlowSnapshot elseState = analyzeBranchState(
                branchEntry,
                [&]
                {
                    if (statement->third() == nullptr) return;
                    if (statement->third()->kind() == ast::AstNodeKind::IfStatement)
                    {
                        analyzeStatement(
                            statement->third(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                    }
                    else
                    {
                        analyzeBlock(
                            statement->third(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
                    }
                });
            commitBranchJoin(thenState, elseState);
            break;
        }
        // CC1-210/CC1-211/CC1-212. Los dos bucles COMPARTEN este case, pero ya NO comparten su
        // semantica de flujo: las dos diferencias reales estan marcadas abajo con `isDoWhile`.
        //
        // Por que un solo case y no dos: lo unico que distingue a un do-while de un while son (a)
        // el orden entre cuerpo y condicion y (b) que NO existe el camino de cero iteraciones. Todo
        // lo demas -- el LoopContext, la union de los estados de `continue`, y sobre todo la
        // comprobacion de RAII a traves del back-edge, que son sesenta lineas -- es identico.
        // Duplicarlo daria dos copias de la parte dificil que habria que mantener en paso para
        // siempre, que es exactamente el problema que este milestone existe para reducir.
        //
        // El comentario anterior afirmaba que analizar un do-while como un while era "meramente
        // conservador". Para definite assignment eso era falso en un sentido: incluir el camino de
        // cero iteraciones en una INTERSECCION hacia que una variable asignada solo en el cuerpo NO
        // contara como definitivamente asignada despues del bucle -- rechazaba codigo valido. No era
        // inseguro, pero era incorrecto, y la correccion es esta.
        case ast::AstNodeKind::DoWhileStatement:
        case ast::AstNodeKind::WhileStatement:
        {
            const bool isDoWhile = statement->kind() == ast::AstNodeKind::DoWhileStatement;

            // La condicion se analiza EXACTAMENTE UNA VEZ, aqui para un while y despues del cuerpo
            // para un do-while. Una vez y no cero: aunque el punto en que se analiza sea
            // inalcanzable, el HIR va a recorrer esa sintaxis igual y necesita su binding de tipo.
            const auto analyzeLoopCondition = [&]()
            {
                const ExpressionResult condition = analyzeExpression(
                    statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                if (condition.valid && condition.typeKey != "builtin:bool")
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidCondition,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->first()->range(),
                        isDoWhile
                            ? "do-while condition must have type bool"
                            : "while condition must have type bool");
                }
            };

            if (!isDoWhile)
            {
                analyzeLoopCondition();
            }

            const OwnershipFlowSnapshot loopEntry = captureOwnershipFlow();
            const std::size_t localCountAtBodyEntry = locals_.size();
            const ClassMemberSymbol* loopMethod = methodSymbol(methodMemberOrdinal);
            loopStack_.push_back(LoopContext{
                scopeOrdinal,
                activeCleanupRegions_.size(),
                activeCleanupRegions_.size(),
                {},
                {}});
            analyzeBlock(statement->second(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
            const OwnershipFlowSnapshot fallthroughState = captureOwnershipFlow();
            LoopContext loopContext = std::move(loopStack_.back());
            loopStack_.pop_back();

            std::vector<OwnershipFlowSnapshot> backEdgeStates = std::move(loopContext.continueStates);
            if (fallthroughState.reachable)
            {
                backEdgeStates.push_back(fallthroughState);
            }
            OwnershipFlowSnapshot noBackEdge;
            noBackEdge.reachable = false;
            const OwnershipFlowSnapshot backEdgeState = joinOwnershipFlows(noBackEdge, backEdgeStates);

            // CC1-212. En un do-while la condicion se evalua DESPUES del cuerpo, sobre el estado que
            // llega por el back-edge. Eso es lo que hace que la condicion pueda leer una variable
            // asignada dentro del cuerpo, que es legal y antes se rechazaba.
            OwnershipFlowSnapshot afterCondition;
            afterCondition.reachable = false;
            if (isDoWhile)
            {
                if (backEdgeState.reachable)
                {
                    restoreOwnershipFlow(backEdgeState);
                    analyzeLoopCondition();
                    afterCondition = captureOwnershipFlow();
                }
                else
                {
                    // Cuerpo que nunca cae ni continua (todas sus salidas retornan o lanzan). La
                    // condicion es codigo inalcanzable, pero el HIR va a recorrerla igual, asi que
                    // se analiza en una sonda alcanzable desechable -- el mismo patron que usa
                    // analyzeBlock para el codigo muerto -- y el estado terminado se restaura.
                    const OwnershipFlowSnapshot terminated = captureOwnershipFlow();
                    OwnershipFlowSnapshot probe = backEdgeState;
                    probe.reachable = true;
                    restoreOwnershipFlow(probe);
                    analyzeLoopCondition();
                    restoreOwnershipFlow(terminated);
                }
            }

            if (backEdgeState.reachable)
            {
                for (std::size_t localOrdinal = 0; localOrdinal < localCountAtBodyEntry; ++localOrdinal)
                {
                    if (localOrdinal >= locals_.size() || !locals_[localOrdinal].isRaii)
                    {
                        continue;
                    }
                    const bool aliveAtEntry = !loopEntry.movedRaiiLocals.contains(localOrdinal) &&
                        !loopEntry.maybeMovedRaiiLocals.contains(localOrdinal);
                    const bool movedAtBackEdge = backEdgeState.movedRaiiLocals.contains(localOrdinal) ||
                        backEdgeState.maybeMovedRaiiLocals.contains(localOrdinal);
                    if (aliveAtEntry && movedAtBackEdge)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            statement->range(),
                            "RAII owner '" + locals_[localOrdinal].name +
                                "' may remain moved across a while-loop back-edge; reinitialize it before the next iteration");
                    }
                }
                if (loopMethod != nullptr)
                {
                    for (std::size_t parameterOrdinal = 0;
                         parameterOrdinal < loopMethod->parameters.size(); ++parameterOrdinal)
                    {
                        if (loopMethod->parameters[parameterOrdinal].ownership != sema::OwnershipKind::Owned)
                        {
                            continue;
                        }
                        const bool aliveAtEntry = !loopEntry.movedOwnedParameters.contains(parameterOrdinal) &&
                            !loopEntry.maybeMovedOwnedParameters.contains(parameterOrdinal);
                        const bool movedAtBackEdge = backEdgeState.movedOwnedParameters.contains(parameterOrdinal) ||
                            backEdgeState.maybeMovedOwnedParameters.contains(parameterOrdinal);
                        if (aliveAtEntry && movedAtBackEdge)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                statement->range(),
                                "owned parameter '" + loopMethod->parameters[parameterOrdinal].name +
                                    "' may remain moved across a while-loop back-edge; transfer it only on an exiting path");
                        }
                    }
                }
            }

            // CC1-211. AQUI ESTA LA SEGUNDA DIFERENCIA, y es la que importa para definite
            // assignment: un while puede no ejecutar el cuerpo NI UNA VEZ, asi que su estado
            // posterior parte de `loopEntry`; un do-while SIEMPRE lo ejecuta, asi que ese camino no
            // existe y no debe entrar en el join. Como el join de un conjunto MUST es una
            // interseccion, incluir `loopEntry` en un do-while borraba toda asignacion hecha en el
            // cuerpo.
            OwnershipFlowSnapshot postLoop;
            if (isDoWhile)
            {
                postLoop.reachable = false;
                postLoop = joinOwnershipFlows(postLoop, loopContext.breakStates);
                if (afterCondition.reachable)
                {
                    postLoop = joinOwnershipFlow(postLoop, afterCondition);
                }
            }
            else
            {
                postLoop = loopEntry;
                postLoop = joinOwnershipFlows(postLoop, loopContext.breakStates);
                if (backEdgeState.reachable)
                {
                    postLoop = joinOwnershipFlow(postLoop, backEdgeState);
                }
            }
            restoreOwnershipFlow(postLoop);
            break;
        }
        case ast::AstNodeKind::ForStatement:
            analyzeFor(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::ForeachStatement:
            analyzeForeach(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::SwitchStatement:
            analyzeSwitch(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::ReturnStatement:
            analyzeReturn(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            ownershipFlowReachable_ = false;
            break;
        case ast::AstNodeKind::TryStatement:
            analyzeTry(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::UsingStatement:
            analyzeUsing(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::ThrowStatement:
            analyzeThrow(statement, methodMemberOrdinal, scopeOrdinal, diagnostics);
            ownershipFlowReachable_ = false;
            break;
        case ast::AstNodeKind::StaticAssertStatement:
        {
            const ExpressionResult condition = analyzeExpression(
                statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (condition.valid && condition.typeKey != "builtin:bool")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCondition,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "static_assert condition must have type bool");
            }
            break;
        }
        case ast::AstNodeKind::InlineAssemblyStatement:
            for (ast::AstNode* child : statement->children())
            {
                if (child->kind() == ast::AstNodeKind::InlineAssemblyInputOperand &&
                    child->first() != nullptr)
                {
                    const ExpressionResult input = analyzeExpression(
                        child->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                    if (child->auxiliaryText() == "i" && input.valid &&
                        (!ProjectTypeConversion::isIntegral(input.typeKey) ||
                         !input.integerConstant.has_value()))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
                            diagnostics::DiagnosticSeverity::Error,
                            child->first()->range(),
                            "asm immediate input requires a compile-time integral constant");
                    }
                    else if (child->auxiliaryText() == "m" && input.valid && !input.addressable)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
                            diagnostics::DiagnosticSeverity::Error,
                            child->first()->range(),
                            "asm memory input requires an addressable storage location");
                    }
                }
                else if (child->kind() == ast::AstNodeKind::InlineAssemblyOutputOperand &&
                    child->first() != nullptr)
                {
                    const ast::AstNode* const previousWriteTarget = definiteAssignmentWriteTarget_;
                    markAssignmentTarget(child->first(), scopeOrdinal);
                    const ExpressionResult target = analyzeExpression(
                        child->first(),
                        methodMemberOrdinal,
                        scopeOrdinal,
                        diagnostics,
                        false,
                        true,
                        false,
                        true);
                    definiteAssignmentWriteTarget_ = previousWriteTarget;
                    if (target.valid && !target.assignable)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidAssignmentTarget,
                            diagnostics::DiagnosticSeverity::Error,
                            child->first()->range(),
                            "asm output must be a modifiable lvalue");
                    }
                    else if ((child->auxiliaryText() == "=m" ||
                              child->auxiliaryText() == "+m") &&
                        target.valid && !target.addressable)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
                            diagnostics::DiagnosticSeverity::Error,
                            child->first()->range(),
                            "asm memory output requires an addressable storage location");
                    }
                }
            }
            // Item 298: the project pipeline now has an explicit HIR/MIR representation for the
            // complete validated contract. The standalone frontend keeps its pre-lowering
            // diagnostic until it acquires the same physical backend path.
            break;
        case ast::AstNodeKind::ExpressionStatement:
        {
            const ExpressionResult expression = analyzeExpression(
                statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (expression.valid && expression.typeKey == "builtin:never")
                ownershipFlowReachable_ = false;
            break;
        }
        case ast::AstNodeKind::BreakStatement:
            if (analyzeLoopExit(
                    statement,
                    MethodStructuredExitKind::Break,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    diagnostics))
            {
                ownershipFlowReachable_ = false;
            }
            break;
        case ast::AstNodeKind::ContinueStatement:
            if (analyzeLoopExit(
                    statement,
                    MethodStructuredExitKind::Continue,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    diagnostics))
            {
                ownershipFlowReachable_ = false;
            }
            break;
        default:
            break;
    }
}

std::optional<std::size_t> ProjectMethodBodyModel::exceptionRootTypeOrdinal() const noexcept
{
    if (nameLookup_ == nullptr)
    {
        return std::nullopt;
    }
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[ordinal];
        if (type.kind == NamedTypeDeclarationKind::Class &&
            type.identity.fullyQualifiedName() == "NativeSharp.Core.Exception")
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::isExceptionTypeOrdinal(std::size_t typeOrdinal) const noexcept
{
    if (inheritanceModel_ == nullptr)
    {
        return false;
    }
    const std::optional<std::size_t> root = exceptionRootTypeOrdinal();
    return root.has_value() && inheritanceModel_->isClass(typeOrdinal) &&
        inheritanceModel_->isDerivedFrom(typeOrdinal, *root, true);
}

std::size_t ProjectMethodBodyModel::addCatchLocal(
    const ast::AstNode* catchClause,
    std::string typeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (catchClause == nullptr || scopeOrdinal >= scopeNames_.size())
    {
        return static_cast<std::size_t>(-1);
    }
    const std::optional<ScopeName> sameScope = lookupScopeNameInScope(scopeOrdinal, catchClause->text());
    if (sameScope.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            catchClause->range(),
            "duplicate catch/local name '" + catchClause->text() + "' in the same scope");
        reportPrevious(sameScope->range, diagnostics, "previous declaration is here");
        return static_cast<std::size_t>(-1);
    }

    MethodLocalSymbol local;
    local.methodMemberOrdinal = methodMemberOrdinal;
    local.scopeOrdinal = scopeOrdinal;
    local.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
    if (const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal); method != nullptr)
    {
        local.fileId = method->fileId;
    }
    local.range = catchClause->range();
    local.syntax = catchClause;
    local.name = catchClause->text();
    local.typeKey = std::move(typeKey);
    local.lambdaOrdinal = currentLambdaOrdinal_;

    const std::size_t localOrdinal = locals_.size();
    locals_.push_back(std::move(local));
    scopes_[scopeOrdinal].localOrdinals.push_back(localOrdinal);
    scopeNames_[scopeOrdinal].push_back(ScopeName{
        ScopeNameKind::Local, catchClause->text(), localOrdinal, catchClause->range()});
    // C7: the exception object is bound by the handler itself, never by user code.
    assignedLocals_.insert(localOrdinal);
    return localOrdinal;
}

std::optional<std::size_t> ProjectMethodBodyModel::disposableTypeOrdinal() const noexcept
{
    if (nameLookup_ == nullptr)
    {
        return std::nullopt;
    }
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[ordinal];
        if (type.kind == NamedTypeDeclarationKind::Interface &&
            type.identity.fullyQualifiedName() == "NativeSharp.Core.IDisposable")
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ProjectMethodBodyModel::disposableContractMemberOrdinal(
    std::size_t disposableTypeOrdinalValue) const noexcept
{
    if (interfaceModel_ == nullptr || memberModel_ == nullptr)
    {
        return std::nullopt;
    }
    const InterfaceTypeInfo* info = interfaceModel_->interfaceInfo(disposableTypeOrdinalValue);
    if (info == nullptr)
    {
        return std::nullopt;
    }

    std::optional<std::size_t> result;
    for (const std::size_t memberOrdinal : info->effectiveMethodMemberOrdinals)
    {
        if (memberOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Method || member.name != "Dispose" ||
            !member.parameters.empty() || member.declaredTypeKey != "builtin:void" || member.isStatic)
        {
            continue;
        }
        if (result.has_value())
        {
            return std::nullopt;
        }
        result = memberOrdinal;
    }
    return result;
}

std::size_t ProjectMethodBodyModel::addUsingLocal(
    const ast::AstNode* statement,
    std::string typeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr || scopeOrdinal >= scopeNames_.size())
    {
        return static_cast<std::size_t>(-1);
    }
    const std::optional<ScopeName> sameScope = lookupScopeNameInScope(scopeOrdinal, statement->text());
    if (sameScope.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "duplicate using/local name '" + statement->text() + "' in the same scope");
        reportPrevious(sameScope->range, diagnostics, "previous declaration is here");
        return static_cast<std::size_t>(-1);
    }

    MethodLocalSymbol local;
    local.methodMemberOrdinal = methodMemberOrdinal;
    local.scopeOrdinal = scopeOrdinal;
    local.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
    if (const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal); method != nullptr)
    {
        local.fileId = method->fileId;
    }
    local.range = statement->range();
    local.syntax = statement;
    local.name = statement->text();
    local.typeKey = std::move(typeKey);
    local.lambdaOrdinal = currentLambdaOrdinal_;

    const std::optional<ScopeName> outer = lookupScopeName(scopeOrdinal, statement->text());
    if (outer.has_value())
    {
        MethodShadowingKind kind = MethodShadowingKind::LocalShadowsLocal;
        if (outer->kind == ScopeNameKind::Parameter)
        {
            kind = MethodShadowingKind::LocalShadowsParameter;
        }
        shadowingEvents_.push_back(MethodShadowingEvent{
            kind,
            methodMemberOrdinal,
            scopeOrdinal,
            statement->text(),
            statement->range()});
    }
    else if (!memberCandidates(methodMemberOrdinal, statement->text()).empty())
    {
        shadowingEvents_.push_back(MethodShadowingEvent{
            MethodShadowingKind::LocalShadowsMember,
            methodMemberOrdinal,
            scopeOrdinal,
            statement->text(),
            statement->range()});
    }

    const std::size_t localOrdinal = locals_.size();
    locals_.push_back(std::move(local));
    scopes_[scopeOrdinal].localOrdinals.push_back(localOrdinal);
    scopeNames_[scopeOrdinal].push_back(ScopeName{
        ScopeNameKind::Local, statement->text(), localOrdinal, statement->range()});
    // C7: the foreach element is bound by the loop itself, never by user code.
    assignedLocals_.insert(localOrdinal);
    return localOrdinal;
}

void ProjectMethodBodyModel::analyzeUsing(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr ||
        inheritanceModel_ == nullptr || interfaceModel_ == nullptr)
    {
        return;
    }

    const std::size_t usingScope = createScope(
        MethodScopeKind::Using,
        methodMemberOrdinal,
        scopeOrdinal);
    const std::string resourceTypeKey = memberModel_->canonicalTypeKey(statement->typeSyntax(), *nameLookup_);

    ExpressionResult initializer;
    if (statement->first() != nullptr)
    {
        initializer = analyzeExpression(
            statement->first(), methodMemberOrdinal, usingScope, diagnostics);
        if (initializer.valid)
        {
            const ProjectImplicitConversion conversion = classifyImplicit(
                initializer.typeKey,
                resourceTypeKey,
                initializer.integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "cannot initialize using resource '" + statement->text() + "' of type '" +
                        resourceTypeKey + "' from '" + initializer.typeKey + "'");
            }
        }
    }

    const std::size_t localOrdinal = addUsingLocal(
        statement, resourceTypeKey, methodMemberOrdinal, usingScope, diagnostics);

    const std::optional<std::size_t> disposableOrdinal = disposableTypeOrdinal();
    if (!disposableOrdinal.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "NativeSharp.Core.IDisposable must be declared as an interface before using semantics can be used");
        if (statement->second() != nullptr)
        {
            analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
        }
        return;
    }

    const std::optional<std::size_t> disposeContract = disposableContractMemberOrdinal(*disposableOrdinal);
    if (!disposeContract.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "NativeSharp.Core.IDisposable must declare exactly one instance 'void Dispose()' contract");
        if (statement->second() != nullptr)
        {
            analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
        }
        return;
    }

    const std::optional<std::size_t> resourceTypeOrdinal =
        inheritanceModel_->typeOrdinalForTypeKey(resourceTypeKey);
    bool disposable = false;
    bool interfaceDispatch = false;
    std::optional<std::size_t> implementation;
    if (resourceTypeOrdinal.has_value() && *resourceTypeOrdinal < nameLookup_->namedTypes().size())
    {
        const NamedTypeDeclarationKind kind = nameLookup_->namedTypes()[*resourceTypeOrdinal].kind;
        if (kind == NamedTypeDeclarationKind::Class)
        {
            disposable = interfaceModel_->classImplementsInterface(*resourceTypeOrdinal, *disposableOrdinal);
            if (disposable)
            {
                const ClassInterfaceTablePlan* plan = interfaceModel_->tablePlan(
                    *resourceTypeOrdinal, *disposableOrdinal);
                if (plan != nullptr)
                {
                    for (std::size_t index = 0; index < plan->interfaceMethodMemberOrdinals.size(); ++index)
                    {
                        if (plan->interfaceMethodMemberOrdinals[index] == *disposeContract &&
                            index < plan->implementationMemberOrdinals.size())
                        {
                            implementation = plan->implementationMemberOrdinals[index];
                            break;
                        }
                    }
                }
                interfaceDispatch = !implementation.has_value();
            }
        }
        else if (kind == NamedTypeDeclarationKind::Interface)
        {
            disposable = interfaceModel_->interfaceDerivesFrom(
                *resourceTypeOrdinal, *disposableOrdinal, true);
            interfaceDispatch = disposable;
        }
    }

    if (!disposable)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "using resource type '" + resourceTypeKey +
                "' must implement NativeSharp.Core.IDisposable and provide instance void Dispose()");
        if (statement->second() != nullptr)
        {
            analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
        }
        return;
    }

    if (localOrdinal == static_cast<std::size_t>(-1) || !resourceTypeOrdinal.has_value())
    {
        if (statement->second() != nullptr)
        {
            analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
        }
        return;
    }

    MethodUsingBinding binding;
    binding.syntax = statement;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = usingScope;
    binding.resourceLocalOrdinal = localOrdinal;
    binding.resourceTypeKey = resourceTypeKey;
    binding.resourceTypeOrdinal = *resourceTypeOrdinal;
    binding.disposableTypeOrdinal = *disposableOrdinal;
    binding.disposeContractMemberOrdinal = *disposeContract;
    binding.disposeImplementationMemberOrdinal = implementation;
    binding.interfaceDispatch = interfaceDispatch;
    usingBindings_.push_back(binding);

    const std::size_t cleanupRegionOrdinal = createCleanupRegion(
        MethodCleanupRegionKind::UsingDispose,
        statement,
        nullptr,
        methodMemberOrdinal,
        usingScope);
    MethodCleanupRegion& region = cleanupRegions_[cleanupRegionOrdinal];
    region.resourceLocalOrdinal = localOrdinal;
    region.disposeContractMemberOrdinal = *disposeContract;
    region.disposeImplementationMemberOrdinal = implementation;
    recordNormalCleanupContinuation(
        cleanupRegionOrdinal, statement, methodMemberOrdinal, usingScope);

    activeCleanupRegions_.push_back(cleanupRegionOrdinal);
    if (statement->second() != nullptr)
    {
        analyzeBlock(statement->second(), methodMemberOrdinal, usingScope, false, diagnostics);
    }
    activeCleanupRegions_.pop_back();
}

void ProjectMethodBodyModel::analyzeTry(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr)
    {
        return;
    }

    if (statement->children().empty() && statement->second() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "try statement requires at least one catch clause or a finally block");
    }

    std::optional<std::size_t> cleanupRegionOrdinal;
    if (statement->second() != nullptr)
    {
        cleanupRegionOrdinal = createCleanupRegion(
            MethodCleanupRegionKind::Finally,
            statement,
            statement->second(),
            methodMemberOrdinal,
            scopeOrdinal);
        recordNormalCleanupContinuation(
            *cleanupRegionOrdinal, statement, methodMemberOrdinal, scopeOrdinal);
        activeCleanupRegions_.push_back(*cleanupRegionOrdinal);
    }

    const OwnershipFlowSnapshot tryEntryState = captureOwnershipFlow();
    const std::size_t throwStateBegin = throwFlowStates_.size();
    analyzeBlock(statement->first(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
    const OwnershipFlowSnapshot tryExitState = captureOwnershipFlow();

    // A catch can observe ownership state from any explicit throw point and, conservatively,
    // from both try entry and normal try exit to account for other potentially-throwing calls.
    OwnershipFlowSnapshot catchEntryState = joinOwnershipFlow(tryEntryState, tryExitState);
    for (std::size_t index = throwStateBegin; index < throwFlowStates_.size(); ++index)
    {
        catchEntryState = joinOwnershipFlow(catchEntryState, throwFlowStates_[index]);
    }
    OwnershipFlowSnapshot joinedExitState = tryExitState;

    const std::optional<std::size_t> exceptionRoot = exceptionRootTypeOrdinal();
    bool sawCatchAll = false;
    std::vector<std::size_t> previousTypedCatchOrdinals;
    for (const ast::AstNode* catchClause : statement->children())
    {
        if (catchClause == nullptr || catchClause->kind() != ast::AstNodeKind::CatchClause)
        {
            continue;
        }
        const std::size_t catchScope = createScope(MethodScopeKind::Catch, methodMemberOrdinal, scopeOrdinal);
        MethodExceptionBinding binding;
        binding.kind = MethodExceptionOperationKind::Catch;
        binding.syntax = catchClause;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = catchScope;
        binding.catchAll = catchClause->flag();

        if (sawCatchAll)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                catchClause->range(),
                "catch clause is unreachable because a previous catch-all handles every exception");
        }

        if (catchClause->flag())
        {
            sawCatchAll = true;
            binding.exceptionTypeKey = "<any-exception>";
        }
        else
        {
            const std::string typeKey = memberModel_->canonicalTypeKey(catchClause->typeSyntax(), *nameLookup_);
            binding.exceptionTypeKey = typeKey;
            const std::optional<std::size_t> typeOrdinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
            binding.exceptionTypeOrdinal = typeOrdinal;

            if (!exceptionRoot.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    catchClause->range(),
                    "NativeSharp.Core.Exception must be declared before typed catch/throw semantics can be used");
            }
            else if (!typeOrdinal.has_value() || !isExceptionTypeOrdinal(*typeOrdinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    catchClause->range(),
                    "catch type '" + typeKey + "' must be NativeSharp.Core.Exception or derive from it");
            }
            else
            {
                for (const std::size_t previous : previousTypedCatchOrdinals)
                {
                    if (inheritanceModel_->isDerivedFrom(*typeOrdinal, previous, true))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            catchClause->range(),
                            "catch type '" + typeKey + "' is unreachable because an earlier catch handles the same or a base exception type");
                        break;
                    }
                }
                previousTypedCatchOrdinals.push_back(*typeOrdinal);
            }
            if (!catchClause->text().empty())
            {
                addCatchLocal(catchClause, typeKey, methodMemberOrdinal, catchScope, diagnostics);
            }
        }

        exceptionBindings_.push_back(std::move(binding));
        restoreOwnershipFlow(catchEntryState);
        ++currentCatchDepth_;
        analyzeBlock(catchClause->first(), methodMemberOrdinal, catchScope, false, diagnostics);
        --currentCatchDepth_;
        joinedExitState = joinOwnershipFlow(joinedExitState, captureOwnershipFlow());
    }

    restoreOwnershipFlow(joinedExitState);
    if (cleanupRegionOrdinal.has_value())
    {
        activeCleanupRegions_.pop_back();
    }

    if (statement->second() != nullptr)
    {
        const OwnershipFlowSnapshot postTryState = captureOwnershipFlow();
        OwnershipFlowSnapshot finallyInput = postTryState;
        if (!finallyInput.reachable)
        {
            finallyInput = catchEntryState;
            finallyInput.reachable = true;
        }
        restoreOwnershipFlow(finallyInput);
        finallyStack_.push_back(FinallyContext{
            *cleanupRegionOrdinal,
            loopStack_.size()});
        analyzeBlock(statement->second(), methodMemberOrdinal, scopeOrdinal, true, diagnostics);
        finallyStack_.pop_back();
        const OwnershipFlowSnapshot finalState = captureOwnershipFlow();
        if (postTryState.reachable)
        {
            restoreOwnershipFlow(finalState);
        }
        else
        {
            restoreOwnershipFlow(postTryState);
        }
    }
}

void ProjectMethodBodyModel::analyzeThrow(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr)
    {
        return;
    }

    MethodExceptionBinding binding;
    binding.syntax = statement;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;

    if (statement->first() == nullptr)
    {
        binding.kind = MethodExceptionOperationKind::Rethrow;
        if (currentCatchDepth_ == 0)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "throw; is only valid within a catch block");
        }
        exceptionBindings_.push_back(std::move(binding));
        throwFlowStates_.push_back(captureOwnershipFlow());
        return;
    }

    binding.kind = MethodExceptionOperationKind::Throw;
    const ExpressionResult value = analyzeExpression(
        statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (value.valid)
    {
        binding.exceptionTypeKey = value.typeKey;
        if (inheritanceModel_ != nullptr)
        {
            const std::optional<std::size_t> ordinal = inheritanceModel_->typeOrdinalForTypeKey(value.typeKey);
            binding.exceptionTypeOrdinal = ordinal;
            if (!exceptionRootTypeOrdinal().has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "NativeSharp.Core.Exception must be declared before throw semantics can be used");
            }
            else if (!ordinal.has_value() || !isExceptionTypeOrdinal(*ordinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "throw operand type '" + value.typeKey + "' must be NativeSharp.Core.Exception or derive from it");
            }
        }
    }
    exceptionBindings_.push_back(std::move(binding));
    throwFlowStates_.push_back(captureOwnershipFlow());
}

void ProjectMethodBodyModel::analyzeVariable(
    const ast::AstNode* variable,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (variable == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr ||
        scopeOrdinal >= scopeNames_.size())
    {
        return;
    }

    const bool isReferenceLocal = variable->auxiliaryText() == "local:ref";
    const bool isPinnedLocal = variable->auxiliaryText() == "local:pinned";
    const bool isForeachEnumerator =
        variable->auxiliaryText() == "local:foreach-enumerator";

    // `var`: the parser bypasses parseType() entirely for this keyword and hands us a marker
    // TypeSyntax (baseTokenKind() == TokenKind::KwVar) instead -- see Parser::parseVariableDeclaration.
    // Infer the real typeKey from the initializer's own resolved type, analyzed here (before any
    // of the diagnostics below, which all need a real typeKey) with no delegate-target-typing --
    // matching real C#'s restriction that `var` cannot bind a bare lambda or method group, since
    // there is no declared delegate type to bind it against. The resulting ExpressionResult is
    // reused later in this function instead of being re-analyzed.
    const bool isVarInferred =
        variable->typeSyntax() != nullptr && variable->typeSyntax()->baseTokenKind() == TokenKind::KwVar;
    std::optional<ExpressionResult> precomputedVarInitializer;
    std::string varInferredTypeKey;
    if (isVarInferred)
    {
        if (isReferenceLocal)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                variable->range(),
                "reference locals require an explicit element type; 'ref var' is not supported");
            return;
        }
        if (variable->first() == nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                variable->range(),
                "'var' requires an initializer");
            return;
        }
        // C7 "aggregate initializer": a braced initializer is filled AGAINST a declared type; it
        // has no type of its own to infer from, exactly like a bare lambda or a method group.
        if (variable->first()->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "'var' cannot infer a type from an aggregate initializer; name the struct or "
                "fixed-array type");
            return;
        }
        const bool isRaiiForVar = variable->hasModifier(TokenKind::KwRaii);
        ExpressionResult varInit = analyzeExpression(
            variable->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
            /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/isRaiiForVar);
        if (!varInit.valid || varInit.typeKey.empty() || varInit.typeKey == "<error>")
        {
            return;
        }
        if (varInit.typeKey == "builtin:void")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "'var' cannot infer a type from a void-returning expression");
            return;
        }
        varInferredTypeKey = varInit.typeKey;
        precomputedVarInitializer = std::move(varInit);
    }

    bool isVariableLengthArray = false;
    std::string variableLengthArrayElementTypeKey;
    const ast::AstNode* variableLengthArrayCountSyntax = nullptr;
    std::vector<const ast::AstNode*> variableLengthArrayExtentSyntaxes;
    std::vector<std::string> variableLengthArrayExtentTypeKeys;

    // NativeSharp 2.0 item 269 -- a non-constant array extent is meaningful only on a local
    // declaration: `T[length] values;` reserves raw, contiguous storage in the current stack
    // frame.  Constant extents retain the fixed inline-array semantics implemented by item 267.
    //
    // Internally the local is pointer-shaped because the existing raw-indexing and stackalloc
    // pipeline already define precisely the ABI we need.  The explicit VLA marker kept on the
    // local prevents this representation choice from leaking into the source grammar or from
    // enabling variable extents on fields and callable signatures.
    if (!isVarInferred && variable->typeSyntax() != nullptr)
    {
        const auto& modifiers = variable->typeSyntax()->modifiers();
        if (std::ranges::any_of(modifiers, [](const ast::TypeModifierSyntax& modifier) {
                return modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray;
            }))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                variable->typeSyntax()->range(),
                "a flexible array is incomplete trailing struct storage and cannot be a local variable");
            return;
        }
        std::size_t fixedArrayModifierCount = 0;
        bool hasRuntimeExtent = false;
        for (const ast::TypeModifierSyntax& modifier : modifiers)
        {
            if (modifier.kind != ast::TypeModifierSyntaxKind::Array) continue;
            ++fixedArrayModifierCount;
            if (!ProjectMemberModel::evaluateConstantArrayCount(modifier.arrayCount).has_value())
                hasRuntimeExtent = true;
        }

        if (hasRuntimeExtent)
        {
            const bool supportedShape = fixedArrayModifierCount != 0 &&
                modifiers.size() >= fixedArrayModifierCount &&
                std::all_of(
                    modifiers.end() - static_cast<std::ptrdiff_t>(fixedArrayModifierCount),
                    modifiers.end(),
                    [](const ast::TypeModifierSyntax& modifier)
                    {
                        return modifier.kind == ast::TypeModifierSyntaxKind::Array;
                    });
            if (!supportedShape)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidArrayCount,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->typeSyntax()->range(),
                    "a local variable-length array requires its runtime dimensions to be the "
                    "outermost type modifiers");
            }
            else
            {
                const std::string declaredKey =
                    memberModel_->canonicalTypeKey(variable->typeSyntax(), *nameLookup_);
                std::size_t elementEnd = declaredKey.size();
                bool completeSuffix = true;
                for (std::size_t dimension = 0; dimension < fixedArrayModifierCount; ++dimension)
                {
                    const std::size_t extentStart = declaredKey.rfind('[', elementEnd - 1);
                    if (extentStart == std::string::npos)
                    {
                        completeSuffix = false;
                        break;
                    }
                    elementEnd = extentStart;
                }
                if (!completeSuffix)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        variable->typeSyntax()->range(),
                        "cannot determine the element type of the variable-length array");
                }
                else
                {
                    variableLengthArrayElementTypeKey = declaredKey.substr(0, elementEnd);
                    // Parser stores fixed-array modifiers inside-out so canonical layout is
                    // nested correctly. Reverse them back here: runtime evaluation and row-major
                    // indexing must follow the dimensions as the programmer wrote them.
                    for (auto modifier = modifiers.rbegin(); modifier != modifiers.rend(); ++modifier)
                    {
                        if (modifier->kind != ast::TypeModifierSyntaxKind::Array) break;
                        variableLengthArrayExtentSyntaxes.push_back(modifier->arrayCount);
                    }
                    variableLengthArrayCountSyntax = variableLengthArrayExtentSyntaxes.front();
                    isVariableLengthArray = true;

                    for (const ast::AstNode* extentSyntax : variableLengthArrayExtentSyntaxes)
                    {
                        const ExpressionResult count = analyzeExpression(
                            extentSyntax, methodMemberOrdinal, scopeOrdinal, diagnostics);
                        variableLengthArrayExtentTypeKeys.push_back(count.typeKey);
                        if (!count.valid || !ProjectTypeConversion::isIntegral(count.typeKey) ||
                            count.typeKey == "builtin:bool")
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidArrayCount,
                                diagnostics::DiagnosticSeverity::Error,
                                extentSyntax != nullptr ? extentSyntax->range() : variable->range(),
                                "every variable-length array dimension must have an integral type");
                        }
                    }
                    if (variableLengthArrayElementTypeKey == "builtin:void")
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidStackalloc,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->typeSyntax()->range(),
                            "a variable-length array requires an element type with a size; void has none");
                    }
                    else if (!isErrorType(variableLengthArrayElementTypeKey) &&
                        !elementTypeHasTrivialLifetime(variableLengthArrayElementTypeKey))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidStackalloc,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->typeSyntax()->range(),
                            "a variable-length array requires an element type with no destructor "
                            "and no owned fields; '" + variableLengthArrayElementTypeKey +
                                "' owns a lifetime that raw stack storage cannot discharge");
                    }
                    if (variable->first() != nullptr)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->first()->range(),
                            "a variable-length array declaration cannot have an initializer; "
                            "initialize its elements explicitly");
                    }
                }
            }
        }
    }

    const std::string typeKey = isVarInferred
        ? varInferredTypeKey
        : (isVariableLengthArray
            ? variableLengthArrayElementTypeKey + '*'
            : memberModel_->canonicalTypeKey(variable->typeSyntax(), *nameLookup_));
    const sema::OwnershipKind declaredOwnership = ownershipKindFromSyntax(variable->typeSyntax());
    const bool isBorrowed = declaredOwnership == sema::OwnershipKind::Borrowed;
    // Bloque 1 item 1/6/7 fix -- `owned T x = ...` declares an OWNING local, exactly like
    // `raii T x = ...`. Before this, the `owned` qualifier on a local was computed here (into
    // `declaredOwnership`) and then silently DISCARDED: only the Borrowed case was ever consulted,
    // so an `owned` local parsed, type-checked, compiled and ran while being treated as an
    // ordinary non-owning binding. Found by real execution, not by reading IR
    // (tests/unit/Stage50_11OwnershipExecutionTests.cpp): the object it owned was never destroyed
    // -- a silent leak of a declared language feature -- it could not be used as a `move(...)`
    // source (so the project's own Stage50_1 syntax sample
    // `owned Resource local = new Resource(); return move(local);` parsed but was rejected by
    // semantic analysis), and copying it into a second owner was accepted, which is a
    // double-ownership hole the equivalent `raii` code has always rejected.
    //
    // Routing it through the same `isRaii` flag is what makes all of that correct at once: that one
    // flag is what opens the cleanup region (destruction at scope exit), what admits a local as a
    // move source, what enables the copy-owner rejection, and what drives moved-from tracking.
    //
    // Scope of the `owned` spelling, deliberately narrower than `raii`'s: it makes the local an
    // owner only for the types that actually own a separate allocation -- a class/interface
    // reference or a native string -- and stays the no-op it always was for everything else. That
    // is not a shortcut, it is the SAME rule `owned` PARAMETERS already follow (see
    // requiresOwnedDestruction and emitOwnedParameterDestroy: "a value type owns no separate heap
    // allocation for `owned` to release"). Making the spellings disagree is what would be
    // surprising.
    //
    // It also keeps working code working. `owned T[] scratch = new T[1]` is an established idiom in
    // this project's own library (LinkedList<T>.AddFirst's real shape, covered by
    // Stage52GenericCollectionsCompilerFixesTests fix 10) used purely as a relocation buffer. An
    // array does not currently destroy its elements at all, so treating that local as an owner
    // would either reject the idiom outright or promise a destruction traversal the compiler does
    // not yet perform -- both worse than leaving it exactly as it was.
    const bool isOwnedQualified = declaredOwnership == sema::OwnershipKind::Owned;
    const bool managedDelegate = isAnyDelegateTypeKey(typeKey) && !isExternCDelegateTypeKey(typeKey);
    const bool ownedQualifierOwnsStorage =
        isOwnedQualified && (isReferenceTypeKey(typeKey) || typeKey == "builtin:string" || managedDelegate);
    // A foreach enumerator has an automatic lifetime whose representation follows the inferred
    // GetEnumerator() result. Reference-shaped results retain the established RAII cleanup path;
    // value-shaped results are ordinary stack locals and therefore allocate nothing.
    const bool foreachEnumeratorNeedsRaii = isForeachEnumerator &&
        (isReferenceTypeKey(typeKey) || typeKey == "builtin:string" || managedDelegate);
    const bool isRaii = variable->hasModifier(TokenKind::KwRaii) || ownedQualifierOwnsStorage ||
        foreachEnumeratorNeedsRaii;
    // Wording helper: every owner diagnostic below has to name the spelling the user actually
    // wrote, otherwise an `owned` local reports errors about "raii local" and sends them looking at
    // the wrong keyword.
    const char* const ownerSpelling = ownedQualifierOwnsStorage
        ? "owned local"
        : (isForeachEnumerator ? "foreach enumerator" : "raii local");
    std::optional<MethodLocalBorrowBinding> pendingLocalBorrow;
    MethodEscapeOrigin pendingReferenceEscapeOrigin;
    const bool raiiOwnsNativeArrayBuffer =
        variable->hasModifier(TokenKind::KwRaii) && ProjectTypeConversion::isNativeArray(typeKey);
    if (isReferenceLocal &&
        (variable->flag() || variable->hasModifier(TokenKind::KwRaii) ||
         declaredOwnership != sema::OwnershipKind::None))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "a reference local cannot be const, raii, owned, or borrowed; it aliases mutable storage without ownership");
    }
    if (isReferenceLocal && variable->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "reference local requires an initializer naming mutable addressable storage");
    }
    if (isRaii && !isReferenceTypeKey(typeKey) && typeKey != "builtin:string" && !managedDelegate &&
        !raiiOwnsNativeArrayBuffer)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            std::string(ownerSpelling) +
                " requires a NativeSharp class/interface reference, managed delegate, native string, or native array buffer");
    }
    if (isForeachEnumerator && typeKeyIsStruct(typeKey) &&
        !elementTypeHasTrivialLifetime(typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "foreach struct enumerator '" + typeKey +
                "' must have a trivial lifetime; automatic destruction of value locals is not "
                "yet supported");
    }
    if (isRaii && variable->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            std::string(ownerSpelling) + " requires an initializer: new T(...), null, or move(other)");
    }
    if (isBorrowed && variable->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "borrowed local requires an initializer naming a live owner or borrow");
    }
    sema::OwnershipKind pendingObjectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind pendingStorageOwnership = sema::OwnershipKind::None;
    if (variable->first() != nullptr)
    {
        ExpressionResult initializer;
        if (isVarInferred)
        {
            // Already analyzed above (before typeKey existed, so this couldn't happen in the
            // usual typeKey-first order) -- reuse it rather than analyzing the initializer twice.
            initializer = std::move(*precomputedVarInitializer);
        }
        else if (variable->first()->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            // C7: a braced initializer has no type of its own -- it is filled AGAINST the
            // declared type, exactly like a lambda is bound against a delegate type just below.
            initializer = bindAggregateInitializer(
                variable->first(), typeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        else if (variable->first()->kind() == ast::AstNodeKind::LambdaExpression && isDelegateTypeKey(typeKey))
        {
            initializer = bindLambdaToDelegate(
                variable->first(), typeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        else
        {
            initializer = analyzeExpression(
                variable->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                isDelegateTypeKey(typeKey), false, isRaii);
            if (initializer.methodGroup && isDelegateTypeKey(typeKey))
            {
                initializer = bindMethodGroupToDelegate(
                    variable->first(), initializer, typeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
            }
        }
        if (!isRaii && !isBorrowed && variable->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> sourceName = lookupScopeName(scopeOrdinal, variable->first()->text());
            if (sourceName.has_value() && sourceName->kind == ScopeNameKind::Local &&
                sourceName->semanticOrdinal < locals_.size() && locals_[sourceName->semanticOrdinal].isRaii)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "RAII owner '" + locals_[sourceName->semanticOrdinal].name +
                        "' cannot be copied into a non-owning local; Stage 49 requires ownership transfer with move(...)");
            }
        }
        if (variable->first()->kind() == ast::AstNodeKind::MoveExpression && !isRaii)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "move construction requires a raii destination local");
        }
        // Stage 52: an "owned"/"borrowed"-returning call whose declared return type is a
        // generic T closed over a plain value type (int, bool, a struct, ...) carries an
        // ownership qualifier that is structurally meaningless for that T -- there is nothing
        // to transfer or to keep alive. Requiring a raii/borrowed destination local in that
        // case would make it impossible to write `int x = list.Pop();` for a generic
        // collection. Only enforce the raii-destination / borrowed-local requirement when the
        // local's own declared type actually needs ownership tracking.
        const bool localTypeOwnable = isReferenceTypeKey(typeKey) || typeKey == "builtin:string" || managedDelegate ||
            raiiOwnsNativeArrayBuffer;
        if (!isRaii && initializer.valid && initializer.ownership == sema::OwnershipKind::Owned &&
            initializer.ownershipTransfer && !isBorrowed && localTypeOwnable)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "owned return value cannot initialize a non-owning local; use a raii owner destination");
        }
        const bool ownedReturningInitializer = initializer.valid &&
            initializer.ownership == sema::OwnershipKind::Owned && initializer.ownershipTransfer;
        if (isRaii && variable->first()->kind() != ast::AstNodeKind::ObjectCreationExpression &&
            variable->first()->kind() != ast::AstNodeKind::NewExpression &&
            variable->first()->kind() != ast::AstNodeKind::MoveExpression &&
            variable->first()->kind() != ast::AstNodeKind::NullLiteralExpression &&
            !ownedReturningInitializer && !initializer.ownershipFreshOwner)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "RAII ownership cannot be copied or borrowed; initialize with new, null, move(other), or an owned-returning call");
        }
        if (isReferenceLocal && initializer.valid)
        {
            if (!initializer.addressable || !initializer.assignable)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "reference-local initializer must be a mutable addressable storage location");
            }
            if (initializer.typeKey != typeKey)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "reference-local initializer type '" + initializer.typeKey +
                        "' must exactly match declared type '" + typeKey + "'");
            }
            pendingReferenceEscapeOrigin =
                referenceEscapeOriginOfExpression(variable->first(), scopeOrdinal);
            if (!pendingReferenceEscapeOrigin.maximumValidScopeOrdinal.has_value())
            {
                std::size_t rootScope = scopeOrdinal;
                while (scopes_[rootScope].parentScopeOrdinal.has_value())
                    rootScope = *scopes_[rootScope].parentScopeOrdinal;
                pendingReferenceEscapeOrigin.maximumValidScopeOrdinal = rootScope;
            }
            if (!scopeContains(*pendingReferenceEscapeOrigin.maximumValidScopeOrdinal, scopeOrdinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->range(),
                    "reference local lifetime exceeds the lifetime region of its source storage");
            }
        }
        else if (initializer.valid)
        {
            const ProjectImplicitConversion conversion = classifyImplicit(
                initializer.typeKey,
                typeKey,
                initializer.integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "cannot initialize local '" + variable->text() + "' of type '" + typeKey +
                        "' from '" + initializer.typeKey + "'");
            }
        }

        if (!isBorrowed && !isRaii && initializer.valid &&
            initializer.ownership == sema::OwnershipKind::Borrowed && localTypeOwnable)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "borrowed value cannot be stored in unqualified local '" + variable->text() +
                    "'; declare the local borrowed so its lifetime remains checked");
        }

        if (isBorrowed)
        {
            if (isRaii)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->range(),
                    "a borrowed local cannot also be declared raii");
            }
            else if (variable->first()->kind() != ast::AstNodeKind::IdentifierExpression)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    variable->first()->range(),
                    "borrowed local requires a direct live owner or borrowed identifier source");
            }
            else if (initializer.valid)
            {
                const std::optional<ScopeName> source = lookupScopeName(
                    scopeOrdinal, variable->first()->text());
                if (!source.has_value())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        variable->first()->range(),
                        "borrowed local requires a checked owner or borrowed source");
                }
                else
                {
                    MethodLocalBorrowBinding borrow;
                    borrow.syntax = variable;
                    borrow.methodMemberOrdinal = methodMemberOrdinal;
                    borrow.creationScopeOrdinal = scopeOrdinal;
                    borrow.typeKey = typeKey;

                    if (source->kind == ScopeNameKind::Local && source->semanticOrdinal < locals_.size())
                    {
                        const MethodLocalSymbol& sourceLocal = locals_[source->semanticOrdinal];
                        borrow.sourceLocalOrdinal = source->semanticOrdinal;
                        if (sourceLocal.isRaii)
                        {
                            borrow.sourceKind = MethodLocalBorrowSourceKind::OwnerLocal;
                            borrow.sourceOwnerLocalOrdinal = source->semanticOrdinal;
                            borrow.maximumValidScopeOrdinal = sourceLocal.scopeOrdinal;
                        }
                        else if (sourceLocal.ownership == sema::OwnershipKind::Borrowed &&
                            sourceLocal.localBorrowBindingOrdinal.has_value() &&
                            *sourceLocal.localBorrowBindingOrdinal < localBorrowBindings_.size())
                        {
                            const MethodLocalBorrowBinding& sourceBorrow =
                                localBorrowBindings_[*sourceLocal.localBorrowBindingOrdinal];
                            borrow.sourceKind = MethodLocalBorrowSourceKind::BorrowedLocal;
                            borrow.sourceOwnerLocalOrdinal = sourceBorrow.sourceOwnerLocalOrdinal;
                            borrow.sourceOwnerParameterOrdinal = sourceBorrow.sourceOwnerParameterOrdinal;
                            borrow.maximumValidScopeOrdinal = sourceBorrow.maximumValidScopeOrdinal;
                        }
                        else
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                variable->first()->range(),
                                "borrowed local source '" + sourceLocal.name +
                                    "' is not a RAII owner or borrowed local");
                        }
                    }
                    else if (source->kind == ScopeNameKind::Parameter)
                    {
                        const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
                        if (method != nullptr && source->semanticOrdinal < method->parameters.size())
                        {
                            const MethodParameterSymbol& parameter = method->parameters[source->semanticOrdinal];
                            borrow.sourceParameterOrdinal = source->semanticOrdinal;
                            std::size_t rootScope = scopeOrdinal;
                            while (scopes_[rootScope].parentScopeOrdinal.has_value())
                            {
                                rootScope = *scopes_[rootScope].parentScopeOrdinal;
                            }
                            borrow.maximumValidScopeOrdinal = rootScope;
                            if (parameter.ownership == sema::OwnershipKind::Owned)
                            {
                                borrow.sourceKind = MethodLocalBorrowSourceKind::OwnedParameter;
                                borrow.sourceOwnerParameterOrdinal = source->semanticOrdinal;
                            }
                            else if (parameter.ownership == sema::OwnershipKind::Borrowed)
                            {
                                borrow.sourceKind = MethodLocalBorrowSourceKind::BorrowedParameter;
                                borrow.sourceOwnerParameterOrdinal = source->semanticOrdinal;
                            }
                            else
                            {
                                diagnostics.report(
                                    diagnostics::DiagnosticId::InvalidOperand,
                                    diagnostics::DiagnosticSeverity::Error,
                                    variable->first()->range(),
                                    "borrowed local source parameter '" + parameter.name +
                                        "' must be declared owned or borrowed");
                            }
                        }
                    }

                    const bool hasRootOwner = borrow.sourceOwnerLocalOrdinal.has_value() ||
                        borrow.sourceOwnerParameterOrdinal.has_value();
                    if (hasRootOwner &&
                        scopeContains(borrow.maximumValidScopeOrdinal, borrow.creationScopeOrdinal))
                    {
                        pendingLocalBorrow = std::move(borrow);
                    }
                    else if (hasRootOwner)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            variable->range(),
                            "borrowed local lifetime exceeds the lifetime region of its source owner");
                    }
                }
            }
        }
        pendingObjectLifetimeOwnership = initializer.objectLifetimeOwnership;
        pendingStorageOwnership = initializer.storageOwnership;
    }

    const std::optional<ScopeName> sameScope = lookupScopeNameInScope(scopeOrdinal, variable->text());
    if (sameScope.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            "duplicate local/parameter name '" + variable->text() + "' in the same method scope");
        reportPrevious(sameScope->range, diagnostics, "previous declaration is here");
        return;
    }

    MethodLocalSymbol local;
    local.methodMemberOrdinal = methodMemberOrdinal;
    local.scopeOrdinal = scopeOrdinal;
    local.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method != nullptr)
    {
        local.fileId = method->fileId;
    }
    local.range = variable->range();
    local.syntax = variable;
    local.name = variable->text();
    local.typeKey = typeKey;
    local.isReference = isReferenceLocal;
    local.referenceEscapeOrigin = pendingReferenceEscapeOrigin;
    if (isReferenceLocal) local.referenceLifetimeScopeOrdinal = scopeOrdinal;
    local.isVariableLengthArray = isVariableLengthArray;
    local.variableLengthArrayElementTypeKey = variableLengthArrayElementTypeKey;
    local.variableLengthArrayCountSyntax = variableLengthArrayCountSyntax;
    local.variableLengthArrayExtentSyntaxes = variableLengthArrayExtentSyntaxes;
    local.variableLengthArrayExtentTypeKeys = variableLengthArrayExtentTypeKeys;
    if (!isVariableLengthArray && variable->first() != nullptr && typeKey.ends_with("*"))
    {
        local.variableLengthArrayRootLocalOrdinal =
            variableArrayRootLocalOfExpression(variable->first(), scopeOrdinal);
    }
    local.isConst = variable->flag();
    // El valor de un `const` entero, plegado aqui. Ver el comentario del campo: sirve para
    // contestar "¿el divisor es cero?" y para nada mas.
    if (local.isConst && variable->first() != nullptr &&
        ProjectIntegerEvaluator::isIntegerType(typeKey))
    {
        const std::optional<ProjectIntegerValue> folded =
            foldIntegerConstant(variable->first(), std::nullopt);
        if (folded.has_value())
        {
            local.constantIntegerValue =
                static_cast<std::int64_t>(ProjectIntegerEvaluator::normalize(folded->bits, typeKey));
        }
    }
    local.isRaii = isRaii;
    local.isPinned = isPinnedLocal;
    local.ownership = isBorrowed ? sema::OwnershipKind::Borrowed : sema::OwnershipKind::None;
    local.objectLifetimeOwnership = pendingObjectLifetimeOwnership;
    local.storageOwnership = pendingStorageOwnership;
    if (pendingLocalBorrow.has_value())
    {
        local.localBorrowBindingOrdinal = localBorrowBindings_.size();
    }
    local.lambdaOrdinal = currentLambdaOrdinal_;
    // SHR-013: de que local presta esta vista, si presta de alguno. Se graba AQUI, al declarar,
    // y no se recalcula al usarla: la expresion que la creo es la unica que lo sabe, y para
    // cuando alguien la devuelve ya no esta a la vista.
    if (typeKeyIsViewLike(typeKey))
    {
        const std::optional<std::string> root =
            viewRootLocalOfExpression(variable->first(), scopeOrdinal);
        if (root.has_value()) local.viewRootLocalName = *root;
    }

    const std::optional<ScopeName> outer = lookupScopeName(scopeOrdinal, variable->text());
    if (outer.has_value())
    {
        MethodShadowingKind kind = MethodShadowingKind::LocalShadowsLocal;
        if (outer->kind == ScopeNameKind::Parameter)
        {
            kind = MethodShadowingKind::LocalShadowsParameter;
        }
        shadowingEvents_.push_back(MethodShadowingEvent{
            kind,
            methodMemberOrdinal,
            scopeOrdinal,
            variable->text(),
            variable->range()});
    }
    else if (!memberCandidates(methodMemberOrdinal, variable->text()).empty())
    {
        shadowingEvents_.push_back(MethodShadowingEvent{
            MethodShadowingKind::LocalShadowsMember,
            methodMemberOrdinal,
            scopeOrdinal,
            variable->text(),
            variable->range()});
    }

    const std::size_t localOrdinal = locals_.size();
    locals_.push_back(std::move(local));
    if (isVariableLengthArray)
        locals_[localOrdinal].variableLengthArrayRootLocalOrdinal = localOrdinal;
    scopes_[scopeOrdinal].localOrdinals.push_back(localOrdinal);
    scopeNames_[scopeOrdinal].push_back(ScopeName{
        ScopeNameKind::Local,
        variable->text(),
        localOrdinal,
        variable->range()});

    // Item 270: multidimensional extents are values, not aliases for their source expressions.
    // Snapshot them in hidden nuint locals in source order so side effects happen once and later
    // assignments to a variable used as an extent cannot silently reshape the array's strides.
    if (isVariableLengthArray && variableLengthArrayExtentSyntaxes.size() > 1)
    {
        for (std::size_t dimension = 0;
             dimension < variableLengthArrayExtentSyntaxes.size(); ++dimension)
        {
            MethodLocalSymbol extentLocal;
            extentLocal.methodMemberOrdinal = methodMemberOrdinal;
            extentLocal.scopeOrdinal = scopeOrdinal;
            extentLocal.declarationOrdinal = scopes_[scopeOrdinal].localOrdinals.size();
            extentLocal.fileId = locals_[localOrdinal].fileId;
            extentLocal.range = variableLengthArrayExtentSyntaxes[dimension] != nullptr
                ? variableLengthArrayExtentSyntaxes[dimension]->range() : variable->range();
            extentLocal.syntax = variableLengthArrayExtentSyntaxes[dimension];
            extentLocal.name = "__vla_extent_" + std::to_string(localOrdinal) + "_" +
                std::to_string(dimension);
            extentLocal.typeKey = dimension < variableLengthArrayExtentTypeKeys.size()
                ? variableLengthArrayExtentTypeKeys[dimension] : "builtin:nuint";
            extentLocal.lambdaOrdinal = currentLambdaOrdinal_;
            const std::size_t extentOrdinal = locals_.size();
            locals_.push_back(std::move(extentLocal));
            scopes_[scopeOrdinal].localOrdinals.push_back(extentOrdinal);
            assignedLocals_.insert(extentOrdinal);
            locals_[localOrdinal].variableLengthArrayExtentLocalOrdinals.push_back(extentOrdinal);
        }
    }
    // C7 "definite assignment": a declaration with an initializer assigns the local, full stop.
    // Without one, only the types the analysis does not track (aggregates, which the backend
    // zeroes) start out assigned; everything else must be written before it is read. A local
    // re-entering scope on a later loop iteration is re-declared through this same path, so its
    // assigned state is correctly reset rather than inherited from the previous iteration.
    if (variable->first() != nullptr || isVariableLengthArray ||
        !definiteAssignmentTracksLocal(localOrdinal))
    {
        assignedLocals_.insert(localOrdinal);
    }
    else
    {
        assignedLocals_.erase(localOrdinal);
    }
    if (pendingLocalBorrow.has_value())
    {
        pendingLocalBorrow->localOrdinal = localOrdinal;
        localBorrowBindings_.push_back(std::move(*pendingLocalBorrow));
    }
    if (isRaii)
    {
        const std::size_t cleanupOrdinal = createCleanupRegion(
            MethodCleanupRegionKind::RaiiDestroy,
            variable,
            variable,
            methodMemberOrdinal,
            scopeOrdinal);
        cleanupRegions_[cleanupOrdinal].resourceLocalOrdinal = localOrdinal;
        activeCleanupRegions_.push_back(cleanupOrdinal);
        movedRaiiLocals_.erase(localOrdinal);
        maybeMovedRaiiLocals_.erase(localOrdinal);
    }
}

void ProjectMethodBodyModel::analyzeFor(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t forScope = createScope(
        MethodScopeKind::For,
        methodMemberOrdinal,
        scopeOrdinal);
    const std::size_t breakCleanupDepth = activeCleanupRegions_.size();

    if (statement->first() != nullptr)
    {
        if (statement->first()->kind() == ast::AstNodeKind::VariableDeclarationStatement)
        {
            analyzeVariable(statement->first(), methodMemberOrdinal, forScope, diagnostics);
        }
        else if (statement->first()->kind() == ast::AstNodeKind::BlockStatement)
        {
            for (const ast::AstNode* expressionStatement : statement->first()->children())
            {
                if (expressionStatement != nullptr && expressionStatement->first() != nullptr)
                    analyzeExpression(
                        expressionStatement->first(), methodMemberOrdinal, forScope, diagnostics);
            }
        }
        else
        {
            analyzeExpression(statement->first(), methodMemberOrdinal, forScope, diagnostics);
        }
    }

    if (statement->second() != nullptr)
    {
        const ExpressionResult condition = analyzeExpression(
            statement->second(), methodMemberOrdinal, forScope, diagnostics);
        if (condition.valid && condition.typeKey != "builtin:bool")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCondition,
                diagnostics::DiagnosticSeverity::Error,
                statement->second()->range(),
                "for condition must have type bool");
        }
    }

    const OwnershipFlowSnapshot loopEntry = captureOwnershipFlow();
    const std::size_t localCountAtBodyEntry = locals_.size();
    const ClassMemberSymbol* loopMethod = methodSymbol(methodMemberOrdinal);
    const std::size_t continueCleanupDepth = activeCleanupRegions_.size();
    loopStack_.push_back(LoopContext{
        forScope,
        breakCleanupDepth,
        continueCleanupDepth,
        {},
        {}});

    if (!statement->children().empty())
    {
        analyzeBlock(
            statement->children().front(),
            methodMemberOrdinal,
            forScope,
            true,
            diagnostics);
    }
    const OwnershipFlowSnapshot bodyFallthroughState = captureOwnershipFlow();
    LoopContext loopContext = std::move(loopStack_.back());
    loopStack_.pop_back();

    std::vector<OwnershipFlowSnapshot> updateEntryStates = std::move(loopContext.continueStates);
    if (bodyFallthroughState.reachable)
    {
        updateEntryStates.push_back(bodyFallthroughState);
    }
    OwnershipFlowSnapshot noUpdateEntry;
    noUpdateEntry.reachable = false;
    OwnershipFlowSnapshot backEdgeState = joinOwnershipFlows(noUpdateEntry, updateEntryStates);

    if (backEdgeState.reachable && statement->third() != nullptr)
    {
        restoreOwnershipFlow(backEdgeState);
        if (statement->third()->kind() == ast::AstNodeKind::BlockStatement)
        {
            for (const ast::AstNode* expressionStatement : statement->third()->children())
            {
                if (expressionStatement != nullptr && expressionStatement->first() != nullptr)
                    analyzeExpression(
                        expressionStatement->first(), methodMemberOrdinal, forScope, diagnostics);
            }
        }
        else
        {
            analyzeExpression(statement->third(), methodMemberOrdinal, forScope, diagnostics);
        }
        backEdgeState = captureOwnershipFlow();
    }

    if (backEdgeState.reachable)
    {
        for (std::size_t localOrdinal = 0; localOrdinal < localCountAtBodyEntry; ++localOrdinal)
        {
            if (localOrdinal >= locals_.size() || !locals_[localOrdinal].isRaii)
            {
                continue;
            }
            const bool aliveAtEntry = !loopEntry.movedRaiiLocals.contains(localOrdinal) &&
                !loopEntry.maybeMovedRaiiLocals.contains(localOrdinal);
            const bool movedAtBackEdge = backEdgeState.movedRaiiLocals.contains(localOrdinal) ||
                backEdgeState.maybeMovedRaiiLocals.contains(localOrdinal);
            if (aliveAtEntry && movedAtBackEdge)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->range(),
                    "RAII owner '" + locals_[localOrdinal].name +
                        "' may remain moved across a for-loop back-edge; reinitialize it before the next iteration");
            }
        }
        if (loopMethod != nullptr)
        {
            for (std::size_t parameterOrdinal = 0;
                 parameterOrdinal < loopMethod->parameters.size(); ++parameterOrdinal)
            {
                if (loopMethod->parameters[parameterOrdinal].ownership != sema::OwnershipKind::Owned)
                {
                    continue;
                }
                const bool aliveAtEntry = !loopEntry.movedOwnedParameters.contains(parameterOrdinal) &&
                    !loopEntry.maybeMovedOwnedParameters.contains(parameterOrdinal);
                const bool movedAtBackEdge = backEdgeState.movedOwnedParameters.contains(parameterOrdinal) ||
                    backEdgeState.maybeMovedOwnedParameters.contains(parameterOrdinal);
                if (aliveAtEntry && movedAtBackEdge)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->range(),
                        "owned parameter '" + loopMethod->parameters[parameterOrdinal].name +
                            "' may remain moved across a for-loop back-edge; transfer it only on an exiting path");
                }
            }
        }
    }

    OwnershipFlowSnapshot postLoop = loopEntry;
    postLoop = joinOwnershipFlows(postLoop, loopContext.breakStates);
    if (backEdgeState.reachable)
    {
        postLoop = joinOwnershipFlow(postLoop, backEdgeState);
    }
    restoreOwnershipFlow(postLoop);

    for (std::size_t index = activeCleanupRegions_.size(); index > breakCleanupDepth; --index)
    {
        recordNormalCleanupContinuation(
            activeCleanupRegions_[index - 1], statement, methodMemberOrdinal, forScope);
    }
    activeCleanupRegions_.resize(breakCleanupDepth);
}

// ---------------------------------------------------------------------------------------------
// analyzeForeach -- semantic resolution of the array-vs-enumerator-pattern desugaring.
//
// Parser::parseForeachStatement() cannot know the collection's type (parsing is a purely
// syntactic pass), so it builds BOTH possible desugarings unconditionally and stores them on the
// ForeachStatement node:
//   first()  -- `var $__foreach_source = <collection>;` -- evaluates the collection expression
//                exactly once, regardless of which candidate is chosen below. Whether this ends
//                up `raii` is decided in this function (P0.2, see below), not by the parser.
//   second() -- the array-mode candidate: an index/`.Length` while-loop.
//   third()  -- the enumerator-mode candidate: a `GetEnumerator()`/`MoveNext()`/`Current`
//                while-loop, with the enumerator held in a `raii var` local so it is destroyed
//                automatically on every exit path (break/return/exception) -- NativeSharp's
//                RAII machinery standing in for C#'s implicit `Dispose()` call in foreach's
//                generated `finally` block. `Current` is read as a plain property (P1.1), not a
//                method call.
//   children()[0] -- `collectionProbe`, a throwaway second parse of the collection expression,
//                used below (P0.2) to decide first()'s raii-ness before it is analyzed for real.
//
// This function is the ONE place that decides which candidate is "real": it analyzes first() to
// resolve $__foreach_source's typeKey (reusing analyzeVariable exactly as any other `var` local
// would), then dispatches on that typeKey (array types are `<element>[]`, see
// ProjectGenericModel::canonicalTypeKey) to analyze ONLY the matching candidate via analyzeBlock.
// The other candidate's AST subtree is left completely unanalyzed -- no locals are registered for
// it, no diagnostics are produced from it, and it never reaches HIR/MIR/LLVM. It is simply dead
// arena memory, reclaimed with everything else when the compilation unit's AstArena is destroyed.
//
// No new control-flow analysis is written here on purpose: both candidates are built entirely out
// of ordinary VariableDeclarationStatement/WhileStatement nodes, so delegating to analyzeBlock
// means the WhileStatement case in analyzeStatement (loopStack_ push/pop, RAII-across-back-edge
// checking, ownership-flow join, break/continue support) runs completely unmodified. foreach gets
// full loop semantics for free, with zero duplicated logic -- exactly like the HIR/MIR/LLVM
// layers below it, which likewise need no foreach-specific lowering code at all.
void ProjectMethodBodyModel::analyzeForeach(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t foreachScope = createScope(
        MethodScopeKind::Foreach,
        methodMemberOrdinal,
        scopeOrdinal);

    // Captured BEFORE $__foreach_source is declared below, exactly like analyzeFor captures its
    // own breakCleanupDepth before analyzing its init statement -- see the teardown block at the
    // end of this function for why this is needed (P0.2 fix, see comment there for the full
    // story: $__foreach_source can now become `raii`, and its cleanup region must be popped by
    // THIS function, the same way analyzeFor pops its own init-scope's cleanup regions, since
    // nothing else ever will).
    const std::size_t foreachCleanupDepth = activeCleanupRegions_.size();

    if (statement->first() == nullptr)
    {
        return;
    }

    // P0.2 of the closure review (STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md): decide
    // whether $__foreach_source needs to be `raii` BEFORE analyzing it for real below.
    // Parser::parseForeachStatement always builds sourceDecl as a plain, non-raii `var` local,
    // because at parse time the collection expression's ownership shape is unknown. But if the
    // collection expression is itself owned-returning (e.g. `foreach (var x in
    // factory.CreateList())` where CreateList() returns `owned List<T>`), analyzing it into a
    // non-raii local would hit the ordinary "owned return value cannot initialize a non-owning
    // local" diagnostic (see analyzeVariable, ~line 1738) -- exactly the P0.2 bug this probe
    // exists to avoid.
    //
    // The probe: Parser attached a throwaway, structurally-identical copy of the collection
    // expression (`collectionProbe`, parsed a second time from the same tokens) as this
    // ForeachStatement node's own extra child. Analyzing THAT copy -- rather than analyzing
    // statement->first()'s own initializer expression twice -- avoids double-registering the
    // real node's bindings in nameBindings_/locals_/etc (analyzeExpression has real side
    // effects beyond its return value). The probe's own diagnostics are deliberately discarded
    // into a scratch DiagnosticEngine: this is not real analysis, only its ExpressionResult
    // (ownership / ownershipTransfer / typeKey) is consulted, and the SAME expression is about
    // to be analyzed again for real via analyzeVariable below, whose diagnostics (if any) are
    // the ones that actually reach the user.
    if (!statement->children().empty())
    {
        ast::AstNode* collectionProbe = statement->children()[0];
        diagnostics::DiagnosticEngine scratchDiagnostics;
        const ExpressionResult probe = analyzeExpression(
            collectionProbe, methodMemberOrdinal, foreachScope, scratchDiagnostics);

        // Arrays can never be raii (analyzeVariable rejects `raii` on anything but a class/
        // interface reference type or native string), so an owned-returning expression of
        // array type must stay a plain `var` -- it simply forgoes the extra owned-destructor
        // NativeSharp would otherwise emit for a raii owner, same as any other owned array
        // temporary elsewhere in this compiler (arrays are not RAII-tracked). Only
        // reference-like/class-shaped or string owned returns get the `raii` treatment here,
        // mirroring the exact `localTypeOwnable` condition analyzeVariable itself uses.
        const bool probeTypeOwnable = probe.valid &&
            (isReferenceTypeKey(probe.typeKey) || probe.typeKey == "builtin:string");
        if (probe.valid && probe.ownership == sema::OwnershipKind::Owned &&
            probe.ownershipTransfer && probeTypeOwnable)
        {
            statement->first()->addModifier(TokenKind::KwRaii);
        }
    }

    // Resolve __foreach_source's type by analyzing its declaration exactly like any other `var`
    // local. On success this appends exactly one MethodLocalSymbol to locals_ (see
    // analyzeVariable); on failure (e.g. the collection expression itself doesn't type-check) it
    // reports its own diagnostic and appends nothing, and there is no reliable typeKey left to
    // dispatch on -- bail out rather than guess which candidate to analyze.
    const std::size_t localCountBeforeSource = locals_.size();
    analyzeVariable(statement->first(), methodMemberOrdinal, foreachScope, diagnostics);
    if (locals_.size() != localCountBeforeSource + 1)
    {
        return;
    }

    const std::string sourceTypeKey = locals_.back().typeKey;
    const bool isArray = sourceTypeKey.size() > 2 &&
        sourceTypeKey.compare(sourceTypeKey.size() - 2, 2, "[]") == 0;

    ast::AstNode* chosenCandidate = const_cast<ast::AstNode*>(isArray ? statement->second() : statement->third());
    if (chosenCandidate != nullptr)
    {
        // Everything past this point is ordinary VariableDeclarationStatement/WhileStatement
        // analysis -- including, for the enumerator-mode candidate, the
        // `GetEnumerator()`/`MoveNext()`/`Current` member lookups. Those go through the SAME
        // accessible-member resolution as any other method call/property access, so a collection
        // missing one of these members (or having them with the wrong signature/shape) is
        // reported by the ordinary "no accessible member" / argument-mismatch diagnostics --
        // foreach needs no bespoke "does this type support foreach" pre-check.
        analyzeBlock(chosenCandidate, methodMemberOrdinal, foreachScope, true, diagnostics);
    }

    // Teardown for $__foreach_source's OWN cleanup region, mirroring analyzeFor's end-of-forScope
    // teardown (see the last lines of analyzeFor above). This is genuinely new: before the P0.2
    // fix, $__foreach_source was never `raii`, so it never created a cleanup region and this
    // function never needed to pop one -- the gap was latent, not exercised by any existing test.
    // Now that $__foreach_source CAN be raii (an owned-returning collection source), its cleanup
    // region -- pushed by analyzeVariable above as if it were any other raii local -- must be
    // popped from activeCleanupRegions_ here, once, after the whole foreach statement (source
    // declaration AND whichever candidate was chosen) has finished analysis. Without this, the
    // region stays permanently "active" in the semantic model's bookkeeping, and the NEXT
    // structured exit analyzed anywhere later in the method (a `return`, an enclosing loop's
    // `break`/`continue`) would compute a cleanup path that includes an ordinal LLVM lowering
    // never actually enters/exits for real (foreach's own scope was never wired to do so) --
    // surfacing as "Stage 45.10 structured exit references an inactive cleanup region" in
    // ProjectOopLlvmLowerer, discovered while writing the regression test for this exact fix.
    for (std::size_t index = activeCleanupRegions_.size(); index > foreachCleanupDepth; --index)
    {
        recordNormalCleanupContinuation(
            activeCleanupRegions_[index - 1], statement, methodMemberOrdinal, foreachScope);
    }
    activeCleanupRegions_.resize(foreachCleanupDepth);
}

void ProjectMethodBodyModel::analyzeReturn(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr)
    {
        return;
    }

    if (currentLambdaOrdinal_.has_value())
    {
        if (statement->isReference())
        {
            if (statement->first() != nullptr)
                analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "lambda ref returns are not part of the item 313 callable contract");
            return;
        }
        const std::string expected = currentLambdaReturnTypeKey_;
        if (expected == "builtin:void")
        {
            if (statement->first() != nullptr)
            {
                analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn, diagnostics::DiagnosticSeverity::Error,
                    statement->range(), "void lambda cannot return a value");
            }
            return;
        }
        if (statement->first() == nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn, diagnostics::DiagnosticSeverity::Error,
                statement->range(), "non-void lambda must return a value");
            return;
        }
        if (statement->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> returnedName = lookupScopeName(scopeOrdinal, statement->first()->text());
            if (returnedName.has_value() && returnedName->kind == ScopeNameKind::Local &&
                returnedName->semanticOrdinal < locals_.size() && locals_[returnedName->semanticOrdinal].isRaii)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "returning RAII owner '" + locals_[returnedName->semanticOrdinal].name +
                        "' requires return move(owner) to transfer ownership");
            }
        }
        ExpressionResult value;
        if (statement->first()->kind() == ast::AstNodeKind::LambdaExpression && isDelegateTypeKey(expected))
            value = bindLambdaToDelegate(statement->first(), expected, methodMemberOrdinal, scopeOrdinal, diagnostics);
        else
            value = analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                isDelegateTypeKey(expected), false, true);
        if (value.methodGroup && isDelegateTypeKey(expected))
            value = bindMethodGroupToDelegate(statement->first(), value, expected, methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (value.valid && !classifyImplicit(value.typeKey, expected, value.integerConstant).exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion, diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(), "cannot convert lambda return expression from '" + value.typeKey +
                    "' to '" + expected + "'");
        }
        return;
    }

    if (!finallyStack_.empty())
    {
        if (statement->first() != nullptr)
        {
            analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "return cannot leave a finally block");
        return;
    }

    recordStructuredExit(
        MethodStructuredExitKind::Return,
        statement,
        methodMemberOrdinal,
        scopeOrdinal,
        std::nullopt,
        0);

    if (method->kind == ClassMemberKind::Constructor || method->kind == ClassMemberKind::Destructor)
    {
        if (statement->first() != nullptr)
        {
            analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                method->kind == ClassMemberKind::Constructor
                    ? "constructor cannot return a value"
                    : "destructor cannot return a value");
        }
        return;
    }

    if (method->declaredTypeKey == "builtin:void")
    {
        if (statement->first() != nullptr)
        {
            analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "void instance method cannot return a value");
        }
        return;
    }

    if (statement->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "non-void instance method must return a value");
        return;
    }

    if (statement->isReference() && !method->isReferenceReturn)
    {
        analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "return ref is valid only inside a ref-returning method");
        return;
    }

    if (method->isReferenceReturn)
    {
        const ExpressionResult value = analyzeExpression(
            statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!statement->isReference())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->range(),
                "ref-returning method must return an alias with 'return ref expression;'");
            return;
        }
        if (!value.valid) return;
        if (value.typeKey != method->declaredTypeKey)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "ref return requires exact type identity; expression has '" + value.typeKey +
                    "' but method returns ref '" + method->declaredTypeKey + "'");
            return;
        }
        if (!value.addressable || !value.assignable)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "ref return requires a mutable addressable storage location");
            return;
        }

        const MethodEscapeOrigin escape =
            referenceEscapeOriginOfExpression(statement->first(), scopeOrdinal);
        if (escape.kind == MethodEscapeOriginKind::CurrentFrameLocal ||
            escape.kind == MethodEscapeOriginKind::VariableLengthArray)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "ref return cannot expose storage owned by the current stack frame");
            return;
        }
        if (escape.kind == MethodEscapeOriginKind::CurrentFrameParameter)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "ref return cannot expose a by-value or readonly parameter");
            return;
        }
        return;
    }

    if (const std::optional<std::size_t> vlaRoot =
            method->declaredTypeKey.ends_with("*")
                ? variableArrayRootLocalOfExpression(statement->first(), scopeOrdinal)
                : std::nullopt;
        vlaRoot.has_value())
    {
        const std::string name = *vlaRoot < locals_.size()
            ? locals_[*vlaRoot].name : std::string("<variable-length-array>");
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->first()->range(),
            "variable-length array storage rooted at '" + name +
                "' cannot be returned; its stack frame ends with this call");
        return;
    }

    bool directOwnerReturnWithoutMove = false;
    if (method->declaredOwnership != sema::OwnershipKind::Borrowed &&
        statement->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> returnedName = lookupScopeName(scopeOrdinal, statement->first()->text());
        if (returnedName.has_value())
        {
            if (returnedName->kind == ScopeNameKind::Local &&
                returnedName->semanticOrdinal < locals_.size() && locals_[returnedName->semanticOrdinal].isRaii)
            {
                directOwnerReturnWithoutMove = true;
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "returning RAII owner '" + locals_[returnedName->semanticOrdinal].name +
                        "' requires return move(owner) to transfer ownership");
            }
            else if (returnedName->kind == ScopeNameKind::Parameter &&
                returnedName->semanticOrdinal < method->parameters.size() &&
                method->parameters[returnedName->semanticOrdinal].ownership == sema::OwnershipKind::Owned &&
                // Stage 52: an "owned T" parameter closed over a value type never needed
                // move(...) in the first place -- see the returnTypeOwnable rationale below.
                (isReferenceTypeKey(method->parameters[returnedName->semanticOrdinal].typeKey) ||
                 method->parameters[returnedName->semanticOrdinal].typeKey == "builtin:string"))
            {
                directOwnerReturnWithoutMove = true;
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "returning owned parameter '" + method->parameters[returnedName->semanticOrdinal].name +
                        "' requires return move(owner) to transfer ownership");
            }
        }
    }
    ExpressionResult value;
    if (statement->first()->kind() == ast::AstNodeKind::LambdaExpression && isDelegateTypeKey(method->declaredTypeKey))
        value = bindLambdaToDelegate(
            statement->first(), method->declaredTypeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
    else
        value = analyzeExpression(
            statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
            isDelegateTypeKey(method->declaredTypeKey), false, true);
    if (value.methodGroup && isDelegateTypeKey(method->declaredTypeKey))
    {
        value = bindMethodGroupToDelegate(
            statement->first(), value, method->declaredTypeKey,
            methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    if (!value.valid)
    {
        return;
    }

    const ProjectImplicitConversion conversion = classifyImplicit(
        value.typeKey,
        method->declaredTypeKey,
        value.integerConstant);
    if (!conversion.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            statement->first()->range(),
            "cannot convert return expression from '" + value.typeKey + "' to '" +
                method->declaredTypeKey + "'");
        return;
    }

    // Stage 52: a generic method's declared "owned T"/"borrowed T" return type may monomorphize
    // to a plain value type (int, bool, a struct, ...) for a given closed T. Ownership/borrow
    // provenance tracking is meaningless there -- a value has no lifetime to escape and nothing
    // to transfer -- so the strict transfer/borrow-provenance checks below only apply when the
    // method's ACTUAL closed return type needs ownership tracking. The bookkeeping past each
    // check (registering a forwarding borrow/transfer binding) still runs unconditionally; it
    // is inert for a value-typed return since there is no RAII cleanup to suppress.
    const bool returnTypeOwnable = typeKeyIsOwnable(method->declaredTypeKey);

    // SHR-013 "vistas y tiempo de vida" -- EL CRITERIO DEL BLOQUE 3, y esta es su linea.
    //
    // Devolver una vista enraizada en un local del propio metodo es un uso-despues-de-liberar
    // con la cara de un valor normal: el struct viaja por valor, el puntero que lleva dentro
    // apunta a memoria que acaba de morir, y nada en el tipo de retorno lo delata.
    //
    // Por que NO lo atrapaba la comprobacion de prestamo que ya existia (Stage 50.7): esa mira
    // el CALIFICATIVO `borrowed` sobre un tipo por referencia, y `returnTypeOwnable` de arriba
    // la apaga para cualquier struct por valor. Una vista es un struct por valor. El hueco era
    // exactamente ese, y no se ve leyendo la regla vieja: se ve preguntandole por un tipo nuevo
    // que hasta el Bloque 3 no existia.
    if (typeKeyIsViewLike(method->declaredTypeKey))
    {
        const std::optional<std::string> root =
            viewRootLocalOfExpression(statement->first(), scopeOrdinal);
        if (root.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                "view of type '" + method->declaredTypeKey + "' borrows from local '" + *root +
                    "' and cannot be returned: the local dies when the method returns. Take the "
                    "view from a parameter, a field, or return a copy that owns its storage");
            return;
        }
    }
    if (method->declaredOwnership == sema::OwnershipKind::Borrowed)
    {
        const bool nullBorrow = statement->first()->kind() == ast::AstNodeKind::NullLiteralExpression;
        if (!nullBorrow && value.ownership != sema::OwnershipKind::Borrowed && returnTypeOwnable)
        {
            std::string message = "borrowed return requires a borrowed value whose lifetime outlives the call";
            if (value.ownershipSourceLocalOrdinal.has_value() &&
                *value.ownershipSourceLocalOrdinal < locals_.size())
            {
                message = "borrowed return value outlives local owner '" +
                    locals_[*value.ownershipSourceLocalOrdinal].name + "'";
            }
            else if (value.ownershipSourceParameterOrdinal.has_value() &&
                *value.ownershipSourceParameterOrdinal < method->parameters.size())
            {
                message = "borrowed return value refers to owned parameter '" +
                    method->parameters[*value.ownershipSourceParameterOrdinal].name +
                    "' whose lifetime ends when the method returns";
            }
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                statement->first()->range(),
                std::move(message));
            return;
        }

        if (!nullBorrow)
        {
            MethodBorrowBinding borrow;
            borrow.syntax = statement->first();
            borrow.methodMemberOrdinal = methodMemberOrdinal;
            borrow.scopeOrdinal = scopeOrdinal;
            borrow.targetMemberOrdinal = methodMemberOrdinal;
            borrow.typeKey = method->declaredTypeKey;

            if (value.borrowedLocalOrdinal.has_value() &&
                *value.borrowedLocalOrdinal < locals_.size())
            {
                const MethodLocalSymbol& local = locals_[*value.borrowedLocalOrdinal];
                if (!local.localBorrowBindingOrdinal.has_value() ||
                    *local.localBorrowBindingOrdinal >= localBorrowBindings_.size())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidReturn,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->first()->range(),
                        "borrowed return value has no valid lifetime provenance");
                    return;
                }

                const MethodLocalBorrowBinding& localBorrow =
                    localBorrowBindings_[*local.localBorrowBindingOrdinal];
                if (localBorrow.sourceOwnerLocalOrdinal.has_value())
                {
                    const std::size_t ownerOrdinal = *localBorrow.sourceOwnerLocalOrdinal;
                    const std::string ownerName = ownerOrdinal < locals_.size()
                        ? locals_[ownerOrdinal].name : std::string("<local-owner>");
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidReturn,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->first()->range(),
                        "borrowed return value '" + local.name +
                            "' outlives its owner '" + ownerName + "'");
                    return;
                }
                if (localBorrow.sourceOwnerParameterOrdinal.has_value())
                {
                    const std::size_t parameterOrdinal = *localBorrow.sourceOwnerParameterOrdinal;
                    if (parameterOrdinal >= method->parameters.size() ||
                        method->parameters[parameterOrdinal].ownership != sema::OwnershipKind::Borrowed)
                    {
                        const std::string ownerName = parameterOrdinal < method->parameters.size()
                            ? method->parameters[parameterOrdinal].name : std::string("<owned-parameter>");
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidReturn,
                            diagnostics::DiagnosticSeverity::Error,
                            statement->first()->range(),
                            "borrowed return value '" + local.name +
                                "' refers to owned parameter '" + ownerName +
                                "' whose lifetime ends when the method returns");
                        return;
                    }
                    borrow.sourceParameterOrdinal = parameterOrdinal;
                }
                borrow.sourceLocalOrdinal = *value.borrowedLocalOrdinal;
                borrow.kind = MethodBorrowKind::BorrowedLocalToBorrowedReturn;
            }
            else if (value.ownershipSourceParameterOrdinal.has_value())
            {
                const std::size_t parameterOrdinal = *value.ownershipSourceParameterOrdinal;
                if (parameterOrdinal >= method->parameters.size() ||
                    method->parameters[parameterOrdinal].ownership != sema::OwnershipKind::Borrowed)
                {
                    const std::string ownerName = parameterOrdinal < method->parameters.size()
                        ? method->parameters[parameterOrdinal].name : std::string("<owned-parameter>");
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidReturn,
                        diagnostics::DiagnosticSeverity::Error,
                        statement->first()->range(),
                        "borrowed return value refers to owned parameter '" + ownerName +
                            "' whose lifetime ends when the method returns");
                    return;
                }
                borrow.sourceParameterOrdinal = parameterOrdinal;
                borrow.kind = MethodBorrowKind::BorrowedParameterToBorrowedReturn;
            }
            else
            {
                // A borrowed-returning call is itself a lifetime contract supplied by the callee.
                // Forwarding it preserves that contract without manufacturing a new owner.
                borrow.kind = MethodBorrowKind::BorrowedReturnForward;
            }
            borrowBindings_.push_back(std::move(borrow));
        }
        return;
    }

    if (method->declaredOwnership == sema::OwnershipKind::None &&
        value.ownership == sema::OwnershipKind::Borrowed)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            statement->first()->range(),
            "borrowed value cannot escape through an unqualified return type; declare the return type borrowed");
        return;
    }

    if (method->declaredOwnership == sema::OwnershipKind::Owned)
    {
        const bool freshOwnedObject =
            statement->first()->kind() == ast::AstNodeKind::ObjectCreationExpression;
        const bool nullOwnedValue =
            statement->first()->kind() == ast::AstNodeKind::NullLiteralExpression;
        const bool validOwnedTransfer =
            (value.ownership == sema::OwnershipKind::Owned && value.ownershipTransfer) ||
            value.ownershipFreshOwner || freshOwnedObject || nullOwnedValue;
        if (!validOwnedTransfer && returnTypeOwnable)
        {
            if (!directOwnerReturnWithoutMove)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    statement->first()->range(),
                    "owned return requires an ownership-transfer expression such as move(owner), new T(...), or an owned-returning call");
            }
            return;
        }

        MethodOwnershipTransferBinding transfer;
        transfer.syntax = statement->first();
        transfer.methodMemberOrdinal = methodMemberOrdinal;
        transfer.scopeOrdinal = scopeOrdinal;
        transfer.sourceLocalOrdinal = value.ownershipSourceLocalOrdinal;
        transfer.sourceParameterOrdinal = value.ownershipSourceParameterOrdinal;
        transfer.targetMemberOrdinal = methodMemberOrdinal;
        transfer.typeKey = method->declaredTypeKey;
        if (transfer.sourceLocalOrdinal.has_value())
        {
            transfer.kind = MethodOwnershipTransferKind::MoveLocalToOwnedReturn;
        }
        else if (transfer.sourceParameterOrdinal.has_value())
        {
            transfer.kind = MethodOwnershipTransferKind::MoveParameterToOwnedReturn;
        }
        else
        {
            transfer.kind = MethodOwnershipTransferKind::OwnedReturnForward;
        }
        ownershipTransferBindings_.push_back(std::move(transfer));
    }
}

const MethodNameBinding* ProjectMethodBodyModel::recordedNameBinding(
    const ast::AstNode* syntax) const noexcept
{
    if (syntax == nullptr) return nullptr;
    for (std::size_t index = nameBindings_.size(); index > 0; --index)
    {
        if (nameBindings_[index - 1].syntax == syntax) return &nameBindings_[index - 1];
    }
    return nullptr;
}

// The constant folder for `case` labels.
//
// WHY IT EXISTS SEPARATELY. The body model already computes `ExpressionResult::integerConstant`,
// but only for the shapes that need it for overload resolution -- literals, unary minus on a
// literal, casts. A `case` label needs more than that and needs it for a different reason: two
// labels that name the same value must be REJECTED, and "the same value" cannot be decided by
// comparing syntax. `case WM_DESTROY:` and `case 2:` are the same label.
//
// WHAT IT DELIBERATELY DOES NOT DO. It is not a general constant evaluator: no floats, no strings,
// no method calls, no `sizeof`. Anything it cannot answer for comes back as std::nullopt and the
// caller reports "a case label must be a compile-time integer constant", which is a better outcome
// than a folder that quietly gets a hard case wrong.
std::optional<std::size_t> ProjectMethodBodyModel::constantMemberOfType(
    std::size_t typeOrdinal, std::string_view name) const noexcept
{
    if (memberModel_ == nullptr) return std::nullopt;
    for (const std::size_t ordinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.kind != ClassMemberKind::Field || member.name != name) continue;
        if (member.isConst || member.isEnumConstant) return ordinal;
    }
    return std::nullopt;
}

// CC1-105. El plegado de constantes enteras ya NO se implementa aqui.
//
// Aqui vivia un segundo evaluador de constantes, escrito para las etiquetas de `switch`, que
// operaba todo sobre `std::int64_t` sin consultar el tipo semantico del valor. Produjo cuatro
// defectos P1 de una sola vez -- casts que no normalizaban (C-001), division/modulo/shift sin
// signo evaluados con signo (C-002), minimo-con-signo entre -1 sin proteger (C-003, comportamiento
// indefinido DENTRO del compilador disparado por texto de entrada) y ancho de shift fijo a 64
// (C-004) -- y no por descuido: un evaluador que no conoce el tipo del valor que evalua no puede
// dar la respuesta correcta a ninguna de esas cuatro preguntas.
//
// Ahora esto es un adaptador: `ProjectIntegerEvaluator` hace el trabajo, y este modelo solo le
// presta su resolucion de nombres. Devuelve el par (bits, tipo) para que el llamador pueda
// normalizar al tipo gobernante antes de comparar.
std::optional<ProjectIntegerValue> ProjectMethodBodyModel::foldIntegerConstant(
    const ast::AstNode* expression,
    std::optional<std::size_t> declaringTypeOrdinal) const
{
    const ConstantMemberResolver resolver(*this);
    return ProjectIntegerEvaluator(&resolver).evaluate(expression, declaringTypeOrdinal);
}

std::optional<std::size_t> ProjectMethodBodyModel::ConstantMemberResolver::resolveConstantMember(
    const ast::AstNode* syntax,
    std::optional<std::size_t> declaringTypeOrdinal) const
{
    // Dos formas llegan aqui y las dos importan: `Color.Red`, y `Messages.Destroy` (un campo
    // `const int`, que es lo que es todo identificador de mensaje de Win32). El nombre ya lo
    // resolvio analyzeExpression, asi que esto lee el binding que dejo en vez de hacer lookup una
    // segunda vez y arriesgar una respuesta distinta.
    if (syntax == nullptr) return std::nullopt;
    const MethodNameBinding* binding = model_.recordedNameBinding(syntax);
    if (binding != nullptr && binding->candidateMemberOrdinals.size() == 1)
    {
        return binding->candidateMemberOrdinals.front();
    }

    // Dentro del inicializador de un campo `const` puede no haber binding que leer: build()
    // analiza los CUERPOS DE METODO ANTES que los inicializadores de campo, asi que cuando una
    // etiqueta en un cuerpo pliega `Messages.Second`, el inicializador `First + 1` todavia no paso
    // por analyzeExpression y su `First` no tiene binding. Resolver un nombre sin calificar contra
    // el propio tipo declarante del campo es exactamente la regla bajo la que ese inicializador
    // esta escrito, y mantiene vivo el idioma de Win32 (`WM_USER + 1`) sin reordenar build().
    if (declaringTypeOrdinal.has_value() &&
        syntax->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        return model_.constantMemberOfType(*declaringTypeOrdinal, syntax->text());
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::ConstantMemberResolver::describeConstantMember(
    std::size_t memberOrdinal,
    bool& isEnumConstant,
    std::int64_t& enumConstantValue,
    std::string& enumUnderlyingTypeKey,
    const ast::AstNode*& initializer,
    std::string& declaredTypeKey,
    std::optional<std::size_t>& declaringTypeOrdinal) const
{
    if (model_.memberModel_ == nullptr ||
        memberOrdinal >= model_.memberModel_->members().size())
    {
        return false;
    }
    const ClassMemberSymbol& member = model_.memberModel_->members()[memberOrdinal];
    if (member.kind != ClassMemberKind::Field) return false;

    isEnumConstant = member.isEnumConstant;
    enumConstantValue = member.enumConstantValue;
    enumUnderlyingTypeKey.clear();
    if (member.isEnumConstant && model_.nameLookup_ != nullptr &&
        member.declaringTypeOrdinal < model_.nameLookup_->namedTypes().size())
    {
        // Nombre completo: el parametro de salida se llama igual que la funcion y la tapa.
        enumUnderlyingTypeKey = std::string(
            ::nativesharp::frontend::enumUnderlyingTypeKey(
                model_.nameLookup_->namedTypes()[member.declaringTypeOrdinal].syntax));
    }
    declaredTypeKey = member.declaredTypeKey;
    declaringTypeOrdinal = member.declaringTypeOrdinal;
    initializer = member.syntax != nullptr ? member.syntax->first() : nullptr;

    if (isEnumConstant) return true;
    return member.isConst && initializer != nullptr;
}

std::string ProjectMethodBodyModel::ConstantMemberResolver::resolveTypeKey(
    const ast::TypeSyntax* typeSyntax) const
{
    if (typeSyntax == nullptr || model_.memberModel_ == nullptr || model_.nameLookup_ == nullptr)
    {
        return {};
    }
    return model_.memberModel_->canonicalTypeKey(typeSyntax, *model_.nameLookup_);
}

// `switch (governing) { case a: ... default: ... }`
//
// THE FOUR THINGS THIS DECIDES, and why each is here rather than downstream:
//
//   1. The governing type must be INTEGRAL or an ENUM. Not `string`: a string switch is a hash
//      table and a chain of comparisons, not a jump table, and pretending otherwise in the syntax
//      would hide that cost. Not `float`: equality on floats is a trap.
//   2. Every `case` label is folded to an integer HERE. Downstream passes get numbers, so neither
//      the HIR, the MIR nor the backend needs to know what a constant expression is.
//   3. DUPLICATE LABELS ARE REJECTED. Two labels with the same value are unreachable code that
//      looks like working code, and LLVM's `switch` has undefined behaviour with a repeated case
//      value, so this is a correctness check and not a style one.
//   4. At most one `default`, in any position.
//
// A switch pushes a LoopContext with isSwitch set, so `break` leaves the switch while `continue`
// walks past it to the enclosing loop. See analyzeLoopExit.
void ProjectMethodBodyModel::analyzeSwitch(
    const ast::AstNode* statement,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (statement == nullptr) return;

    const ExpressionResult governing = analyzeExpression(
        statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);

    const bool governingIsEnum = governing.valid && isEnumTypeKey(governing.typeKey);
    const bool governingIsIntegral =
        governing.valid && ProjectTypeConversion::isIntegral(governing.typeKey);
    if (governing.valid && !governingIsEnum && !governingIsIntegral)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            statement->first() != nullptr ? statement->first()->range() : statement->range(),
            "switch governing expression must have an integral or enum type; it has type '" +
                governing.typeKey + "'");
    }

    MethodSwitchBinding binding;
    binding.syntax = statement;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.governingTypeKey = governing.typeKey;

    // Every label value already claimed, across the whole switch -- sections do not get their own
    // namespace, so this is one flat list.
    std::vector<std::int64_t> seenValues;
    const ast::AstNode* defaultLabel = nullptr;

    for (const ast::AstNode* section : statement->children())
    {
        if (section == nullptr || section->kind() != ast::AstNodeKind::SwitchSection) continue;

        MethodSwitchSectionBinding sectionBinding;
        sectionBinding.syntax = section;

        const std::size_t labelCount = static_cast<std::size_t>(section->integerValue());
        for (std::size_t index = 0; index < labelCount && index < section->children().size(); ++index)
        {
            const ast::AstNode* label = section->children()[index];
            if (label == nullptr || label->kind() != ast::AstNodeKind::SwitchLabel) continue;

            if (label->flag())
            {
                if (defaultLabel != nullptr)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        label->range(),
                        "a switch may declare only one default label");
                    continue;
                }
                defaultLabel = label;
                sectionBinding.hasDefault = true;
                continue;
            }

            const ExpressionResult value = analyzeExpression(
                label->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!value.valid) continue;

            if (governing.valid)
            {
                const ProjectImplicitConversion conversion =
                    classifyImplicit(value.typeKey, governing.typeKey, value.integerConstant);
                if (!conversion.exists())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidConversion,
                        diagnostics::DiagnosticSeverity::Error,
                        label->range(),
                        "case label of type '" + value.typeKey +
                            "' cannot be converted to the switch governing type '" +
                            governing.typeKey + "'");
                    continue;
                }
            }

            const std::optional<ProjectIntegerValue> folded =
                foldIntegerConstant(label->first(), std::nullopt);
            if (!folded.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::ConstRequiresConstant,
                    diagnostics::DiagnosticSeverity::Error,
                    label->range(),
                    "a case label must be a compile-time integer constant");
                continue;
            }

            // CC1-106. LA NORMALIZACION VA ANTES DE COMPARAR, y esa es toda la correccion de C-001.
            //
            // Una etiqueta se pliega en el tipo de SU PROPIA expresion, que puede ser mas ancho que
            // el tipo gobernante. El backend, en cambio, materializa la constante con el ancho del
            // gobernante. Comparar sin normalizar deja que el frontend vea dos numeros distintos
            // donde LLVM vera dos casos identicos -- y un `switch` de LLVM con un valor de caso
            // repetido es comportamiento indefinido.
            //
            // El caso concreto: gobernante `char`, `case 65:` y `case 321:`. Antes de CC1-100 la
            // conversion constante aceptaba 321 hacia `char`, el frontend los veia distintos, y el
            // backend emitia `i8 65` para los dos.
            const std::int64_t canonical = static_cast<std::int64_t>(
                governing.valid && ProjectIntegerEvaluator::isIntegerType(governing.typeKey)
                    ? ProjectIntegerEvaluator::normalize(folded->bits, governing.typeKey)
                    : folded->bits);

            bool duplicate = false;
            for (const std::int64_t seen : seenValues)
            {
                if (seen != canonical) continue;
                duplicate = true;
                // No es una regla de estilo: el `switch` de LLVM tiene comportamiento indefinido con
                // un valor de caso repetido, y la segunda etiqueta es codigo muerto que se lee como
                // codigo vivo.
                diagnostics.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    label->range(),
                    "duplicate case label with value " + std::to_string(canonical));
                break;
            }
            if (duplicate) continue;

            // CC1-107. Aguas abajo viaja el valor YA CANONICO, para que HIR, MIR y el backend
            // reciban exactamente el patron de bits que LLVM materializara. Si el binding llevara el
            // valor sin normalizar, la comprobacion de duplicados de arriba estaria protegiendo un
            // numero distinto del que acaba en la instruccion.
            seenValues.push_back(canonical);
            sectionBinding.labelValues.push_back(canonical);
        }

        binding.sections.push_back(std::move(sectionBinding));
    }

    // Every section starts from the state at the switch, never from the previous section's exit:
    // there is no fall-through, so the previous section's effects cannot reach this one.
    const OwnershipFlowSnapshot switchEntry = captureOwnershipFlow();
    loopStack_.push_back(LoopContext{
        scopeOrdinal,
        activeCleanupRegions_.size(),
        activeCleanupRegions_.size(),
        {},
        {},
        true});

    std::vector<OwnershipFlowSnapshot> sectionExitStates;
    for (const ast::AstNode* section : statement->children())
    {
        if (section == nullptr || section->kind() != ast::AstNodeKind::SwitchSection) continue;
        restoreOwnershipFlow(switchEntry);
        analyzeBlock(section, methodMemberOrdinal, scopeOrdinal, true, diagnostics);
        const OwnershipFlowSnapshot exitState = captureOwnershipFlow();
        if (exitState.reachable)
        {
            sectionExitStates.push_back(exitState);
        }
    }

    LoopContext switchContext = std::move(loopStack_.back());
    loopStack_.pop_back();

    // Control reaches the statement after the switch three ways: off the end of a section, through
    // a `break`, or -- only when there is no `default` -- by matching nothing at all.
    std::vector<OwnershipFlowSnapshot> exitStates = std::move(switchContext.breakStates);
    for (OwnershipFlowSnapshot& state : sectionExitStates)
    {
        exitStates.push_back(std::move(state));
    }
    OwnershipFlowSnapshot seed;
    if (defaultLabel != nullptr)
    {
        seed.reachable = false;
    }
    else
    {
        seed = switchEntry;
    }
    restoreOwnershipFlow(joinOwnershipFlows(seed, exitStates));

    switchBindings_.push_back(std::move(binding));
}

bool ProjectMethodBodyModel::analyzeLoopExit(
    const ast::AstNode* statement,
    MethodStructuredExitKind kind,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (loopStack_.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            statement != nullptr ? statement->range() : SourceRange(),
            kind == MethodStructuredExitKind::Break
                ? "break is only valid within a loop or a switch"
                : "continue is only valid within a loop");
        return false;
    }

    // `break` binds to the innermost frame, loop or switch alike -- that is what makes `break`
    // inside a case leave the switch. `continue` binds to the innermost LOOP, so it walks past
    // every switch frame on the way out: a `continue` inside a `switch` inside a `while` continues
    // the while. Getting this backwards is not a compile error anywhere downstream; it is a jump
    // to the wrong label.
    std::size_t targetLoopIndex = loopStack_.size() - 1;
    if (kind == MethodStructuredExitKind::Continue)
    {
        while (loopStack_[targetLoopIndex].isSwitch)
        {
            if (targetLoopIndex == 0)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    statement != nullptr ? statement->range() : SourceRange(),
                    "continue is only valid within a loop");
                return false;
            }
            --targetLoopIndex;
        }
    }

    for (const FinallyContext& finallyContext : finallyStack_)
    {
        if (targetLoopIndex < finallyContext.loopDepthAtEntry)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                statement != nullptr ? statement->range() : SourceRange(),
                kind == MethodStructuredExitKind::Break
                    ? "break cannot leave a finally block"
                    : "continue cannot leave a finally block");
            return false;
        }
    }

    LoopContext& loop = loopStack_[targetLoopIndex];
    if (kind == MethodStructuredExitKind::Break)
    {
        loop.breakStates.push_back(captureOwnershipFlow());
    }
    else
    {
        loop.continueStates.push_back(captureOwnershipFlow());
    }
    recordStructuredExit(
        kind,
        statement,
        methodMemberOrdinal,
        scopeOrdinal,
        loop.targetScopeOrdinal,
        kind == MethodStructuredExitKind::Break
            ? loop.breakCleanupDepth
            : loop.continueCleanupDepth);
    return true;
}

std::size_t ProjectMethodBodyModel::createCleanupRegion(
    MethodCleanupRegionKind kind,
    const ast::AstNode* syntax,
    const ast::AstNode* cleanupSyntax,
    std::size_t methodMemberOrdinal,
    std::size_t ownerScopeOrdinal)
{
    MethodCleanupRegion region;
    region.kind = kind;
    region.syntax = syntax;
    region.cleanupSyntax = cleanupSyntax;
    region.methodMemberOrdinal = methodMemberOrdinal;
    region.ownerScopeOrdinal = ownerScopeOrdinal;
    region.depth = activeCleanupRegions_.size();
    if (!activeCleanupRegions_.empty())
    {
        region.parentCleanupRegionOrdinal = activeCleanupRegions_.back();
    }

    const std::size_t ordinal = cleanupRegions_.size();
    cleanupRegions_.push_back(std::move(region));
    return ordinal;
}

std::vector<std::size_t> ProjectMethodBodyModel::cleanupPathFromDepth(std::size_t retainedDepth) const
{
    retainedDepth = std::min(retainedDepth, activeCleanupRegions_.size());
    std::vector<std::size_t> path;
    path.reserve(activeCleanupRegions_.size() - retainedDepth);
    for (std::size_t index = activeCleanupRegions_.size(); index > retainedDepth; --index)
    {
        path.push_back(activeCleanupRegions_[index - 1]);
    }
    return path;
}

void ProjectMethodBodyModel::recordStructuredExit(
    MethodStructuredExitKind kind,
    const ast::AstNode* syntax,
    std::size_t methodMemberOrdinal,
    std::size_t sourceScopeOrdinal,
    std::optional<std::size_t> targetScopeOrdinal,
    std::size_t retainedCleanupDepth)
{
    MethodStructuredExitBinding binding;
    binding.kind = kind;
    binding.syntax = syntax;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.sourceScopeOrdinal = sourceScopeOrdinal;
    binding.targetScopeOrdinal = targetScopeOrdinal;
    binding.cleanupRegionOrdinals = cleanupPathFromDepth(retainedCleanupDepth);
    recordCleanupContinuations(binding);
    structuredExits_.push_back(std::move(binding));
}

void ProjectMethodBodyModel::recordCleanupContinuations(const MethodStructuredExitBinding& exit)
{
    MethodCleanupContinuationKind continuationKind = MethodCleanupContinuationKind::Return;
    switch (exit.kind)
    {
        case MethodStructuredExitKind::Return:
            continuationKind = MethodCleanupContinuationKind::Return;
            break;
        case MethodStructuredExitKind::Break:
            continuationKind = MethodCleanupContinuationKind::Break;
            break;
        case MethodStructuredExitKind::Continue:
            continuationKind = MethodCleanupContinuationKind::Continue;
            break;
    }

    for (std::size_t index = 0; index < exit.cleanupRegionOrdinals.size(); ++index)
    {
        MethodCleanupContinuation continuation;
        continuation.kind = continuationKind;
        continuation.syntax = exit.syntax;
        continuation.methodMemberOrdinal = exit.methodMemberOrdinal;
        continuation.sourceScopeOrdinal = exit.sourceScopeOrdinal;
        continuation.cleanupRegionOrdinal = exit.cleanupRegionOrdinals[index];
        if (index + 1 < exit.cleanupRegionOrdinals.size())
        {
            continuation.nextCleanupRegionOrdinal = exit.cleanupRegionOrdinals[index + 1];
        }
        continuation.targetScopeOrdinal = exit.targetScopeOrdinal;
        cleanupContinuations_.push_back(std::move(continuation));
    }
}

void ProjectMethodBodyModel::recordNormalCleanupContinuation(
    std::size_t cleanupRegionOrdinal,
    const ast::AstNode* syntax,
    std::size_t methodMemberOrdinal,
    std::size_t sourceScopeOrdinal)
{
    MethodCleanupContinuation continuation;
    continuation.kind = MethodCleanupContinuationKind::Normal;
    continuation.syntax = syntax;
    continuation.methodMemberOrdinal = methodMemberOrdinal;
    continuation.sourceScopeOrdinal = sourceScopeOrdinal;
    continuation.cleanupRegionOrdinal = cleanupRegionOrdinal;
    cleanupContinuations_.push_back(std::move(continuation));
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowMove,
    bool allowConstructorPropertyWrite)
{
    if (expression == nullptr)
    {
        ExpressionResult empty;
        empty.typeKey = "builtin:void";
        empty.valid = true;
        return empty;
    }

    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
            result = analyzeIdentifier(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup, allowEvent,
                allowConstructorPropertyWrite);
            break;
        case ast::AstNodeKind::ThisExpression:
            if (!contextHasImplicitThis(methodMemberOrdinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidMemberAccess,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "this is not available in a static member or field initializer");
                break;
            }
            result.typeKey = implicitThisTypeKey(methodMemberOrdinal);
            result.valid = !isErrorType(result.typeKey);
            result.receiverSyntax = expression;
            result.receiverTypeKey = result.typeKey;
            result.receiverTypeOrdinal = referenceTypeOrdinalForTypeKey(result.typeKey);
            result.readonlyReceiver = contextHasReadonlyReceiver(methodMemberOrdinal);
            nameBindings_.push_back(MethodNameBinding{
                MethodNameBindingKind::ThisReference,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "this",
                result.typeKey,
                std::nullopt,
                std::nullopt,
                {},
                expression,
                result.typeKey,
                result.receiverTypeOrdinal,
                false});
            break;
        case ast::AstNodeKind::BaseExpression:
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidMemberAccess,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                contextHasImplicitThis(methodMemberOrdinal)
                    ? "base may only appear as the receiver of a member access"
                    : "base is not available in a static member or field initializer");
            break;
        case ast::AstNodeKind::IntegerLiteralExpression:
            result.typeKey = integerLiteralType(expression);
            result.valid = true;
            result.integerConstant = ProjectIntegerConstant{
                expression->integerValue(),
                isUnsignedIntegerLiteral(expression)};
            break;
        case ast::AstNodeKind::FloatingLiteralExpression:
            result.typeKey = (!expression->text().empty() &&
                (expression->text().back() == 'f' || expression->text().back() == 'F'))
                ? "builtin:float"
                : "builtin:double";
            result.valid = true;
            break;
        case ast::AstNodeKind::CharacterLiteralExpression:
            result.typeKey = "builtin:char";
            result.valid = true;
            break;
        case ast::AstNodeKind::StringLiteralExpression:
            result.typeKey = "string-literal";
            result.valid = true;
            coreBindings_.push_back(MethodCoreBinding{
                MethodCoreOperationKind::StringLiteral,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "builtin:string",
                "builtin:char",
                false, std::nullopt, std::nullopt});
            break;
        case ast::AstNodeKind::InterpolatedStringExpression:
        {
            // T01-F06 first allocation-free handler: every hole is a compile-time scalar and the
            // complete answer is interned exactly like a source string literal. Runtime holes are
            // rejected instead of silently selecting String.Concat (which would allocate heap in
            // a `noheap` context). Later handler protocols may widen this without changing the
            // representation of this fully-static form.
            std::vector<language::InterpolationPart> parts;
            std::string parseError;
            if (!language::parseStaticInterpolation(expression->text(), parts, parseError))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidStringLiteral,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    std::move(parseError));
                break;
            }
            bool valid = true;
            std::string rendered;
            for (const language::InterpolationPart& part : parts)
            {
                if (part.kind == language::InterpolationPartKind::Literal)
                {
                    rendered += part.text;
                    continue;
                }
                if (part.text == "true" || part.text == "false")
                {
                    if (!part.format.empty()) valid = false;
                    else rendered += part.text;
                    continue;
                }

                bool negative = false;
                std::string_view digits = part.text;
                if (!digits.empty() && digits.front() == '-')
                {
                    negative = true;
                    digits.remove_prefix(1);
                }
                std::uint64_t value = 0;
                const auto parsed = std::from_chars(
                    digits.data(), digits.data() + digits.size(), value, 10);
                const bool integer = !digits.empty() && parsed.ec == std::errc{} &&
                    parsed.ptr == digits.data() + digits.size();
                char mode = 'D';
                std::size_t width = 0;
                bool formatValid = part.format.empty();
                if (!part.format.empty() &&
                    (part.format.front() == 'D' || part.format.front() == 'd' ||
                     part.format.front() == 'X' || part.format.front() == 'x'))
                {
                    mode = part.format.front();
                    std::string_view widthText(part.format);
                    widthText.remove_prefix(1);
                    formatValid = widthText.empty();
                    if (!widthText.empty())
                    {
                        const auto widthResult = std::from_chars(
                            widthText.data(), widthText.data() + widthText.size(), width, 10);
                        formatValid = widthResult.ec == std::errc{} &&
                            widthResult.ptr == widthText.data() + widthText.size() && width <= 64U;
                    }
                }
                if (!integer || !formatValid ||
                    (negative && (mode == 'X' || mode == 'x')))
                {
                    valid = false;
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidStringLiteral,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "static interpolation hole '" + part.text +
                            "' must be a bool or integer literal with constant D/d/X/x format; "
                            "runtime holes require an explicit formatting handler");
                    continue;
                }
                char buffer[96] = {};
                const int base = (mode == 'X' || mode == 'x') ? 16 : 10;
                const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, base);
                std::string text(buffer, converted.ptr);
                if (mode == 'X')
                {
                    for (char& ch : text)
                    {
                        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    }
                }
                if (text.size() < width) text.insert(0, width - text.size(), '0');
                if (negative) text.insert(text.begin(), '-');
                rendered += text;
            }
            if (!valid) break;
            staticInterpolationValues_[expression] = std::move(rendered);
            result.typeKey = "string-literal";
            result.valid = true;
            break;
        }
        case ast::AstNodeKind::BooleanLiteralExpression:
            result.typeKey = "builtin:bool";
            result.valid = true;
            break;
        case ast::AstNodeKind::NullLiteralExpression:
            result.typeKey = "builtin:null";
            result.valid = true;
            break;
        case ast::AstNodeKind::LambdaExpression:
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion, diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "Stage 43 lambda expression requires a target delegate type");
            break;
        case ast::AstNodeKind::MoveExpression:
        {
            // A field/element move mutates one slot without consuming its containing owner. Find
            // the named owner at the root of that storage expression so an outstanding borrowed
            // view cannot observe the mutation behind the borrow checker's back.
            const auto partialMoveRoot = [&](const ast::AstNode* storage)
                -> std::optional<ScopeName>
            {
                const ast::AstNode* root = storage;
                while (root != nullptr &&
                       (root->kind() == ast::AstNodeKind::MemberExpression ||
                        root->kind() == ast::AstNodeKind::IndexExpression))
                {
                    root = root->first();
                }
                if (root == nullptr || root->kind() != ast::AstNodeKind::IdentifierExpression)
                {
                    return std::nullopt;
                }
                const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, root->text());
                if (!name.has_value()) return std::nullopt;
                if (name->kind == ScopeNameKind::Local)
                {
                    return name->semanticOrdinal < locals_.size() &&
                            locals_[name->semanticOrdinal].isRaii
                        ? name
                        : std::nullopt;
                }
                const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
                return method != nullptr && name->semanticOrdinal < method->parameters.size() &&
                        method->parameters[name->semanticOrdinal].ownership == sema::OwnershipKind::Owned
                    ? name
                    : std::nullopt;
            };
            const auto rejectInvalidPartialMove = [&](const std::optional<ScopeName>& root) -> bool
            {
                if (!root.has_value()) return false;
                if (root->kind == ScopeNameKind::Local)
                {
                    if (locals_[root->semanticOrdinal].isPinned)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            "cannot partially move from pinned owner '" +
                                locals_[root->semanticOrdinal].name +
                                "'; pinned projections are not movable");
                        return true;
                    }
                    if (!hasActiveBorrowOfOwnerLocal(root->semanticOrdinal, scopeOrdinal)) return false;
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "cannot partially move from owner '" + locals_[root->semanticOrdinal].name +
                            "' while a borrowed local is alive");
                    return true;
                }
                if (!hasActiveBorrowOfOwnedParameter(root->semanticOrdinal, scopeOrdinal)) return false;
                const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "cannot partially move from owned parameter '" +
                        method->parameters[root->semanticOrdinal].name +
                        "' while a borrowed local is alive");
                return true;
            };

            // Stage 52: move(...) also accepts a direct native-array element access, e.g.
            // "move(items[i])". This is what lets a generic collection (Queue<T>.Dequeue(),
            // Stack<T>.Pop(), ...) extract an owned element out of its backing T[] storage and
            // return it -- previously the only supported move sources were a named RAII local
            // or an owned parameter, neither of which a computed array slot is. Static
            // moved-from tracking (movedRaiiLocals_/maybeMovedRaiiLocals_ and friends) is
            // deliberately NOT extended here: a runtime array index cannot be reasoned about at
            // compile time the way a named local/parameter can, so this operation stays
            // available only through the explicit, auditable move(...) syntax rather than
            // silently applying to ordinary array reads -- and HIR/LLVM lowering pairs every
            // extraction with a null-store into the vacated slot to prevent a later `delete`
            // from double-freeing it. It is the caller's responsibility not to also read the
            // same slot again as if it still held a live value.
            //
            // SHR-005 (residuo de "owned destination"). Aqui tambien entra el ARREGLO FIJO
            // (`T[n]` en un campo). Era el unico almacenamiento con elementos que no participaba
            // del chequeo de ownership: `isNativeArray` compara el sufijo `"[]"` y un arreglo fijo
            // termina en `"n]"`, asi que `move(buf[i])` se rechazaba con este mismo mensaje y
            // `buf[i] = alias` se aceptaba SIN comprobar nada. Los dos lados del mismo hueco.
            //
            // Por que es el MISMO caso y no uno parecido: lo unico que la bajada necesita de este
            // ligado es una DIRECCION de ranura, y el HIR ya sabe dar la de un elemento de arreglo
            // fijo (caso RawElement, "C2.1 fixed arrays"); `MoveArrayElement` recibe esa direccion
            // y hace el load-and-null de siempre, gobernado por `elementIsReferenceLike`, que sale
            // del tipo del ELEMENTO. Un arreglo fijo de un tipo por valor sigue siendo el no-op
            // que ya era.
            if (allowMove && expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IndexExpression)
            {
                const std::optional<ScopeName> partialRoot = partialMoveRoot(expression->first());
                if (rejectInvalidPartialMove(partialRoot)) break;
                const ExpressionResult element = analyzeExpression(
                    expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                // El analisis de la base se mantiene DENTRO del cortocircuito, igual que antes: si
                // el elemento ya no es valido, volver a analizar la base solo duplicaria sus
                // diagnosticos.
                bool acceptableElementSource = element.valid && element.assignable;
                if (acceptableElementSource)
                {
                    const std::string elementBaseTypeKey = expression->first()->first() != nullptr
                        ? analyzeExpression(
                              expression->first()->first(), methodMemberOrdinal, scopeOrdinal, diagnostics)
                              .typeKey
                        : "<error>";
                    acceptableElementSource =
                        ProjectTypeConversion::isNativeArray(elementBaseTypeKey) ||
                        ProjectMemberModel::fixedArrayElementTypeKey(elementBaseTypeKey).has_value() ||
                        ProjectTypeConversion::isPointer(elementBaseTypeKey);
                }
                if (!acceptableElementSource)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "move(...) array-element source must be a direct native array, fixed array, or raw pointer element access");
                    break;
                }
                MethodMoveBinding move;
                move.syntax = expression;
                move.methodMemberOrdinal = methodMemberOrdinal;
                move.scopeOrdinal = scopeOrdinal;
                move.typeKey = element.typeKey;
                move.sourceIsArrayElement = true;
                if (partialRoot.has_value() && partialRoot->kind == ScopeNameKind::Local)
                {
                    move.partialSourceOwnerLocalOrdinal = partialRoot->semanticOrdinal;
                }
                else if (partialRoot.has_value())
                {
                    move.partialSourceOwnerParameterOrdinal = partialRoot->semanticOrdinal;
                }
                move.elementIsReferenceLike =
                    isReferenceTypeKey(element.typeKey) || element.typeKey == "builtin:string";
                moveBindings_.push_back(std::move(move));
                result.typeKey = element.typeKey;
                result.valid = true;
                result.assignable = false;
                result.ownership = sema::OwnershipKind::Owned;
                result.ownershipTransfer = true;
                break;
            }
            // Bloque 1 (audit finding "move-out desde field owned"): move(...) also accepts a
            // direct `owned` FIELD access -- `move(holder.slot)` and, inside the declaring class,
            // `move(slot)`. Without this, an owned field was an ownership SINK: values could be
            // transferred in but never taken back out, so a container could not implement the
            // Take()/Dequeue()-shaped operations its array-backed siblings already have via
            // move(items[i]).
            //
            // Semantics, atomically from ownership's point of view: load the slot, null it, hand
            // the loaded value to the new owner. The null-store is what makes the container's own
            // later discharge (its destruction entry, or an overwrite's release) skip the vacated
            // slot -- the same "the slot's null state IS the guard" rule every owner cleanup here
            // uses. Like the array-element case above, static moved-from tracking is deliberately
            // NOT extended to fields: which object a field belongs to is a runtime fact, so the
            // vacated slot simply holds null and a later read sees an empty slot.
            const auto tryOwnedFieldMoveSource = [&](
                const ExpressionResult& fieldSource,
                const std::optional<ScopeName>& partialRoot) -> bool
            {
                if (!fieldSource.valid || !fieldSource.assignable ||
                    !fieldSource.assignmentMemberOrdinal.has_value() || memberModel_ == nullptr ||
                    *fieldSource.assignmentMemberOrdinal >= memberModel_->members().size())
                {
                    return false;
                }
                const ClassMemberSymbol& sourceField =
                    memberModel_->members()[*fieldSource.assignmentMemberOrdinal];
                const bool ownedStorageField = sourceField.kind == ClassMemberKind::Field &&
                    !sourceField.isStatic &&
                    sourceField.declaredOwnership == sema::OwnershipKind::Owned;
                if (!ownedStorageField) return false;
                MethodMoveBinding move;
                move.syntax = expression;
                move.methodMemberOrdinal = methodMemberOrdinal;
                move.scopeOrdinal = scopeOrdinal;
                move.typeKey = fieldSource.typeKey;
                move.sourceIsOwnedField = true;
                if (partialRoot.has_value() && partialRoot->kind == ScopeNameKind::Local)
                {
                    move.partialSourceOwnerLocalOrdinal = partialRoot->semanticOrdinal;
                }
                else if (partialRoot.has_value())
                {
                    move.partialSourceOwnerParameterOrdinal = partialRoot->semanticOrdinal;
                }
                move.elementIsReferenceLike =
                    isReferenceTypeKey(fieldSource.typeKey) || fieldSource.typeKey == "builtin:string";
                moveBindings_.push_back(std::move(move));
                result.typeKey = fieldSource.typeKey;
                result.valid = true;
                result.assignable = false;
                result.ownership = sema::OwnershipKind::Owned;
                result.ownershipTransfer = true;
                return true;
            };
            if (allowMove && expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::MemberExpression)
            {
                const std::optional<ScopeName> partialRoot = partialMoveRoot(expression->first());
                if (rejectInvalidPartialMove(partialRoot)) break;
                const ExpressionResult member = analyzeExpression(
                    expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                if (tryOwnedFieldMoveSource(member, partialRoot)) break;
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "move(...) member source must be an assignable owned field");
                break;
            }
            if (!allowMove || expression->first() == nullptr ||
                expression->first()->kind() != ast::AstNodeKind::IdentifierExpression)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "move(...) is only valid for a direct RAII local or owned parameter in RAII initialization, move assignment, owned argument transfer, or return");
                break;
            }
            const std::optional<ScopeName> source = lookupScopeName(scopeOrdinal, expression->first()->text());
            if (!source.has_value())
            {
                // An identifier that is not a local or parameter can still be a FIELD of the
                // current type reached through the implicit `this` -- `move(slot)` inside the
                // declaring class is the container's own Take() idiom.
                const ExpressionResult implicitField = analyzeExpression(
                    expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                if (tryOwnedFieldMoveSource(implicitField, std::nullopt)) break;
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "move(...) source must be a RAII local or owned parameter");
                break;
            }

            if (source->kind == ScopeNameKind::Local)
            {
                if (source->semanticOrdinal >= locals_.size() || !locals_[source->semanticOrdinal].isRaii)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "move(...) source must be a RAII local or owned parameter");
                    break;
                }
                const MethodLocalSymbol& local = locals_[source->semanticOrdinal];
                if (local.isPinned)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "cannot move pinned owner '" + local.name +
                            "'; it must remain at a stable address until lexical destruction");
                    break;
                }
                if (movedRaiiLocals_.contains(source->semanticOrdinal))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "cannot move from already moved-from RAII local '" + local.name + "'");
                    break;
                }
                if (maybeMovedRaiiLocals_.contains(source->semanticOrdinal))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "cannot move from possibly moved RAII local '" + local.name +
                            "' after a control-flow join");
                    break;
                }
                if (hasActiveBorrowOfOwnerLocal(source->semanticOrdinal, scopeOrdinal))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "cannot move owner '" + local.name + "' while a borrowed local is alive");
                    break;
                }
                nameBindings_.push_back(MethodNameBinding{
                    MethodNameBindingKind::Local,
                    expression->first(),
                    methodMemberOrdinal,
                    scopeOrdinal,
                    local.name,
                    local.typeKey,
                    source->semanticOrdinal,
                    std::nullopt,
                    {},
                    nullptr,
                    "",
                    std::nullopt,
                    false});
                MethodMoveBinding move;
                move.syntax = expression;
                move.methodMemberOrdinal = methodMemberOrdinal;
                move.scopeOrdinal = scopeOrdinal;
                move.sourceLocalOrdinal = source->semanticOrdinal;
                move.typeKey = local.typeKey;
                move.elementIsReferenceLike =
                    isReferenceTypeKey(local.typeKey) || local.typeKey == "builtin:string" ||
                    ProjectTypeConversion::isNativeArray(local.typeKey);
                moveBindings_.push_back(std::move(move));
                maybeMovedRaiiLocals_.erase(source->semanticOrdinal);
                movedRaiiLocals_.insert(source->semanticOrdinal);
                invalidateHistoricalBorrowsOfOwnerLocal(source->semanticOrdinal);
                result.typeKey = local.typeKey;
                result.ownershipSourceLocalOrdinal = source->semanticOrdinal;
                result.objectLifetimeOwnership = local.objectLifetimeOwnership;
                result.storageOwnership = local.storageOwnership;
            }
            else
            {
                const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
                if (method == nullptr || source->semanticOrdinal >= method->parameters.size() ||
                    method->parameters[source->semanticOrdinal].ownership != sema::OwnershipKind::Owned)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "move(...) source must be a RAII local or owned parameter");
                    break;
                }
                const MethodParameterSymbol& parameter = method->parameters[source->semanticOrdinal];
                if (movedOwnedParameters_.contains(source->semanticOrdinal))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "cannot move from already moved-from owned parameter '" + parameter.name + "'");
                    break;
                }
                if (maybeMovedOwnedParameters_.contains(source->semanticOrdinal))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "cannot move from possibly moved owned parameter '" + parameter.name +
                            "' after a control-flow join");
                    break;
                }
                if (hasActiveBorrowOfOwnedParameter(source->semanticOrdinal, scopeOrdinal))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "cannot move owned parameter '" + parameter.name +
                            "' while a borrowed local is alive");
                    break;
                }
                nameBindings_.push_back(MethodNameBinding{
                    MethodNameBindingKind::Parameter,
                    expression->first(),
                    methodMemberOrdinal,
                    scopeOrdinal,
                    parameter.name,
                    parameter.typeKey,
                    std::nullopt,
                    source->semanticOrdinal,
                    {},
                    nullptr,
                    "",
                    std::nullopt,
                    false});
                MethodMoveBinding move;
                move.syntax = expression;
                move.methodMemberOrdinal = methodMemberOrdinal;
                move.scopeOrdinal = scopeOrdinal;
                move.sourceParameterOrdinal = source->semanticOrdinal;
                move.typeKey = parameter.typeKey;
                move.elementIsReferenceLike =
                    isReferenceTypeKey(parameter.typeKey) || parameter.typeKey == "builtin:string" ||
                    ProjectTypeConversion::isNativeArray(parameter.typeKey);
                moveBindings_.push_back(std::move(move));
                maybeMovedOwnedParameters_.erase(source->semanticOrdinal);
                movedOwnedParameters_.insert(source->semanticOrdinal);
                invalidateHistoricalBorrowsOfOwnedParameter(source->semanticOrdinal);
                result.typeKey = parameter.typeKey;
                result.ownershipSourceParameterOrdinal = source->semanticOrdinal;
                result.objectLifetimeOwnership = sema::OwnershipKind::Owned;
                result.storageOwnership = sema::OwnershipKind::Owned;
            }

            result.valid = true;
            result.assignable = false;
            result.ownership = sema::OwnershipKind::Owned;
            result.ownershipTransfer = true;
            break;
        }
        case ast::AstNodeKind::NullConditionalMemberExpression:
        {
            result = analyzeMember(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                allowMethodGroup, false, false);
            if (!result.valid) break;
            const std::optional<std::size_t> receiverOrdinal =
                namedTypeOrdinalForTypeKey(result.receiverTypeKey);
            const bool nullableReceiver = receiverOrdinal.has_value() && inheritanceModel_ != nullptr &&
                (inheritanceModel_->isClass(*receiverOrdinal) ||
                 (interfaceModel_ != nullptr && interfaceModel_->isInterface(*receiverOrdinal)));
            if (!nullableReceiver)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->first() != nullptr ? expression->first()->range() : expression->range(),
                    "the receiver of '?.' must be a class or interface reference");
                result.valid = false;
                break;
            }
            const bool nullableResult = ProjectTypeConversion::isPointer(result.typeKey) ||
                ProjectTypeConversion::isNativeArray(result.typeKey) ||
                result.typeKey == "builtin:string" || isAnyDelegateTypeKey(result.typeKey) ||
                [&]
                {
                    const std::optional<std::size_t> ordinal =
                        namedTypeOrdinalForTypeKey(result.typeKey);
                    return ordinal.has_value() && inheritanceModel_ != nullptr &&
                        (inheritanceModel_->isClass(*ordinal) ||
                         (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)));
                }();
            if (!result.assignmentMemberOrdinal.has_value() || !nullableResult)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "this '?.' access requires a nullable instance field result; value members "
                    "require the Optional<T> carrier and property/method access requires a "
                    "captured conditional-call lowering");
                result.valid = false;
                break;
            }
            result.assignable = false;
            result.addressable = false;
            break;
        }
        case ast::AstNodeKind::MemberExpression:
            result = analyzeMember(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup, allowEvent,
                allowConstructorPropertyWrite);
            break;
        // IDs 80 y 83. `p->campo` y `p->Metodo()` ya no llegan aqui: el parser los convierte en
        // `(*p).campo` y `(*p).Metodo()`, que es la MISMA operacion y la que siempre funciono.
        // Ver Parser::parsePostfixExpression. El caso se queda -- sin rechazo -- porque el nodo
        // sigue existiendo en el AST y un `default` mudo seria peor que una ruta explicita.
        case ast::AstNodeKind::PointerMemberExpression:
            result = analyzeMember(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup, allowEvent,
                allowConstructorPropertyWrite);
            break;
        case ast::AstNodeKind::CallExpression:
            result = analyzeCall(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
            result = analyzeUnary(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::BinaryExpression:
            result = analyzeBinary(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::NullCoalescingExpression:
        {
            // T01-F03. The left operand is unconditional; the right operand is a real branch.
            // Keeping the branch in the ownership/definite-assignment model matters just as much
            // as keeping it in LLVM: a move or assignment on the fallback edge did not happen on
            // the non-null edge.
            const ExpressionResult left = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            std::optional<MethodOptionalCarrierBinding> optionalCarrier;
            const auto nullCarrier = [&](std::string_view typeKey)
            {
                if (ProjectTypeConversion::isPointer(typeKey) ||
                    ProjectTypeConversion::isNativeArray(typeKey) ||
                    typeKey == "builtin:string" || isAnyDelegateTypeKey(typeKey))
                {
                    return true;
                }
                optionalCarrier = resolveOptionalCarrier(typeKey);
                if (optionalCarrier.has_value()) return true;
                const std::optional<std::size_t> ordinal = referenceTypeOrdinalForTypeKey(typeKey);
                return ordinal.has_value() && inheritanceModel_ != nullptr &&
                    (inheritanceModel_->isClass(*ordinal) ||
                     (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)));
            };
            const bool leftIsCarrier = left.valid && nullCarrier(left.typeKey);
            if (left.valid && !leftIsCarrier)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->first() != nullptr ? expression->first()->range() : expression->range(),
                    "left operand of '?' '?' must be a pointer, reference, delegate, native array, "
                    "native string, or a supported Optional<T> carrier");
            }

            const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
            ExpressionResult right;
            const OwnershipFlowSnapshot rightState = analyzeBranchState(
                branchEntry,
                [&]
                {
                    right = analyzeExpression(
                        expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                });
            commitBranchJoin(branchEntry, rightState);
            if (!left.valid || !right.valid || !leftIsCarrier) break;

            const std::string resultType = optionalCarrier.has_value()
                ? optionalCarrier->valueTypeKey : left.typeKey;
            const ProjectImplicitConversion conversion = classifyImplicit(
                right.typeKey, resultType, right.integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->second() != nullptr ? expression->second()->range() : expression->range(),
                    "right operand of '?' '?' with type '" + right.typeKey +
                        "' is not implicitly convertible to '" + resultType + "'");
                break;
            }
            if (optionalCarrier.has_value())
            {
                optionalCarrier->syntax = expression;
                optionalCarrier->methodMemberOrdinal = methodMemberOrdinal;
                optionalCarrier->scopeOrdinal = scopeOrdinal;
                optionalCarrierBindings_.push_back(std::move(*optionalCarrier));
            }
            result.typeKey = resultType;
            result.valid = true;
            result.assignable = false;
            break;
        }
        case ast::AstNodeKind::TupleExpression:
        {
            if (expression->children().size() < 2)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "a tuple literal requires at least two elements");
                break;
            }
            std::vector<std::string> elementTypeKeys;
            elementTypeKeys.reserve(expression->children().size());
            MethodAggregateInitializerBinding binding;
            binding.syntax = expression;
            bool valid = true;
            for (std::size_t index = 0; index < expression->children().size(); ++index)
            {
                const ast::AstNode* element = expression->children()[index];
                const ExpressionResult value = analyzeExpression(
                    element, methodMemberOrdinal, scopeOrdinal, diagnostics);
                valid = valid && value.valid;
                elementTypeKeys.push_back(value.valid ? value.typeKey : std::string("<error>"));
                MethodAggregateInitializerEntryBinding entry;
                entry.syntax = element;
                entry.targetElementOrdinals.push_back(index);
                binding.entries.push_back(std::move(entry));
            }
            binding.targetTypeKey = ProjectTypeConversion::tupleTypeKey(elementTypeKeys);
            aggregateInitializerBindingBySyntax_[expression] = aggregateInitializerBindings_.size();
            aggregateInitializerBindings_.push_back(std::move(binding));
            result.typeKey = ProjectTypeConversion::tupleTypeKey(elementTypeKeys);
            result.valid = valid;
            result.assignable = false;
            break;
        }
        case ast::AstNodeKind::WithExpression:
        {
            // T01-F07. `source with { A = x }` is an explicit whole-value copy followed by
            // textual updates. Source is analyzed before every update and exactly once. Until
            // T06 supplies per-field drop flags, only trivially copyable/destructible structs are
            // admitted; accepting an owning aggregate here would turn `with` into an implicit
            // aliasing copy and make exceptional cleanup ambiguous.
            const ExpressionResult source = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!source.valid) break;
            const std::optional<std::vector<std::string>> elements =
                aggregateElementTypeKeys(source.typeKey);
            const std::optional<std::vector<std::string>> names =
                aggregateElementNames(source.typeKey);
            if (!elements.has_value() || !names.has_value() || names->size() != elements->size() ||
                names->empty())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "a with expression requires a named struct value, not '" + source.typeKey + "'");
                break;
            }
            bool valid = true;
            if (!elementTypeHasTrivialLifetime(source.typeKey) ||
                source.ownership == sema::OwnershipKind::Owned || source.ownershipTransfer ||
                source.ownershipFreshOwner)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "with expression for '" + source.typeKey +
                        "' requires a trivially copyable value; move-only or owning values need "
                        "an explicit clone/copy strategy");
                valid = false;
            }

            MethodAggregateInitializerBinding binding;
            binding.syntax = expression;
            binding.targetTypeKey = source.typeKey;
            binding.generalized = true;
            std::vector<bool> assigned(elements->size(), false);
            for (const ast::AstNode* update : expression->children())
            {
                MethodAggregateInitializerEntryBinding entry;
                entry.syntax = update;
                const auto found = update != nullptr
                    ? std::ranges::find(*names, update->argumentName()) : names->end();
                if (update == nullptr || found == names->end())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::UnknownMember,
                        diagnostics::DiagnosticSeverity::Error,
                        update != nullptr ? update->range() : expression->range(),
                        "with source '" + source.typeKey + "' has no field named '" +
                            (update != nullptr ? update->argumentName() : std::string{}) + "'");
                    valid = false;
                    binding.entries.push_back(std::move(entry));
                    continue;
                }
                const std::size_t target =
                    static_cast<std::size_t>(std::distance(names->begin(), found));
                if (assigned[target])
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        update->range(),
                        "with member '" + update->argumentName() + "' is assigned more than once");
                    valid = false;
                }
                assigned[target] = true;
                entry.targetElementOrdinals.push_back(target);
                const ExpressionResult value = analyzeExpression(
                    update, methodMemberOrdinal, scopeOrdinal, diagnostics);
                if (!value.valid || !classifyImplicit(
                        value.typeKey, (*elements)[target], value.integerConstant).exists())
                {
                    if (value.valid)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidConversion,
                            diagnostics::DiagnosticSeverity::Error,
                            update->range(),
                            "with member '" + update->argumentName() + "' of type '" +
                                (*elements)[target] + "' cannot be updated from '" + value.typeKey + "'");
                    }
                    valid = false;
                }
                binding.entries.push_back(std::move(entry));
            }
            aggregateInitializerBindingBySyntax_[expression] = aggregateInitializerBindings_.size();
            aggregateInitializerBindings_.push_back(std::move(binding));
            result.typeKey = source.typeKey;
            result.valid = valid;
            result.assignable = false;
            break;
        }
        case ast::AstNodeKind::ConditionalExpression:
        {
            // `condition ? whenTrue : whenFalse`.
            //
            // EL TIPO. Las dos ramas deben tener el MISMO tipo, o ser ambas numericas -- en cuyo
            // caso el resultado es su tipo comun, la misma regla que usa un operador aritmetico.
            // C# ademas recorre una reticula de conversiones entre tipos referencia no
            // emparentados para encontrar una base comun; eso necesita el modelo de herencia y no
            // es lo que hace util al operador. Un par de ramas que esto no acepta recibe un
            // diagnostico que dice exactamente que dos tipos discreparon, nunca una eleccion
            // silenciosa de una de ellas.
            //
            // EL FLUJO (CC1-203). Las dos ramas se analizan CADA UNA desde el estado que hay tras
            // la condicion, y el estado posterior es la union de las dos. Antes se analizaban
            // secuencialmente, y eso hacia que `c ? (x = 1) : 0` diera `x` por definitivamente
            // asignada: definite assignment es un conjunto MUST cuyo join es una INTERSECCION, y
            // analizar en secuencia equivale a unir, que es la direccion insegura. El programa no
            // explotaba porque C7 pone a cero el almacenamiento del local -- devolvia 0. El backend
            // tapaba el bug del analisis.
            const ExpressionResult condition = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (condition.valid && condition.typeKey != "builtin:bool")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCondition,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->first()->range(),
                    "the condition of a conditional expression must have type bool");
            }

            const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
            ExpressionResult whenTrue;
            ExpressionResult whenFalse;
            const OwnershipFlowSnapshot trueState = analyzeBranchState(
                branchEntry,
                [&]
                {
                    whenTrue = analyzeExpression(
                        expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                });
            const OwnershipFlowSnapshot falseState = analyzeBranchState(
                branchEntry,
                [&]
                {
                    whenFalse = analyzeExpression(
                        expression->third(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                });
            commitBranchJoin(trueState, falseState);

            if (!whenTrue.valid || !whenFalse.valid)
            {
                break;
            }

            // CC1-204. OWNERSHIP DEL RESULTADO. Un ternario cuyas dos ramas devuelven `owned` ES
            // una expresion que transfiere propiedad, y hasta ahora se perdia: solo se copiaban
            // typeKey y valid, asi que `raii string r = c ? MakeA() : MakeB();` no podia
            // reconocerse como inicializacion valida de un RAII.
            //
            // La regla es la conservadora y la unica sana: el resultado solo conserva una propiedad
            // de ownership si AMBAS ramas coinciden en ella. Si difieren, el valor no tiene una
            // procedencia unica que un checker de lifetime pueda seguir, y decir que si la tiene
            // seria peor que no decir nada. El origen concreto (local o parametro) solo se propaga
            // cuando las dos ramas nombran exactamente el mismo, por la misma razon.
            const auto mergeOwnership = [&]()
            {
                if (whenTrue.ownership != whenFalse.ownership) return;
                result.ownership = whenTrue.ownership;
                result.ownershipTransfer =
                    whenTrue.ownershipTransfer && whenFalse.ownershipTransfer;
                if (whenTrue.ownershipSourceLocalOrdinal == whenFalse.ownershipSourceLocalOrdinal)
                {
                    result.ownershipSourceLocalOrdinal = whenTrue.ownershipSourceLocalOrdinal;
                }
                if (whenTrue.ownershipSourceParameterOrdinal ==
                    whenFalse.ownershipSourceParameterOrdinal)
                {
                    result.ownershipSourceParameterOrdinal =
                        whenTrue.ownershipSourceParameterOrdinal;
                }
                if (whenTrue.borrowedLocalOrdinal == whenFalse.borrowedLocalOrdinal)
                {
                    result.borrowedLocalOrdinal = whenTrue.borrowedLocalOrdinal;
                }
            };

            if (whenTrue.typeKey == whenFalse.typeKey)
            {
                result.typeKey = whenTrue.typeKey;
                result.valid = true;
                mergeOwnership();
                break;
            }
            // A string literal already converts implicitly to the native string value everywhere
            // else (assignment, arguments and returns).  The conditional join must use that same
            // conversion instead of treating `string-literal` as an unrelated final type.  This
            // is deliberately narrow: it does not add the C# reference-type best-common-type
            // lattice described above.
            if ((whenTrue.typeKey == "string-literal" &&
                 whenFalse.typeKey == "builtin:string") ||
                (whenTrue.typeKey == "builtin:string" &&
                 whenFalse.typeKey == "string-literal"))
            {
                result.typeKey = "builtin:string";
                result.valid = true;
                mergeOwnership();
                break;
            }
            const std::optional<std::string> common = ProjectTypeConversion::commonNumericType(
                whenTrue.typeKey, whenFalse.typeKey, whenTrue.integerConstant, whenFalse.integerConstant);
            if (common.has_value())
            {
                result.typeKey = *common;
                result.valid = true;
                // Un tipo numerico comun implica que las dos ramas son valores numericos, que no
                // llevan ownership. No hay nada que fusionar.
                break;
            }
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "the branches of a conditional expression have incompatible types '" +
                    whenTrue.typeKey + "' and '" + whenFalse.typeKey + "'");
            break;
        }
        case ast::AstNodeKind::AssignmentExpression:
            result = analyzeAssignment(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        case ast::AstNodeKind::TypeTestExpression:
        case ast::AstNodeKind::SafeCastExpression:
        {
            const ExpressionResult operand = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (memberModel_ == nullptr || nameLookup_ == nullptr)
            {
                break;
            }
            const std::string targetTypeKey =
                memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
            const std::optional<std::size_t> targetType =
                referenceTypeOrdinalForTypeKey(targetTypeKey);
            const bool sourceIsReference = operand.typeKey == "builtin:null" ||
                isReferenceTypeKey(operand.typeKey);
            if (!operand.valid || isErrorType(targetTypeKey))
            {
                break;
            }
            if (!targetType.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    std::string(expression->kind() == ast::AstNodeKind::TypeTestExpression ? "is" : "as") +
                        " target must be a class or interface reference type");
                break;
            }
            if (!sourceIsReference || !canRuntimeReferenceConvert(operand.typeKey, targetTypeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "no runtime reference conversion exists from '" + operand.typeKey +
                        "' to '" + targetTypeKey + "'");
                break;
            }

            result.typeKey = expression->kind() == ast::AstNodeKind::TypeTestExpression
                ? "builtin:bool"
                : targetTypeKey;
            result.valid = true;
            if (expression->kind() == ast::AstNodeKind::SafeCastExpression)
            {
                result.objectLifetimeOwnership = operand.objectLifetimeOwnership;
                result.storageOwnership = operand.storageOwnership;
            }
            runtimeTypeBindings_.push_back(MethodRuntimeTypeBinding{
                expression->kind() == ast::AstNodeKind::TypeTestExpression
                    ? MethodRuntimeTypeOperationKind::TypeTest
                    : MethodRuntimeTypeOperationKind::SafeCast,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                operand.typeKey,
                targetTypeKey,
                *targetType,
                interfaceModel_ != nullptr && interfaceModel_->isInterface(*targetType)});
            break;
        }
        case ast::AstNodeKind::CastExpression:
        {
            const ExpressionResult operand = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (memberModel_ != nullptr && nameLookup_ != nullptr)
            {
                result.typeKey = memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
                result.valid = operand.valid && !isErrorType(result.typeKey);
                result.objectLifetimeOwnership = operand.objectLifetimeOwnership;
                result.storageOwnership = operand.storageOwnership;
                if (result.valid && inheritanceModel_ != nullptr)
                {
                    const ProjectConversionClassifier classifier(
                        *inheritanceModel_, interfaceModel_, memberModel_);
                    const ProjectImplicitConversion userConversion =
                        classifier.classifyExplicitUserDefined(operand.typeKey, result.typeKey);
                    if ((userConversion.kind == ProjectConversionKind::UserDefinedImplicit ||
                         userConversion.kind == ProjectConversionKind::UserDefinedExplicit) &&
                        userConversion.userDefinedMemberOrdinal.has_value())
                    {
                        const std::vector<ExpressionResult> arguments = {operand};
                        ExpressionResult converted = resolveSyntheticCall(
                            expression,
                            methodMemberOrdinal,
                            scopeOrdinal,
                            userConversion.kind == ProjectConversionKind::UserDefinedImplicit
                                ? "op_Implicit" : "op_Explicit",
                            {*userConversion.userDefinedMemberOrdinal},
                            arguments,
                            nullptr,
                            {},
                            std::nullopt,
                            false,
                            false,
                            diagnostics);
                        if (converted.valid) result = converted;
                        break;
                    }

                    // C1.1 "function-pointer round trip". A delegate type key is neither a class
                    // nor an interface, so before this existed NOTHING classified a cast that
                    // touched one: `(nint)fp`, `(Fn)raw` and `(Other)fp` all sailed through the
                    // frontend untyped and died in the backend as "Stage 34 local initialization
                    // failed" (measured). The round trip is now a first-class, explicitly
                    // classified conversion in BOTH directions, and the cases that are NOT part of
                    // it get a real diagnostic here instead of a backend crash:
                    //
                    //   extern C delegate <-> nint/nuint      address in, address out
                    //   extern C delegate <-> T* / void*      the GetProcAddress shape
                    //   extern C delegate <-> extern C delegate   explicit reinterpret between
                    //                                             two foreign signatures
                    //
                    // Why the reinterpret is EXPLICIT-only and never implicit: on Win64 there is
                    // exactly one native C calling convention, so the convention is fixed by
                    // `extern C` itself and is part of the type -- what distinguishes two extern C
                    // delegate types is their SIGNATURE, and reinterpreting one signature as
                    // another is precisely the unchecked act `wglGetProcAddress` forces on every
                    // caller. Requiring the cast keeps that act visible in the source.
                    //
                    // An ORDINARY delegate stays out of all of this: it is a 16-byte heap cell
                    // (code + receiver), not an address, so casting one to an integer or pointer
                    // would hand out the cell -- exactly the miscompile Stage 55.x measured on the
                    // creation side (a cell pointer handed to C, invoked, exit 5 for an answer of
                    // 42). It is rejected by name.
                    const bool sourceIsExternCFunctionPointer =
                        isExternCDelegateTypeKey(operand.typeKey);
                    const bool targetIsExternCFunctionPointer =
                        isExternCDelegateTypeKey(result.typeKey);
                    const bool sourceIsAddressLike = sourceIsExternCFunctionPointer ||
                        isRawPointerTypeKey(operand.typeKey) ||
                        isPointerSizedIntegralTypeKey(operand.typeKey);
                    const bool targetIsAddressLike = targetIsExternCFunctionPointer ||
                        isRawPointerTypeKey(result.typeKey) ||
                        isPointerSizedIntegralTypeKey(result.typeKey);
                    if (sourceIsExternCFunctionPointer || targetIsExternCFunctionPointer)
                    {
                        if (!sourceIsAddressLike || !targetIsAddressLike)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidConversion,
                                diagnostics::DiagnosticSeverity::Error,
                                expression->range(),
                                "an extern C function pointer converts only to and from another "
                                "extern C function pointer, a raw pointer or nint/nuint; cannot "
                                "convert '" + operand.typeKey + "' to '" + result.typeKey + "'");
                            result.valid = false;
                        }
                        break;
                    }
                    if (isAnyDelegateTypeKey(operand.typeKey) || isAnyDelegateTypeKey(result.typeKey))
                    {
                        if (operand.typeKey != result.typeKey &&
                            (sourceIsAddressLike || targetIsAddressLike))
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidConversion,
                                diagnostics::DiagnosticSeverity::Error,
                                expression->range(),
                                "an ordinary delegate is a code+receiver cell, not an address; "
                                "declare the type 'extern C delegate' to convert between '" +
                                    operand.typeKey + "' and '" + result.typeKey + "'");
                            result.valid = false;
                            break;
                        }
                    }

                    const bool sourceIsReference = operand.typeKey == "builtin:null" ||
                        isReferenceTypeKey(operand.typeKey);
                    const std::optional<std::size_t> targetType =
                        referenceTypeOrdinalForTypeKey(result.typeKey);
                    const bool targetIsReference = targetType.has_value();
                    if (sourceIsReference || targetIsReference)
                    {
                        if (!sourceIsReference || !targetIsReference ||
                            !canRuntimeReferenceConvert(operand.typeKey, result.typeKey))
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidConversion,
                                diagnostics::DiagnosticSeverity::Error,
                                expression->range(),
                                "invalid explicit reference conversion from '" + operand.typeKey +
                                    "' to '" + result.typeKey + "'");
                            result.valid = false;
                            break;
                        }

                        const ProjectImplicitConversion implicit = classifier.classifyImplicit(
                            operand.typeKey, result.typeKey, operand.integerConstant);
                        if (!implicit.exists() && operand.typeKey != "builtin:null")
                        {
                            runtimeTypeBindings_.push_back(MethodRuntimeTypeBinding{
                                MethodRuntimeTypeOperationKind::CheckedCast,
                                expression,
                                methodMemberOrdinal,
                                scopeOrdinal,
                                operand.typeKey,
                                result.typeKey,
                                *targetType,
                                interfaceModel_ != nullptr && interfaceModel_->isInterface(*targetType)});
                        }
                    }
                }
            }
            break;
        }
        case ast::AstNodeKind::FromEndExpression:
        {
            const ExpressionResult distance = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!distance.valid) break;
            if (!ProjectTypeConversion::isIntegral(distance.typeKey) ||
                distance.typeKey == "builtin:bool")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidIndex,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "a from-end index requires an integral distance");
                break;
            }
            if (distance.integerConstant.has_value() && distance.integerConstant->isNegative)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::ConstantIndexOutOfBounds,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "a from-end index distance cannot be negative");
                break;
            }
            result = distance;
            result.assignable = false;
            result.addressable = false;
            break;
        }
        case ast::AstNodeKind::RangeExpression:
        {
            bool valid = true;
            const auto checkEndpoint = [&](const ast::AstNode* endpoint)
            {
                if (endpoint == nullptr) return;
                const ExpressionResult value = analyzeExpression(
                    endpoint, methodMemberOrdinal, scopeOrdinal, diagnostics);
                if (!value.valid || !ProjectTypeConversion::isIntegral(value.typeKey) ||
                    value.typeKey == "builtin:bool")
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidIndex,
                        diagnostics::DiagnosticSeverity::Error,
                        endpoint->range(),
                        "a range endpoint requires an integral index");
                    valid = false;
                }
            };
            checkEndpoint(expression->first());
            checkEndpoint(expression->second());
            result.typeKey = "internal:range";
            result.valid = valid;
            break;
        }
        case ast::AstNodeKind::IndexExpression:
        {
            const ExpressionResult base = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            std::vector<ExpressionResult> indices;
            const auto& indexSyntaxes = expression->children();
            if (!indexSyntaxes.empty())
            {
                indices.reserve(indexSyntaxes.size());
                for (const ast::AstNode* indexSyntax : indexSyntaxes)
                {
                    indices.push_back(analyzeExpression(
                        indexSyntax, methodMemberOrdinal, scopeOrdinal, diagnostics));
                }
            }
            else if (expression->second() != nullptr)
            {
                indices.push_back(analyzeExpression(
                    expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics));
            }

            if (base.valid && ProjectTypeConversion::isNativeArray(base.typeKey))
            {
                const std::size_t arrayRank = ProjectTypeConversion::nativeArrayRank(base.typeKey);
                const ast::AstNode* singleIndex = !indexSyntaxes.empty()
                    ? indexSyntaxes.front() : expression->second();
                if (indices.size() == 1 && singleIndex != nullptr &&
                    singleIndex->kind() == ast::AstNodeKind::RangeExpression)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidIndex,
                        diagnostics::DiagnosticSeverity::Error,
                        singleIndex->range(),
                        "slicing a heap array cannot implicitly allocate another array; use a "
                        "Span<T>/ReadOnlySpan<T> Length/Slice view");
                    break;
                }
                bool validIndices = indices.size() == arrayRank;
                for (const ExpressionResult& index : indices)
                    validIndices = validIndices && index.valid &&
                        ProjectTypeConversion::isIntegral(index.typeKey) &&
                        index.typeKey != "builtin:bool";
                if (validIndices)
                {
                    const std::optional<std::string> element =
                        ProjectTypeConversion::nativeArrayElementType(base.typeKey);
                    if (element.has_value())
                    {
                        result.typeKey = *element;
                        result.valid = true;
                        result.assignable = true;
                        // An array element is real mutable storage. Item 329 made addressability
                        // explicit so bit fields could reject `&field`; preserve the long-standing
                        // `&array[index]` raw-pointer boundary as a separate property.
                        result.addressable = true;
                        coreBindings_.push_back(MethodCoreBinding{
                            arrayRank > 1
                                ? MethodCoreOperationKind::RangeArrayElement
                                : MethodCoreOperationKind::ArrayElement,
                            expression,
                            methodMemberOrdinal,
                            scopeOrdinal,
                            result.typeKey,
                            result.typeKey,
                            true, std::nullopt, std::nullopt});
                    }
                }
                if (!result.valid)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidIndex,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "native array indexing requires exactly one integral index per dimension");
                }
                break;
            }

            if (base.valid &&
                (ProjectTypeConversion::isNativeString(base.typeKey) ||
                 base.typeKey == "string-literal"))
            {
                const ast::AstNode* singleIndex = !indexSyntaxes.empty()
                    ? indexSyntaxes.front() : expression->second();
                if (indices.size() == 1 && singleIndex != nullptr &&
                    singleIndex->kind() == ast::AstNodeKind::RangeExpression)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidIndex,
                        diagnostics::DiagnosticSeverity::Error,
                        singleIndex->range(),
                        "slicing a native string cannot allocate implicitly; use StringView.Slice");
                    break;
                }
                if (indices.size() == 1 && indices.front().valid &&
                    ProjectTypeConversion::isIntegral(indices.front().typeKey))
                {
                    result.typeKey = "builtin:char";
                    result.valid = true;
                    result.assignable = false;
                    coreBindings_.push_back(MethodCoreBinding{
                        MethodCoreOperationKind::StringElement,
                        expression,
                        methodMemberOrdinal,
                        scopeOrdinal,
                        result.typeKey,
                        "builtin:char",
                        false, std::nullopt, std::nullopt});
                }
                else
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidIndex,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "native string indexing requires exactly one integral index");
                }
                break;
            }

            // C6 "struct members": an indexer is a pair of accessor METHODS, and a struct
            // instance method already works -- it takes the receiver's address, which is exactly
            // what an indexer on a value needs in order to be able to write through it. This used
            // to ask referenceTypeOrdinalForTypeKey, which answers only for classes and
            // interfaces, so `m[0]` on a struct fell through to the raw-pointer branch below and
            // was reported as "index expression requires an indexer reference or a pointer".
            // Matrix and vector types -- the whole reason a game framework has structs -- are
            // indexed constantly.
            const std::optional<std::size_t> receiverType = base.valid
                ? accessorReceiverTypeOrdinalForTypeKey(base.typeKey) : std::nullopt;
            if (receiverType.has_value())
            {
                const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
                if (caller == nullptr) break;
                const std::size_t accessingTypeOrdinal = caller->declaringTypeOrdinal;
                const std::vector<std::size_t> getters = stage38AccessorCandidates(
                    *receiverType, "get_Item", false, true, false,
                    accessingTypeOrdinal, *receiverType);
                if (!getters.empty())
                {
                    const bool interfaceCall = interfaceModel_ != nullptr &&
                        interfaceModel_->isInterface(*receiverType);
                    result = resolveSyntheticCall(
                        expression,
                        methodMemberOrdinal,
                        scopeOrdinal,
                        "get_Item",
                        getters,
                        indices,
                        expression->first(),
                        base.typeKey,
                        receiverType,
                        false,
                        interfaceCall,
                        diagnostics,
                        true,
                        base.readonlyReceiver);
                    const std::vector<std::size_t> setters = stage38AccessorCandidates(
                        *receiverType, "set_Item", false, false, false,
                        accessingTypeOrdinal, *receiverType);
                    const bool referenceGetter = result.addressable;
                    result.assignable = result.valid && !base.readonlyReceiver &&
                        (referenceGetter || !setters.empty());
                    result.addressable = result.valid && !base.readonlyReceiver && referenceGetter;
                    result.readonlyReceiver = base.readonlyReceiver;
                    result.receiverSyntax = expression->first();
                    result.receiverTypeKey = base.typeKey;
                    result.receiverTypeOrdinal = receiverType;
                    break;
                }
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidIndex,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "reference type has no accessible Stage 38 indexer getter");
                break;
            }

            // C2.1 "fixed arrays". Two bases share one rule here, because their address
            // arithmetic is the same and neither carries a length: a FIXED ARRAY (`float[4]`,
            // inline storage inside a struct or a local) and a RAW POINTER (`int*`). Both are the
            // language's raw tier -- no bounds check, no null check, C semantics by design, the
            // same tier `*p` and `&x` already live in -- and both are what a C header's
            // `float color[4]` and `float* data` actually mean.
            //
            // MEASURED before this round: the fixed-array TYPE parsed and laid out correctly
            // (`float[4]` at offset 0, size 16) but `c.rgba[i]` was rejected with "index
            // expression requires an indexer reference or a pointer with one integral index" --
            // and the pointer half of that very message was ALSO a lie: `p[0]` passed this check
            // and then died in HIR with "Stage 38 OOP lowering requires class/interface index
            // expressions to resolve to an indexer accessor". Neither shape had a lowering.
            const std::optional<std::string> fixedElementType = memberModel_ != nullptr
                ? ProjectMemberModel::fixedArrayElementTypeKey(base.typeKey)
                : std::nullopt;
            const std::optional<std::string> flexibleElementType = memberModel_ != nullptr
                ? ProjectMemberModel::flexibleArrayElementTypeKey(base.typeKey)
                : std::nullopt;
            const bool rawPointerBase = ProjectTypeConversion::isPointer(base.typeKey);
            bool validFixedIndices = base.valid && fixedElementType.has_value() && !indices.empty();
            std::string indexedType = base.typeKey;
            if (validFixedIndices)
            {
                for (std::size_t dimensionOrdinal = 0;
                     dimensionOrdinal < indices.size(); ++dimensionOrdinal)
                {
                    const ExpressionResult& index = indices[dimensionOrdinal];
                    const std::optional<std::string> element =
                        ProjectMemberModel::fixedArrayElementTypeKey(indexedType);
                    const std::optional<std::uint64_t> count =
                        ProjectMemberModel::fixedArrayCount(indexedType);
                    if (!index.valid || !ProjectTypeConversion::isIntegral(index.typeKey) ||
                        !element.has_value() || !count.has_value())
                    {
                        validFixedIndices = false;
                        break;
                    }
                    if (index.integerConstant.has_value() &&
                        (index.integerConstant->isNegative || index.integerConstant->value >= *count))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::ConstantIndexOutOfBounds,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->children().empty()
                                ? expression->range()
                                : expression->children()[dimensionOrdinal]->range(),
                            "constant fixed-array index is outside [0, " +
                                std::to_string(*count) + ")");
                    }
                    indexedType = *element;
                }
            }

            const MethodLocalSymbol* variableArrayLocal = nullptr;
            if (base.valid && expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const std::optional<ScopeName> baseName = lookupScopeName(
                    scopeOrdinal, expression->first()->text());
                if (baseName.has_value() && baseName->kind == ScopeNameKind::Local &&
                    baseName->semanticOrdinal < locals_.size() &&
                    locals_[baseName->semanticOrdinal].isVariableLengthArray)
                {
                    variableArrayLocal = &locals_[baseName->semanticOrdinal];
                }
            }

            if (variableArrayLocal != nullptr &&
                variableArrayLocal->variableLengthArrayExtentSyntaxes.size() > 1)
            {
                bool validIndices =
                    indices.size() == variableArrayLocal->variableLengthArrayExtentSyntaxes.size();
                for (const ExpressionResult& index : indices)
                {
                    validIndices = validIndices && index.valid &&
                        ProjectTypeConversion::isIntegral(index.typeKey) &&
                        index.typeKey != "builtin:bool";
                }
                if (!validIndices)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidIndex,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "multidimensional VLA indexing requires one integral index per dimension");
                    break;
                }

                result.typeKey = variableArrayLocal->variableLengthArrayElementTypeKey;
                result.valid = true;
                result.assignable = true;
                result.addressable = true;
                MethodCoreBinding binding;
                binding.kind = MethodCoreOperationKind::RawElement;
                binding.syntax = expression;
                binding.methodMemberOrdinal = methodMemberOrdinal;
                binding.scopeOrdinal = scopeOrdinal;
                binding.resultTypeKey = result.typeKey;
                binding.elementTypeKey = result.typeKey;
                binding.assignable = true;
                coreBindings_.push_back(std::move(binding));
                variableArrayIndexBindings_.push_back(MethodVariableArrayIndexBinding{
                    expression,
                    variableArrayLocal->variableLengthArrayExtentLocalOrdinals});
                break;
            }
            const bool validPointerIndex = base.valid && rawPointerBase && indices.size() == 1 &&
                indices.front().valid && ProjectTypeConversion::isIntegral(indices.front().typeKey);
            const bool validFlexibleIndex = base.valid && flexibleElementType.has_value() &&
                indices.size() == 1 && indices.front().valid &&
                ProjectTypeConversion::isIntegral(indices.front().typeKey) &&
                indices.front().typeKey != "builtin:bool";
            if (validFixedIndices || validFlexibleIndex || validPointerIndex)
            {
                result.typeKey = validFixedIndices
                    ? indexedType
                    : validFlexibleIndex
                        ? *flexibleElementType
                        : std::string(base.typeKey.substr(0, base.typeKey.size() - 1));
                result.valid = true;
                const bool readonlyInlineElement = base.readonlyReceiver &&
                    (validFixedIndices || validFlexibleIndex);
                result.assignable = !readonlyInlineElement &&
                    !ProjectMemberModel::fixedArrayElementTypeKey(result.typeKey).has_value();
                result.addressable = true;
                result.readonlyReceiver = readonlyInlineElement;
                MethodCoreBinding binding;
                binding.kind = MethodCoreOperationKind::RawElement;
                binding.syntax = expression;
                binding.methodMemberOrdinal = methodMemberOrdinal;
                binding.scopeOrdinal = scopeOrdinal;
                binding.resultTypeKey = result.typeKey;
                binding.elementTypeKey = result.typeKey;
                binding.assignable = result.assignable;
                coreBindings_.push_back(std::move(binding));
            }
            else if (base.valid)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidIndex,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "fixed-array indices must be integral and cannot exceed its rank; raw pointers require exactly one integral index");
            }
            break;
        }
        // CC1 fase 4A -- `sizeof`. Tres cosas cambian aqui respecto a lo que habia, y las tres
        // eran defectos reales:
        //
        // 1. TIPO DEL RESULTADO: `builtin:nuint`, no `builtin:ulong`.
        //    Aviso sobre la justificacion: el documento de origen dice que hay que unificar con
        //    "el pipeline clasico", que ya bajaria `sizeof` a una constante `nuint`. ESO NO EXISTE
        //    EN ESTE ARBOL -- barrido completo: `sizeof` solo aparece en el lexer, en el parser y
        //    aqui; no hay caso en Sema ni en el HIR clasico, ni un solo test. No hay con que
        //    comparar, asi que la razon no puede ser esa. La razon es propia: `sizeof` produce una
        //    CUENTA DE BYTES, y el tipo sin signo del ancho del puntero es lo que consumen la
        //    aritmetica de punteros y `stackalloc`; darle `ulong` obligaria a una conversion en
        //    cada uso natural y ademas mentiria en un target de 32 bits.
        //
        // 2. OPERANDO NO EVALUADO. `sizeof(expression)` mide el TIPO de su operando; el operando no
        //    se ejecuta. Antes se analizaba como codigo normal, asi que `sizeof(x = 1)` marcaba `x`
        //    como asignada, `sizeof(move(o))` movia el propietario, y un borrow dentro del operando
        //    sobrevivia. El operando se sigue analizando -- hay que resolver nombres, miembros y
        //    tipos para saber QUE se esta midiendo, y sus errores deben seguir apareciendo -- pero
        //    el estado de flujo se captura antes y se restaura despues, de modo que la propiedad
        //    "el estado de flujo antes y despues es identico" se cumple POR CONSTRUCCION y no por
        //    enumerar cada efecto que habria que suprimir. OwnershipFlowSnapshot ya lleva las tres
        //    familias que importan: moves (must y may), borrows y `assignedLocals` (definite
        //    assignment). Es la misma maquinaria que usan las ramas de `?:` y `&&`/`||`.
        //    Los bindings que el analisis del operando deje registrados quedan huerfanos y son
        //    inertes: el HIR los indexa POR NODO DE SINTAXIS y solo los consulta al bajar ese nodo,
        //    y el operando de un `sizeof` no se baja nunca -- se baja una constante.
        //
        // 3. VALIDACION. `SizeofTypeExpression` no tocaba siquiera su `typeSyntax()`: `sizeof(void)`
        //    y `sizeof(TipoQueNoExiste)` pasaban el analisis semantico sin una palabra y morian
        //    despues en el `default` del HIR con "unsupported Stage 34 OOP expression kind". Ahora
        //    se resuelve el tipo aqui y se rechaza lo que no tiene tamano, con DiagnosticId::
        //    InvalidSizeof -- que estaba declarado desde hacia tiempo y no se usaba en ningun sitio.
        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::SizeofExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::AlignofExpression:
        {
            const bool alignmentQuery =
                expression->kind() == ast::AstNodeKind::AlignofTypeExpression ||
                expression->kind() == ast::AstNodeKind::AlignofExpression;
            std::string measuredTypeKey;
            if (expression->kind() == ast::AstNodeKind::SizeofExpression ||
                expression->kind() == ast::AstNodeKind::AlignofExpression)
            {
                const OwnershipFlowSnapshot beforeOperand = captureOwnershipFlow();
                const ExpressionResult operand = analyzeExpression(
                    expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                    /*allowMethodGroup=*/true);
                restoreOwnershipFlow(beforeOperand);
                if (!operand.valid && !operand.methodGroup)
                {
                    break;
                }
                if (operand.methodGroup)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidSizeof,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                    alignmentQuery
                        ? "alignof requires a value or a type; a method group has no alignment"
                        : "sizeof requires a value or a type; a method group has no size");
                    break;
                }
                measuredTypeKey = operand.typeKey;
            }
            else
            {
                if (memberModel_ == nullptr || nameLookup_ == nullptr)
                {
                    break;
                }
                measuredTypeKey = memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
                if (isErrorType(measuredTypeKey))
                {
                    break;
                }
            }

            if (measuredTypeKey == "builtin:void")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidSizeof,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    alignmentQuery ? "alignof(void) has no alignment" : "sizeof(void) has no size");
                break;
            }

            MethodCoreBinding binding;
            binding.kind = alignmentQuery ? MethodCoreOperationKind::AlignofConstant
                                          : MethodCoreOperationKind::SizeofConstant;
            binding.syntax = expression;
            binding.methodMemberOrdinal = methodMemberOrdinal;
            binding.scopeOrdinal = scopeOrdinal;
            binding.resultTypeKey = "builtin:nuint";
            // El tipo MEDIDO, no el tamano. Ver el comentario de SizeofConstant.
            binding.elementTypeKey = measuredTypeKey;
            coreBindings_.push_back(std::move(binding));

            result.typeKey = "builtin:nuint";
            result.valid = true;
            break;
        }
        case ast::AstNodeKind::OffsetofExpression:
        {
            if (memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr)
            {
                break;
            }
            const std::string measuredTypeKey =
                memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
            if (isErrorType(measuredTypeKey)) break;
            const std::optional<std::size_t> typeOrdinal =
                inheritanceModel_->typeOrdinalForTypeKey(measuredTypeKey);
            if (!typeOrdinal.has_value() || *typeOrdinal >= nameLookup_->namedTypes().size() ||
                nameLookup_->namedTypes()[*typeOrdinal].kind != NamedTypeDeclarationKind::Struct)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOffsetof,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "offsetof requires a complete struct or union type");
                break;
            }

            std::optional<std::size_t> selectedField;
            for (const std::size_t candidate : memberModel_->findMembers(*typeOrdinal, expression->text()))
            {
                if (candidate >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& member = memberModel_->members()[candidate];
                if (member.kind == ClassMemberKind::Field && !member.isStatic)
                {
                    selectedField = candidate;
                    break;
                }
            }
            if (!selectedField.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOffsetof,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "type '" + measuredTypeKey + "' has no instance field named '" +
                        expression->text() + "'");
                break;
            }

            MethodCoreBinding binding;
            binding.kind = MethodCoreOperationKind::OffsetofConstant;
            binding.syntax = expression;
            binding.methodMemberOrdinal = methodMemberOrdinal;
            binding.scopeOrdinal = scopeOrdinal;
            binding.resultTypeKey = "builtin:nuint";
            binding.elementTypeKey = measuredTypeKey;
            binding.implicitFieldMemberOrdinal = selectedField;
            coreBindings_.push_back(std::move(binding));

            result.typeKey = "builtin:nuint";
            result.valid = true;
            break;
        }
        // Stage 52: default(T) -- the zero-equivalent value of T (0/false/'\0' for a numeric
        // primitive, null for a reference type or string, an all-zero-fields instance for a
        // value-typed struct). This is what lets a generic method give an honest "nothing found"
        // result (List<T>.Find's "return default(T);" when no element matches) that works
        // uniformly across every T the method gets instantiated with, exactly like real C#.
        // Treated like NullLiteralExpression for ownership purposes -- it names no existing
        // owner anywhere, so it needs no move()/ownership-transfer ceremony to flow into an
        // owned-typed return, field, or parameter.
        case ast::AstNodeKind::DefaultValueExpression:
            if (memberModel_ != nullptr && nameLookup_ != nullptr)
            {
                const std::string typeKey = memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
                if (isErrorType(typeKey))
                {
                    break;
                }
                if (typeKey == "builtin:void")
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "default(void) is not a valid expression");
                    break;
                }
                result.typeKey = typeKey;
                result.valid = true;
            }
            break;
        case ast::AstNodeKind::ObjectCreationExpression:
            if (memberModel_ != nullptr && nameLookup_ != nullptr)
            {
                const std::string objectType = memberModel_->canonicalTypeKey(
                    expression->typeSyntax(), *nameLookup_);
                bool argumentsValid = true;
                for (const ast::AstNode* argument : expression->children())
                {
                    // NSC-LIMIT-010: los argumentos de un constructor se analizan CON
                    // `allowMove`, igual que los de una llamada ordinaria.
                    //
                    // Hasta el 2026-09-04 se analizaban sin el, y aqui habia un comentario que
                    // explicaba el hueco: ProjectConstructionModel elegia constructor mirando solo
                    // tipos y conversiones fisicas, sin validacion de propiedad, asi que admitir
                    // `move(...)` habria producido una transferencia que nadie comprobaba. El
                    // hueco esta cerrado: la regla vive en classifyOwnershipArgument y
                    // bindConstruction la aplica sobre los mismos datos.
                    //
                    // Sin `allowMove` no basta con que el constructor exista: `move(x)` como
                    // argumento se rechazaba ANTES de llegar a la resolucion, y el lowering de HIR
                    // habria muerto despues en «move expression has no semantic move binding»,
                    // porque el binding se crea aqui o no se crea.
                    const ExpressionResult argumentResult = analyzeExpression(
                        argument, methodMemberOrdinal, scopeOrdinal, diagnostics,
                        /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
                    argumentsValid = argumentsValid && argumentResult.valid && !argumentResult.methodGroup;
                }
                result.typeKey = objectType;
                result.valid = argumentsValid && !isErrorType(objectType);
                // NSC-LIMIT-010: `new T(...)` sobre un tipo con almacenamiento propio produce un
                // OWNER RECIEN CREADO. No se marca transferencia --no hay fuente con nombre-- sino
                // el tercer estado, que solo relaja el contrato de un parametro `owned`. Un
                // parametro sin calificador sigue viendo exactamente lo que veia antes.
                result.ownershipFreshOwner = result.valid && typeKeyIsOwnable(objectType);
                if (result.valid)
                {
                    result.objectLifetimeOwnership = sema::OwnershipKind::Owned;
                    result.storageOwnership = sema::OwnershipKind::Owned;
                }
            }
            break;
        case ast::AstNodeKind::PlacementConstructionExpression:
            if (memberModel_ != nullptr && nameLookup_ != nullptr)
            {
                const std::string targetType = memberModel_->canonicalTypeKey(
                    expression->typeSyntax(), *nameLookup_);
                const NamedTypeDeclaration* target =
                    nameLookup_->resolvedDeclaration(expression->typeSyntax());
                const bool constructible = target != nullptr &&
                    (target->kind == NamedTypeDeclarationKind::Class ||
                     target->kind == NamedTypeDeclarationKind::Struct) &&
                    expression->typeSyntax() != nullptr &&
                    expression->typeSyntax()->modifiers().empty();
                if (!constructible)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidAllocationType,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "placement construction requires an unmodified class or struct type");
                }

                const ExpressionResult storage = analyzeExpression(
                    expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                const std::string requiredStorageType = targetType + "*";
                bool storageValid = storage.valid && !storage.methodGroup;
                if (storageValid && storage.typeKey != requiredStorageType)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidConversion,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->first()->range(),
                        "placement construction of '" + targetType +
                            "' requires destination storage of exact type '" +
                            requiredStorageType + "'");
                    storageValid = false;
                }

                bool argumentsValid = true;
                for (const ast::AstNode* argument : expression->children())
                {
                    const ExpressionResult argumentResult = analyzeExpression(
                        argument, methodMemberOrdinal, scopeOrdinal, diagnostics,
                        /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
                    argumentsValid = argumentsValid && argumentResult.valid &&
                        !argumentResult.methodGroup;
                }
                // A placed struct is exposed through the raw T* that names its supplied storage.
                // A class expression keeps NativeSharp's ordinary class-reference type even
                // though its placement operand is T*: physically both are the object address,
                // while logically this lets `placed.Member` use the class contract instead of
                // treating the address as a pointer-to-reference slot.
                result.typeKey = target != nullptr &&
                        target->kind == NamedTypeDeclarationKind::Class
                    ? targetType
                    : requiredStorageType;
                result.valid = constructible && storageValid && argumentsValid &&
                    !isErrorType(targetType);
                if (result.valid)
                {
                    result.objectLifetimeOwnership = sema::OwnershipKind::Owned;
                    result.storageOwnership = sema::OwnershipKind::Borrowed;
                }
            }
            break;
        case ast::AstNodeKind::NewExpression:
        case ast::AstNodeKind::StackallocExpression:
            if (memberModel_ != nullptr && nameLookup_ != nullptr)
            {
                std::string element = memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
                ExpressionResult count;
                if (expression->first() != nullptr)
                {
                    count = analyzeExpression(
                        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                }

                const bool dynamicArrayNew = expression->kind() == ast::AstNodeKind::NewExpression &&
                    expression->flag();
                if (dynamicArrayNew)
                {
                    const bool rangeArray = expression->auxiliaryText().starts_with(
                        "range-array-allocation:");
                    if (rangeArray)
                    {
                        std::vector<const ast::AstNode*> dimensions;
                        dimensions.push_back(expression->first());
                        dimensions.insert(dimensions.end(), expression->children().begin(),
                            expression->children().end());
                        bool dimensionsValid = dimensions.size() >= 2;
                        for (const ast::AstNode* dimension : dimensions)
                        {
                            const ExpressionResult extent = analyzeExpression(
                                dimension, methodMemberOrdinal, scopeOrdinal, diagnostics);
                            if (!extent.valid || !ProjectTypeConversion::isIntegral(extent.typeKey) ||
                                extent.typeKey == "builtin:bool")
                            {
                                diagnostics.report(
                                    diagnostics::DiagnosticId::InvalidArrayCount,
                                    diagnostics::DiagnosticSeverity::Error,
                                    dimension->range(),
                                    "rectangular array dimensions must be integral");
                                dimensionsValid = false;
                            }
                        }
                        if (element == "builtin:void")
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidAllocationType,
                                diagnostics::DiagnosticSeverity::Error,
                                expression->range(),
                                "rectangular arrays cannot have void elements");
                            dimensionsValid = false;
                        }
                        result.typeKey = element + '[' +
                            std::string(dimensions.size() - 1, ',') + ']';
                        result.valid = dimensionsValid && !isErrorType(element);
                        if (result.valid)
                        {
                            result.storageOwnership = sema::OwnershipKind::Owned;
                            coreBindings_.push_back(MethodCoreBinding{
                                MethodCoreOperationKind::RangeArrayAllocation,
                                expression,
                                methodMemberOrdinal,
                                scopeOrdinal,
                                result.typeKey,
                                element,
                                false, std::nullopt, std::nullopt});
                        }
                        break;
                    }
                    const bool inferredInitializer =
                        expression->auxiliaryText() == "inferred-array-initializer";
                    bool countValid = inferredInitializer
                        ? expression->first() == nullptr
                        : count.valid && ProjectTypeConversion::isIntegral(count.typeKey);
                    if (inferredInitializer && expression->first() != nullptr)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidArrayCount,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            "an inferred native array initializer must omit the element count");
                        countValid = false;
                    }
                    if (!countValid)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidArrayCount,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            "native array allocation requires an integral element count");
                    }
                    if (element == "builtin:void")
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidAllocationType,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            "native arrays cannot have void elements");
                    }
                    bool elementsValid = true;
                    if (inferredInitializer)
                    {
                        for (const ast::AstNode* initializer : expression->children())
                        {
                            const ExpressionResult value = analyzeExpression(
                                initializer, methodMemberOrdinal, scopeOrdinal, diagnostics,
                                /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
                            if (!value.valid || !classifyImplicit(
                                    value.typeKey, element, value.integerConstant).exists())
                            {
                                diagnostics.report(
                                    diagnostics::DiagnosticId::InvalidConversion,
                                    diagnostics::DiagnosticSeverity::Error,
                                    initializer->range(),
                                    "native array initializer element is not implicitly convertible to '" +
                                        element + "'");
                                elementsValid = false;
                            }
                        }
                    }
                    result.typeKey = element + "[]";
                    result.valid = !isErrorType(element) && element != "builtin:void" &&
                        countValid && elementsValid;
                    if (result.valid) result.storageOwnership = sema::OwnershipKind::Owned;
                    if (result.valid)
                    {
                        coreBindings_.push_back(MethodCoreBinding{
                            MethodCoreOperationKind::ArrayAllocation,
                            expression,
                            methodMemberOrdinal,
                            scopeOrdinal,
                            result.typeKey,
                            element,
                            false, std::nullopt, std::nullopt});
                    }
                    break;
                }

                const NamedTypeDeclaration* declared = nameLookup_->resolvedDeclaration(expression->typeSyntax());
                const bool genericRawNew = expression->auxiliaryText() == kGenericRawNewAuxiliary;
                const bool classNew = expression->kind() == ast::AstNodeKind::NewExpression &&
                    !genericRawNew && declared != nullptr &&
                    declared->kind == NamedTypeDeclarationKind::Class;
                if (classNew)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidAllocationType,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "class allocation requires object-construction syntax new C(args) in Stage 34");
                }

                // CC1 fases 4B y 4C. Hasta aqui llegan las dos formas que quedaban SIN VALIDAR y
                // sin binding: `stackalloc T[n]` y el `new T` escalar. Las dos ponian `T*`,
                // marcaban valido, y morian despues al bajar -- `stackalloc` en el `default` del
                // HIR y `new T` en un error que decia literalmente que el raw new "remains outside
                // this pipeline". Mitad delantera presente, feature ausente; la misma forma que
                // `sizeof` en la fase 4A.
                //
                // AVISO SOBRE EL TEXTO DE LA TAREA 47. Pedia "portar a ProjectMethodBodyModel las
                // validaciones de stackalloc DEL PIPELINE CLASICO". Barrido completo del arbol:
                // `stackalloc` solo aparece en el lexer, el parser, el AST y este caso. NO HAY
                // pipeline clasico del que portar nada, igual que no lo habia para `sizeof`. Las
                // validaciones de abajo estan ESCRITAS, no portadas, y cada una dice por que
                // existe. (DiagnosticId::InvalidStackalloc llevaba tiempo declarado y sin usar en
                // ningun sitio -- ahora se usa.)
                const bool isStackalloc = expression->kind() == ast::AstNodeKind::StackallocExpression;
                const bool isRawNew = expression->kind() == ast::AstNodeKind::NewExpression && !classNew;
                bool allocationValid = !isErrorType(element) && !classNew;

                if (isStackalloc || isRawNew)
                {
                    // Sin tamano no hay reserva posible, ni en la pila ni en el monton.
                    if (element == "builtin:void")
                    {
                        diagnostics.report(
                            isStackalloc ? diagnostics::DiagnosticId::InvalidStackalloc
                                         : diagnostics::DiagnosticId::InvalidAllocationType,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            isStackalloc ? "stackalloc requires an element type with a size; void has none"
                                         : "new requires an element type with a size; void has none");
                        allocationValid = false;
                    }

                    // Un elemento con destructor o con campos `owned` tiene un ciclo de vida que
                    // alguien debe cerrar. Ni la memoria cruda del monton ni un buffer de pila lo
                    // cierran: no hay nada que llame al destructor al salir del ambito, asi que el
                    // recurso se filtraria -- y en el caso de la pila, la memoria desaparece bajo
                    // los pies del objeto. Se prohibe en vez de inventar una semantica de
                    // destruccion nueva para el nivel crudo.
                    if (allocationValid && !elementTypeHasTrivialLifetime(element))
                    {
                        diagnostics.report(
                            isStackalloc ? diagnostics::DiagnosticId::InvalidStackalloc
                                         : diagnostics::DiagnosticId::InvalidAllocationType,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            std::string(isStackalloc ? "stackalloc" : "raw new") +
                                " requires an element type with no destructor and no owned fields; '" +
                                element + "' owns a lifetime that raw storage cannot discharge");
                        allocationValid = false;
                    }
                }

                if (isStackalloc)
                {
                    // El contador. La forma dinamica de `new T[n]` ya lo exige arriba; esta rama
                    // no lo hacia, asi que `stackalloc int[unaCadena]` tipaba sin una palabra.
                    if (!count.valid || !ProjectTypeConversion::isIntegral(count.typeKey) ||
                        count.typeKey == "builtin:bool")
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidStackalloc,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            "stackalloc requires an integral element count");
                        allocationValid = false;
                    }

                    // =============================================================================
                    // ID 123 -- ENDURECIMIENTO DE `stackalloc`.
                    //
                    // Lo que ya se comprobaba antes de esta ronda: que el elemento tenga tamano
                    // (no `void`), que su vida sea trivial (sin destructor y sin campos `owned`) y
                    // que el contador sea integral. Nada mas. Un `stackalloc` con un contador
                    // NEGATIVO, o con uno tan grande que la reserva no cabe en la pila, pasaba
                    // entero y llegaba a LLVM como un `alloca` que revienta en ejecucion.
                    //
                    // Las cuatro comprobaciones NO CABEN TODAS AQUI, y el reparto no es una
                    // preferencia: dos necesitan el TAMANO DEL ELEMENTO, y este modelo no lo
                    // tiene. El orden de construccion del pipeline es modelo de cuerpos de metodo
                    // PRIMERO y ProjectObjectLayoutModel DESPUES -- `build()` ni siquiera recibe
                    // el modelo de layout, y esta escrito en el comentario de SizeofConstant en
                    // ProjectMethodBodyModel.h por la misma razon. Duplicar aqui las reglas de
                    // tamano seria justo la duplicacion que el milestone CC1 existe para quitar.
                    //
                    // Reparto, entonces:
                    //   AQUI (necesitan solo el contador): contador constante negativo, y
                    //        `stackalloc` dentro de un bucle.
                    //   EN ProjectOopHirModel (necesitan el tamano): que la reserva total quepa.
                    //
                    // Lo que se gana teniendo estas dos aqui: salen por el motor de diagnosticos,
                    // con el rango del codigo fuente y el texto que lee la persona. Las del HIR
                    // salen por su cadena `error`, que habla de etapas internas -- es la
                    // limitacion de esa capa, no una eleccion.
                    // =============================================================================

                    // ID 123 (1/3). CONTADOR CONSTANTE NEGATIVO.
                    //
                    // Se puede comprobar desde que el ID 119 cerro la constancia de los literales
                    // negativos: antes de aquello `-1` ni siquiera llegaba aqui como constante, y
                    // por eso esta comprobacion no se pudo escribir en su momento.
                    //
                    // Sobre el tipo, no sobre los bits: un contador `uint` con el bit alto puesto
                    // NO es negativo, y leerlo como si lo fuera rechazaria `stackalloc byte[n]`
                    // con n = 3000000000u, que es perfectamente legitimo escribirlo aunque luego
                    // no quepa -- de que no quepa se encarga la comprobacion del HIR.
                    if (allocationValid && expression->first() != nullptr)
                    {
                        // `declaringTypeOrdinal` va en nullopt: ese parametro solo se usa
                        // mientras se expande el inicializador de un campo `const`, y el
                        // contador de un `stackalloc` esta en el cuerpo de un metodo. Es lo
                        // mismo que pasa el plegado de las etiquetas de `switch`.
                        const std::optional<ProjectIntegerValue> foldedCount =
                            foldIntegerConstant(expression->first(), std::nullopt);
                        if (foldedCount.has_value() &&
                            ProjectIntegerEvaluator::isSignedInteger(foldedCount->typeKey) &&
                            foldedCount->asSigned() < 0)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidStackalloc,
                                diagnostics::DiagnosticSeverity::Error,
                                expression->range(),
                                "stackalloc element count is negative (" +
                                    std::to_string(foldedCount->asSigned()) +
                                    "); a reservation cannot have fewer than zero elements");
                            allocationValid = false;
                        }
                    }

                    // ID 123 (2/3). `stackalloc` DENTRO DE UN BUCLE.
                    //
                    // Es la trampa clasica de C y merece decirse: un `alloca` dentro de un bucle
                    // reserva OTRA VEZ en cada vuelta y NADA se libera hasta salir de la funcion.
                    // La pila crece con las iteraciones, no con la profundidad de la llamada. Un
                    // bucle de mil vueltas con `stackalloc int[256]` son un mega de pila, y el
                    // programa parece correcto leyendolo.
                    //
                    // AVISO Y NO ERROR, y es deliberado: la forma es legitima cuando el bucle da
                    // pocas vueltas, y este lenguaje no tiene con que demostrar cuantas da. Un
                    // error aqui prohibiria codigo correcto; callarse deja pasar un desbordamiento
                    // de pila con forma de programa sano. El aviso es lo unico honesto.
                    //
                    // ES EL PRIMER WARNING DEL COMPILADOR: `DiagnosticSeverity::Warning` existia y
                    // `DiagnosticEngine::warningCount()` tambien, pero ningun sitio del arbol los
                    // usaba (barrido completo: cero apariciones de `DiagnosticSeverity::Warning`
                    // en src/). Comprobado que no rompe nada: el exito se decide con
                    // `hasErrors()`, que solo cuenta los de severidad Error.
                    if (allocationValid && !loopStack_.empty())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidStackalloc,
                            diagnostics::DiagnosticSeverity::Warning,
                            expression->range(),
                            "stackalloc inside a loop reserves again on every iteration and "
                            "nothing is released until the function returns; hoist it out of the "
                            "loop or use a heap allocation");
                    }
                }

                result.typeKey = element + '*';
                result.valid = allocationValid;
                if (result.valid) result.storageOwnership = sema::OwnershipKind::Owned;

                if (allocationValid && (isStackalloc || isRawNew))
                {
                    MethodCoreBinding binding;
                    binding.kind = isStackalloc ? MethodCoreOperationKind::StackAllocation
                                                : MethodCoreOperationKind::RawAllocation;
                    binding.syntax = expression;
                    binding.methodMemberOrdinal = methodMemberOrdinal;
                    binding.scopeOrdinal = scopeOrdinal;
                    binding.resultTypeKey = result.typeKey;
                    binding.elementTypeKey = element;
                    coreBindings_.push_back(std::move(binding));
                }
            }
            break;
        case ast::AstNodeKind::DeleteExpression:
        {
            bool deletesRaiiLocal = false;
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const std::optional<ScopeName> target = lookupScopeName(scopeOrdinal, expression->first()->text());
                deletesRaiiLocal = target.has_value() && target->kind == ScopeNameKind::Local &&
                    target->semanticOrdinal < locals_.size() && locals_[target->semanticOrdinal].isRaii;
                if (deletesRaiiLocal)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "explicit delete of a RAII local is forbidden; lexical cleanup owns destruction");
                }
            }
            const ExpressionResult operand = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            const ClassMemberSymbol* context = methodSymbol(methodMemberOrdinal);
            const bool deletesCurrentThis = context != nullptr &&
                context->kind == ClassMemberKind::Destructor &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::ThisExpression;
            if (deletesCurrentThis)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "delete this is forbidden inside a destructor; the generated destruction entry owns final deallocation");
            }
            // Bloque 1 (audit finding "delete holder.slot"): explicit delete of an `owned` FIELD
            // is forbidden, for the same reason delete of a RAII local is forbidden two checks
            // above -- the storage's own discharge machinery owns destruction. Measured before
            // this rule: `delete h.slot` destroyed the object AND left the slot's pointer intact,
            // so the container's destruction entry released the same allocation again -- 2
            // destructions of 1 object, -O0 and -O2 alike. A double free the compiler itself
            // guaranteed.
            //
            // Prohibition, not auto-nulling: clearing the slot as a side effect of `delete` would
            // couple delete to the identity of its operand lvalue (delete via any alias would
            // still double-free), and the language already has the honest spellings for every
            // intent -- `h.slot = null` to clear (releases exactly once, verified), overwrite to
            // replace, move-out to take. The audit reached the same conclusion: prohibiting is
            // simpler and safer than making a destruction operator silently mutate its operand.
            //
            // Scope: only owned fields that OWN storage (reference type or native string).
            // `owned T[]` array fields stay deletable -- `delete items[i]` / `delete[] items` is
            // the manual array model ~List() is built on, and an array element read never carries
            // the field's member ordinal anyway.
            bool deletesOwnedField = false;
            if (operand.valid && operand.assignmentMemberOrdinal.has_value() &&
                memberModel_ != nullptr &&
                *operand.assignmentMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& deletedMember =
                    memberModel_->members()[*operand.assignmentMemberOrdinal];
                deletesOwnedField = deletedMember.kind == ClassMemberKind::Field &&
                    !deletedMember.isStatic &&
                    deletedMember.declaredOwnership == sema::OwnershipKind::Owned &&
                    (isReferenceTypeKey(deletedMember.declaredTypeKey) ||
                     deletedMember.declaredTypeKey == "builtin:string");
                if (deletesOwnedField)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "explicit delete of owned field '" + deletedMember.name +
                            "' is forbidden; the container releases it on destruction -- clear it "
                            "with '= null', replace it, or take it with move(...)");
                }
            }
            result.typeKey = "builtin:void";
            result.valid = operand.valid && !deletesCurrentThis && !deletesRaiiLocal &&
                !deletesOwnedField;
            break;
        }
        case ast::AstNodeKind::DestroyExpression:
        {
            const ExpressionResult operand = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);

            bool validTarget = false;
            if (operand.valid)
            {
                // A class/interface reference carries the runtime descriptor needed for dynamic
                // destruction. A raw T* is accepted only when T is a concrete class or struct;
                // the latter currently has trivial destruction but still has a real lifetime to
                // end after placement construction.
                validTarget = isReferenceTypeKey(operand.typeKey);
                if (!validTarget && operand.typeKey.ends_with('*') && operand.typeKey.size() > 1)
                {
                    const std::string_view pointee(
                        operand.typeKey.data(), operand.typeKey.size() - 1);
                    const std::optional<std::size_t> ordinal = namedTypeOrdinalForTypeKey(pointee);
                    validTarget = ordinal.has_value() && nameLookup_ != nullptr &&
                        *ordinal < nameLookup_->namedTypes().size() &&
                        (inheritanceModel_->isClass(*ordinal) ||
                         nameLookup_->namedTypes()[*ordinal].kind == NamedTypeDeclarationKind::Struct);
                }
                if (!validTarget)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "destroy requires a class/interface reference or a pointer to a concrete class or struct");
                }
            }

            bool destroysRaiiLocal = false;
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const std::optional<ScopeName> target =
                    lookupScopeName(scopeOrdinal, expression->first()->text());
                destroysRaiiLocal = target.has_value() && target->kind == ScopeNameKind::Local &&
                    target->semanticOrdinal < locals_.size() && locals_[target->semanticOrdinal].isRaii;
                if (destroysRaiiLocal)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "explicit destroy of a RAII local is forbidden; lexical cleanup owns destruction");
                }
            }

            const ClassMemberSymbol* context = methodSymbol(methodMemberOrdinal);
            const bool destroysCurrentThis = context != nullptr &&
                context->kind == ClassMemberKind::Destructor &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::ThisExpression;
            if (destroysCurrentThis)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "destroy this is forbidden inside a destructor; the active destruction entry owns the object lifetime");
            }

            const bool destroysOwnedValue = operand.valid &&
                operand.ownership == sema::OwnershipKind::Owned && !destroysRaiiLocal;
            if (destroysOwnedValue)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "explicit destroy of an owned value is forbidden; its owner must also discharge storage");
            }

            result.typeKey = "builtin:void";
            result.valid = operand.valid && validTarget && !destroysRaiiLocal &&
                !destroysCurrentThis && !destroysOwnedValue;
            break;
        }
        default:
            break;
    }

    recordExpressionType(expression, methodMemberOrdinal, scopeOrdinal, result);
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeIdentifier(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    const std::optional<ScopeName> scopeName = lookupScopeName(scopeOrdinal, expression->text());
    if (scopeName.has_value())
    {
        if (scopeName->kind == ScopeNameKind::Local)
        {
            if (scopeName->semanticOrdinal < locals_.size())
            {
                const MethodLocalSymbol& local = locals_[scopeName->semanticOrdinal];
                // C7 "definite assignment": this is the one place a local name becomes a READ of
                // its value, so it is the one place the check belongs. It runs before the
                // ownership diagnostics below on purpose -- an unassigned local is the more
                // fundamental complaint, and the moved-from checks below would otherwise report
                // a confusing second opinion about the same name.
                checkDefiniteAssignment(expression, scopeName->semanticOrdinal, diagnostics);
                if (local.isRaii && movedRaiiLocals_.contains(scopeName->semanticOrdinal))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "use of moved-from RAII local '" + local.name + "'");
                    return result;
                }
                if (local.isRaii && maybeMovedRaiiLocals_.contains(scopeName->semanticOrdinal))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "use of moved-from RAII local '" + local.name +
                            "': value is possibly moved after a control-flow join");
                    return result;
                }
                result.typeKey = local.typeKey;
                result.valid = true;
                result.objectLifetimeOwnership = local.objectLifetimeOwnership;
                result.storageOwnership = local.storageOwnership;
                if (local.isRaii)
                {
                    result.ownership = sema::OwnershipKind::Owned;
                    result.ownershipSourceLocalOrdinal = scopeName->semanticOrdinal;
                }
                else if (local.ownership == sema::OwnershipKind::Borrowed)
                {
                    if (!local.localBorrowBindingOrdinal.has_value() ||
                        *local.localBorrowBindingOrdinal >= localBorrowBindings_.size())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            "borrowed local '" + local.name + "' has no valid lifetime binding");
                        return {};
                    }
                    const MethodLocalBorrowBinding& borrow =
                        localBorrowBindings_[*local.localBorrowBindingOrdinal];
                    const MethodBorrowState borrowState =
                        localBorrowFlowState(*local.localBorrowBindingOrdinal);
                    const bool rootDefinitelyMoved =
                        (borrow.sourceOwnerLocalOrdinal.has_value() &&
                         movedRaiiLocals_.contains(*borrow.sourceOwnerLocalOrdinal)) ||
                        (borrow.sourceOwnerParameterOrdinal.has_value() &&
                         movedOwnedParameters_.contains(*borrow.sourceOwnerParameterOrdinal));
                    const bool rootMaybeMoved =
                        (borrow.sourceOwnerLocalOrdinal.has_value() &&
                         maybeMovedRaiiLocals_.contains(*borrow.sourceOwnerLocalOrdinal)) ||
                        (borrow.sourceOwnerParameterOrdinal.has_value() &&
                         maybeMovedOwnedParameters_.contains(*borrow.sourceOwnerParameterOrdinal));
                    if (borrowState == MethodBorrowState::Invalidated || rootDefinitelyMoved)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            "use of invalidated borrowed local '" + local.name + "'");
                        return {};
                    }
                    if (borrowState == MethodBorrowState::MaybeInvalid || rootMaybeMoved)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            "use of possibly invalidated borrowed local '" + local.name +
                                "' after a control-flow join");
                        return {};
                    }
                    result.ownership = sema::OwnershipKind::Borrowed;
                    result.ownershipSourceLocalOrdinal = borrow.sourceOwnerLocalOrdinal;
                    result.ownershipSourceParameterOrdinal = borrow.sourceOwnerParameterOrdinal;
                    result.borrowedLocalOrdinal = scopeName->semanticOrdinal;
                }
                // E11 heap-homes an outer local captured by a lambda in a shared cell.  It is an
                // lvalue on both sides of the closure boundary, exactly like the source local;
                // const and borrowed locals keep their existing restrictions.
                result.assignable = !local.isConst &&
                    local.ownership != sema::OwnershipKind::Borrowed;
                result.addressable = true;
                result.readonlyReceiver = local.isConst;
                nameBindings_.push_back(MethodNameBinding{
                    MethodNameBindingKind::Local,
                    expression,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    expression->text(),
                    result.typeKey,
                    scopeName->semanticOrdinal,
                    std::nullopt,
                    {},
                    nullptr,
                    "",
                    std::nullopt,
                    false});
            }
            return result;
        }

        const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
        if (method != nullptr && scopeName->semanticOrdinal < method->parameters.size())
        {
            const MethodParameterSymbol& parameter = method->parameters[scopeName->semanticOrdinal];
            if (parameter.ownership == sema::OwnershipKind::Owned &&
                movedOwnedParameters_.contains(scopeName->semanticOrdinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "use of moved-from owned parameter '" + parameter.name + "'");
                return result;
            }
            if (parameter.ownership == sema::OwnershipKind::Owned &&
                maybeMovedOwnedParameters_.contains(scopeName->semanticOrdinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "use of moved-from owned parameter '" + parameter.name +
                        "': value is possibly moved after a control-flow join");
                return result;
            }
            // C4 "language references": a by-reference parameter dereferences implicitly. The
            // symbol's own typeKey is `T*` (that is what the callee physically receives), but the
            // programmer wrote `ref T x` and every use of `x` means the T on the other side of
            // the pointer -- reading it, assigning it, passing it on. That implicit dereference
            // is the entire ergonomic difference between `ref int x` and `int* x`, and it is the
            // reason a ref parameter needs its own rule here instead of being pointer sugar at
            // the call site only.
            const bool byReferenceParameter = parameter.passKind != ParameterPassKind::Value;
            result.typeKey = byReferenceParameter ? parameter.pointeeTypeKey : parameter.typeKey;
            result.valid = true;
            result.ownership = parameter.ownership;
            result.objectLifetimeOwnership = parameter.ownership;
            result.storageOwnership = parameter.ownership;
            if (parameter.ownership != sema::OwnershipKind::None)
            {
                result.ownershipSourceParameterOrdinal = scopeName->semanticOrdinal;
            }
            // `in` is read-only by definition: it exists to avoid a copy, not to give the callee
            // a channel back to the caller. `ref`/`out` are assignable, and for `out` that is not
            // a permission but an obligation -- see the definite-assignment check on returns.
            // E11 captured parameters are shared cells too.  `in` remains read-only; value/ref/
            // out parameters preserve the same assignability inside a closure as in the method.
            result.assignable = parameter.passKind != ParameterPassKind::In;
            result.addressable = true;
            result.readonlyReceiver = parameter.passKind == ParameterPassKind::In;
            nameBindings_.push_back(MethodNameBinding{
                MethodNameBindingKind::Parameter,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                expression->text(),
                result.typeKey,
                std::nullopt,
                scopeName->semanticOrdinal,
                {},
                nullptr,
                "",
                std::nullopt,
                false});
        }
        return result;
    }

    const bool namespaceCallable =
        methodMemberOrdinal >= namespaceCallableBase_ &&
        methodMemberOrdinal - namespaceCallableBase_ < namespaceCallableAdapters_.size();
    if (namespaceCallable && namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceStateReferenceBinding* stateReference =
                namespaceFunctionModel_->stateReferenceBinding(expression);
            stateReference != nullptr && !stateReference->qualified)
        {
            return analyzeNamespaceState(
                expression, *stateReference, methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
    }
    const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
    if (caller == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr)
    {
        return result;
    }

    const std::size_t accessingTypeOrdinal = caller->declaringTypeOrdinal;
    const std::string thisTypeKey = implicitThisTypeKey(methodMemberOrdinal);
    const bool staticContext = !contextHasImplicitThis(methodMemberOrdinal);
    const std::vector<std::size_t> candidates = memberCandidates(methodMemberOrdinal, expression->text());
    std::vector<std::size_t> fields;
    std::vector<std::size_t> events;
    std::vector<std::size_t> methods;
    bool inaccessibleFound = false;
    for (const std::size_t candidate : candidates)
    {
        if (candidate >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& member = memberModel_->members()[candidate];
        if (member.kind == ClassMemberKind::Constructor || member.kind == ClassMemberKind::Destructor)
        {
            continue;
        }
        if (staticContext && !member.isStatic)
        {
            continue;
        }
        if (!ProjectMemberAccessibility::isAccessible(
                member,
                accessingTypeOrdinal,
                accessingTypeOrdinal,
                *nameLookup_,
                *inheritanceModel_))
        {
            inaccessibleFound = true;
            continue;
        }
        if (member.kind == ClassMemberKind::Field)
        {
            fields.push_back(candidate);
        }
        else if (member.kind == ClassMemberKind::Event)
        {
            events.push_back(candidate);
        }
        else if (member.kind == ClassMemberKind::Method)
        {
            methods.push_back(candidate);
        }
    }

    if (!events.empty())
    {
        const std::size_t eventOrdinal = events.front();
        const ClassMemberSymbol& event = memberModel_->members()[eventOrdinal];
        result.typeKey = event.declaredTypeKey;
        result.valid = allowEvent;
        result.assignable = false;
        result.eventMemberOrdinal = eventOrdinal;
        result.readonlyReceiver = !event.isStatic &&
            contextHasReadonlyReceiver(methodMemberOrdinal);
        if (!event.isStatic)
        {
            result.receiverTypeKey = thisTypeKey;
            result.receiverTypeOrdinal = accessingTypeOrdinal;
        }
        nameBindings_.push_back(MethodNameBinding{
            event.isStatic ? MethodNameBindingKind::StaticEvent : MethodNameBindingKind::ImplicitEvent,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            expression->text(),
            result.typeKey,
            std::nullopt,
            std::nullopt,
            events,
            nullptr,
            event.isStatic ? "" : thisTypeKey,
            event.isStatic ? std::nullopt : std::optional<std::size_t>(accessingTypeOrdinal),
            false});
        if (!allowEvent)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "event '" + expression->text() +
                    "' may only be used with +=, -=, or invocation from within its declaring type in Stage 42");
        }
        return result;
    }

    if (!fields.empty())
    {
        const ClassMemberSymbol& field = memberModel_->members()[fields.front()];
        const bool readonlyThis = !field.isStatic && contextHasReadonlyReceiver(methodMemberOrdinal);
        result.typeKey = field.declaredTypeKey;
        result.valid = true;
        result.assignable = !readonlyThis && !field.isConst &&
            (!field.isReadonly || readonlyFieldAssignable(field, methodMemberOrdinal));
        result.addressable = !field.isBitField;
        result.readonlyReceiver = readonlyThis || field.isReadonly;
        result.assignmentMemberOrdinal = fields.front();
        if (!field.isStatic)
        {
            result.receiverTypeKey = thisTypeKey;
            result.receiverTypeOrdinal = accessingTypeOrdinal;
        }
        nameBindings_.push_back(MethodNameBinding{
            field.isStatic ? MethodNameBindingKind::StaticField : MethodNameBindingKind::ImplicitField,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            expression->text(),
            result.typeKey,
            std::nullopt,
            std::nullopt,
            fields,
            nullptr,
            field.isStatic ? "" : thisTypeKey,
            field.isStatic ? std::nullopt : std::optional<std::size_t>(accessingTypeOrdinal),
            false});
        return result;
    }

    {
        std::vector<std::size_t> propertyGetters;
        std::vector<std::size_t> propertySetters;
        const auto collectAccessor = [&](std::string_view accessorName, bool getter)
        {
            std::vector<std::size_t>& output = getter ? propertyGetters : propertySetters;
            for (const std::size_t ordinal : memberCandidates(methodMemberOrdinal, accessorName))
            {
                if (ordinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& member = memberModel_->members()[ordinal];
                if (!member.isPropertyAccessor || getter != member.isGetter) continue;
                if (staticContext && !member.isStatic) continue;
                if (!staticContext && member.isStatic && !output.empty()) continue;
                if (!ProjectMemberAccessibility::isAccessible(
                        member, accessingTypeOrdinal, accessingTypeOrdinal, *nameLookup_, *inheritanceModel_))
                {
                    continue;
                }
                output.push_back(ordinal);
            }
            if (!staticContext)
            {
                const bool hasInstance = std::any_of(output.begin(), output.end(), [&](std::size_t ordinal)
                {
                    return ordinal < memberModel_->members().size() && !memberModel_->members()[ordinal].isStatic;
                });
                if (hasInstance)
                {
                    std::erase_if(output, [&](std::size_t ordinal)
                    {
                        return ordinal < memberModel_->members().size() && memberModel_->members()[ordinal].isStatic;
                    });
                }
            }
        };
        collectAccessor("get_" + expression->text(), true);
        collectAccessor("set_" + expression->text(), false);
        if (!propertyGetters.empty())
        {
            const bool staticProperty = memberModel_->members()[propertyGetters.front()].isStatic;
            const std::optional<std::size_t> backing =
                allowConstructorPropertyWrite && propertySetters.empty() && !staticProperty &&
                    caller->kind == ClassMemberKind::Constructor
                ? automaticPropertyBackingField(
                      accessingTypeOrdinal, expression->text(), false)
                : std::nullopt;
            if (backing.has_value())
            {
                const ClassMemberSymbol& field = memberModel_->members()[*backing];
                result.typeKey = field.declaredTypeKey;
                result.valid = true;
                result.assignable = true;
                result.addressable = true;
                result.assignmentMemberOrdinal = *backing;
                result.receiverTypeKey = thisTypeKey;
                result.receiverTypeOrdinal = accessingTypeOrdinal;
                nameBindings_.push_back(MethodNameBinding{
                    MethodNameBindingKind::ImplicitField,
                    expression,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    expression->text(),
                    result.typeKey,
                    std::nullopt,
                    std::nullopt,
                    {*backing},
                    nullptr,
                    thisTypeKey,
                    accessingTypeOrdinal,
                    false});
                return result;
            }
            result = resolveSyntheticCall(
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "get_" + expression->text(),
                propertyGetters,
                {},
                nullptr,
                staticProperty ? "" : thisTypeKey,
                staticProperty ? std::nullopt : std::optional<std::size_t>(accessingTypeOrdinal),
                false,
                false,
                diagnostics,
                true,
                !staticProperty && contextHasReadonlyReceiver(methodMemberOrdinal));
            result.assignable = !contextHasReadonlyReceiver(methodMemberOrdinal) &&
                !propertySetters.empty();
            return result;
        }
    }

    if (!methods.empty())
    {
        result.methodGroup = true;
        result.methodCandidates = methods;
        result.valid = allowMethodGroup;
        const bool allStatic = std::all_of(
            methods.begin(), methods.end(), [&](std::size_t ordinal)
            {
                return ordinal < memberModel_->members().size() && memberModel_->members()[ordinal].isStatic;
            });
        if (!allStatic)
        {
            result.receiverTypeKey = thisTypeKey;
            result.receiverTypeOrdinal = accessingTypeOrdinal;
            result.readonlyReceiver = contextHasReadonlyReceiver(methodMemberOrdinal);
        }
        nameBindings_.push_back(MethodNameBinding{
            allStatic ? MethodNameBindingKind::StaticMethodGroup : MethodNameBindingKind::ImplicitMethodGroup,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            expression->text(),
            "<method-group>",
            std::nullopt,
            std::nullopt,
            methods,
            nullptr,
            allStatic ? "" : thisTypeKey,
            allStatic ? std::nullopt : std::optional<std::size_t>(accessingTypeOrdinal),
            false});
        if (!allowMethodGroup)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "method group '" + expression->text() + "' can only be used as a call target");
        }
        return result;
    }

    diagnostics.report(
        inaccessibleFound ? diagnostics::DiagnosticId::InaccessibleMember
                          : diagnostics::DiagnosticId::UnknownIdentifier,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        inaccessibleFound
            ? "member '" + expression->text() + "' is not accessible in this context"
            : "unknown identifier '" + expression->text() + "' in callable/initializer context");
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeNamespaceState(
    const ast::AstNode* expression,
    const NamespaceStateReferenceBinding& reference,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (namespaceFunctionModel_ == nullptr || reference.candidateStateOrdinals.empty())
        return result;
    if (reference.candidateStateOrdinals.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            expression != nullptr ? expression->range() : SourceRange{},
            "ambiguous namespace state reference '" + reference.writtenName + "'");
        return result;
    }

    const std::size_t stateOrdinal = reference.candidateStateOrdinals.front();
    if (stateOrdinal >= namespaceFunctionModel_->states().size()) return result;
    const NamespaceStateSymbol& state = namespaceFunctionModel_->states()[stateOrdinal];
    result.typeKey = state.declaredTypeKey;
    result.valid = true;
    result.assignable = !state.isConst;
    result.addressable = true;
    result.readonlyReceiver = state.isConst;
    nameBindings_.push_back(MethodNameBinding{
        MethodNameBindingKind::NamespaceState,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        reference.writtenName,
        result.typeKey,
        std::nullopt,
        std::nullopt,
        {stateOrdinal},
        nullptr,
        "",
        std::nullopt,
        false});
    return result;
}
ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeMember(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    if (namespaceFunctionModel_ != nullptr && expression != nullptr)
    {
        if (const NamespaceStateReferenceBinding* stateReference =
                namespaceFunctionModel_->stateReferenceBinding(expression);
            stateReference != nullptr && stateReference->qualified)
        {
            const bool receiverShadowed =
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
                lookupScopeName(scopeOrdinal, expression->first()->text()).has_value();
            if (!receiverShadowed)
            {
                return analyzeNamespaceState(
                    expression, *stateReference, methodMemberOrdinal, scopeOrdinal, diagnostics);
            }
        }
    }
    if (expression == nullptr || expression->first() == nullptr || memberModel_ == nullptr ||
        nameLookup_ == nullptr || inheritanceModel_ == nullptr)
    {
        return result;
    }

    const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
    if (caller == nullptr)
    {
        return result;
    }
    const std::size_t accessingTypeOrdinal = caller->declaringTypeOrdinal;

    ExpressionResult receiver;
    bool typeReceiver = false;
    bool valueReceiver = false;
    std::optional<std::size_t> typeOrdinal;
    std::string writtenTypeName;

    // A value in lexical/member scope wins over an equally named type. Besides matching the
    // usual expression lookup rule, this is essential for APIs with a property named after its
    // type (`Transform Transform` followed by `Transform.Position`). The old type-first probe
    // interpreted that receiver as the class and then complained that Position was not static.
    bool memberValueShadowsType = false;
    if (expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::string& receiverName = expression->first()->text();
        const std::vector<std::size_t> candidates = memberCandidates(methodMemberOrdinal, receiverName);
        memberValueShadowsType = std::any_of(candidates.begin(), candidates.end(), [&](std::size_t ordinal)
        {
            if (ordinal >= memberModel_->members().size()) return false;
            const ClassMemberKind kind = memberModel_->members()[ordinal].kind;
            return kind != ClassMemberKind::Constructor && kind != ClassMemberKind::Destructor;
        });
        memberValueShadowsType = memberValueShadowsType ||
            !memberCandidates(methodMemberOrdinal, "get_" + receiverName).empty() ||
            !memberCandidates(methodMemberOrdinal, "set_" + receiverName).empty();
    }
    if (expression->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        (lookupScopeName(scopeOrdinal, expression->first()->text()).has_value() ||
         memberValueShadowsType))
    {
        receiver = analyzeExpression(
            expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!receiver.valid || receiver.methodGroup)
        {
            return result;
        }
        valueReceiver = true;
        receiver.receiverSyntax = expression->first();
        receiver.receiverTypeKey = receiver.typeKey;
        receiver.receiverTypeOrdinal = namedTypeOrdinalForTypeKey(receiver.typeKey);
    }

    if (!valueReceiver &&
        expression->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        expression->first()->typeSyntax() != nullptr)
    {
        // ID 131. `Caja<int>.Make(...)`. El parser deja el TypeSyntax del tipo CERRADO colgado del
        // identificador cuando la forma escrita llevaba argumentos de tipo. Cuando esta, se
        // resuelve por CLAVE DE TIPO y no por nombre suelto: `resolveTypeIdentifier` busca por
        // `identity.name()`, que para un generico es el nombre ABIERTO, y devolveria la
        // declaracion sin cerrar -- o sea `Caja<T>` donde se escribio `Caja<int>`, y a partir de
        // ahi todo lo que se dedujera del miembro seria de otro tipo.
        //
        // `canonicalTypeKey` es el MISMO servicio que usan la declaracion de un local y el `new`,
        // asi que la clave que sale aqui es la de la monomorfizacion que esas dos ya crearon. Si
        // no existe --se escribio un tipo cerrado que nadie mas usa-- no se resuelve nada y el
        // camino de abajo da el diagnostico de siempre, que es lo correcto: inventar la
        // instanciacion desde un acceso a miembro seria monomorfizar en el sitio equivocado.
        const ast::TypeSyntax* closedType = expression->first()->typeSyntax();
        typeOrdinal = namedTypeOrdinalForTypeKey(
            memberModel_->canonicalTypeKey(closedType, *nameLookup_));
        writtenTypeName = expression->first()->text();
    }
    else if (!valueReceiver)
    {
        const ExpressionTypeNameBinding* binding =
            nameLookup_->expressionTypeBinding(expression->first());
        if (binding != nullptr)
        {
            writtenTypeName = binding->writtenName;
            if (binding->candidateTypeOrdinals.size() > 1)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::AmbiguousTypeName,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->first()->range(),
                    "ambiguous type name '" + writtenTypeName + "'; candidates: " +
                        typeCandidateList(binding->candidateTypeOrdinals, *nameLookup_));
                return result;
            }
            if (!binding->candidateTypeOrdinals.empty())
            {
                typeOrdinal = binding->candidateTypeOrdinals.front();
            }
        }
        else if (expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            typeOrdinal = resolveTypeIdentifier(methodMemberOrdinal, expression->first()->text());
            writtenTypeName = expression->first()->text();
        }
    }

    if (typeOrdinal.has_value())
    {
        typeReceiver = true;
        receiver.typeKey = inheritanceModel_->typeKeyForOrdinal(*typeOrdinal);
        receiver.valid = true;
        receiver.receiverSyntax = expression->first();
        receiver.receiverTypeKey = receiver.typeKey;
        receiver.receiverTypeOrdinal = typeOrdinal;
        nameBindings_.push_back(MethodNameBinding{
            MethodNameBindingKind::TypeReference,
            expression->first(),
            methodMemberOrdinal,
            scopeOrdinal,
            writtenTypeName,
            receiver.typeKey,
            std::nullopt,
            std::nullopt,
            {},
            expression->first(),
            receiver.typeKey,
            typeOrdinal,
            false});
    }

    if (!valueReceiver && !typeReceiver &&
        expression->first()->kind() == ast::AstNodeKind::BaseExpression)
    {
        if (!contextHasImplicitThis(methodMemberOrdinal))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidMemberAccess,
                diagnostics::DiagnosticSeverity::Error,
                expression->first()->range(),
                "base is not available in a static member or field initializer");
            return result;
        }
        const std::optional<std::size_t> baseType = inheritanceModel_->baseTypeOrdinal(accessingTypeOrdinal);
        if (!baseType.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidMemberAccess,
                diagnostics::DiagnosticSeverity::Error,
                expression->first()->range(),
                "base cannot be used because the containing class has no base class");
            return result;
        }
        receiver.typeKey = inheritanceModel_->typeKeyForOrdinal(*baseType);
        receiver.valid = true;
        receiver.receiverSyntax = expression->first();
        receiver.receiverTypeKey = receiver.typeKey;
        receiver.receiverTypeOrdinal = baseType;
        receiver.baseReceiver = true;
        nameBindings_.push_back(MethodNameBinding{
            MethodNameBindingKind::BaseReference,
            expression->first(),
            methodMemberOrdinal,
            scopeOrdinal,
            "base",
            receiver.typeKey,
            std::nullopt,
            std::nullopt,
            {},
            expression->first(),
            receiver.typeKey,
            baseType,
            true});
    }
    else if (!valueReceiver && !typeReceiver)
    {
        receiver = analyzeExpression(
            expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!receiver.valid || receiver.methodGroup)
        {
            return result;
        }
        receiver.receiverSyntax = expression->first();
        receiver.receiverTypeKey = receiver.typeKey;
        receiver.receiverTypeOrdinal = namedTypeOrdinalForTypeKey(receiver.typeKey);
    }

    if (!typeReceiver && expression->text() == "Length")
    {
        const std::optional<std::uint64_t> fixedCount =
            ProjectMemberModel::fixedArrayCount(receiver.typeKey);
        if (fixedCount.has_value())
        {
            result.typeKey = "builtin:int";
            result.valid = true;
            result.assignable = false;
            result.integerConstant = ProjectIntegerConstant{*fixedCount, false};
            coreBindings_.push_back(MethodCoreBinding{
                MethodCoreOperationKind::FixedArrayLength,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                result.typeKey,
                receiver.typeKey,
                false, std::nullopt, std::nullopt});
            return result;
        }
    }

    if (!typeReceiver && expression->text() == "Length" &&
        (ProjectTypeConversion::isNativeArray(receiver.typeKey) ||
         ProjectTypeConversion::isNativeString(receiver.typeKey) ||
         receiver.typeKey == "string-literal"))
    {
        result.typeKey = "builtin:int";
        result.valid = true;
        result.assignable = false;
        const bool arrayLength = ProjectTypeConversion::isNativeArray(receiver.typeKey);
        coreBindings_.push_back(MethodCoreBinding{
            arrayLength ? MethodCoreOperationKind::ArrayLength : MethodCoreOperationKind::StringLength,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            result.typeKey,
            arrayLength
                ? ProjectTypeConversion::nativeArrayElementType(receiver.typeKey).value_or(std::string{})
                : "builtin:char",
            false, std::nullopt, std::nullopt});
        return result;
    }

    // Stage 51: native string and primitive instance syntax is a language-level facade over
    // NativeSharp.Core.String static methods.  The receiver is forwarded as parameter zero;
    // this keeps string itself a native reference type without inventing a CLR-style object.
    if (!typeReceiver && nameLookup_ != nullptr && memberModel_ != nullptr)
    {
        const bool nativeStringReceiver = ProjectTypeConversion::isNativeString(receiver.typeKey) ||
            receiver.typeKey == "string-literal";
        const bool primitiveToStringReceiver = expression->text() == "ToString" &&
            (nativeStringReceiver || receiver.typeKey == "builtin:bool" ||
             ProjectTypeConversion::isNumeric(receiver.typeKey));
        if (nativeStringReceiver || primitiveToStringReceiver)
        {
            std::optional<std::size_t> coreStringTypeOrdinal;
            for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
            {
                if (nameLookup_->namedTypes()[ordinal].identity.fullyQualifiedName() ==
                    "NativeSharp.Core.String")
                {
                    coreStringTypeOrdinal = ordinal;
                    break;
                }
            }
            if (coreStringTypeOrdinal.has_value())
            {
                std::vector<std::size_t> methods;
                for (const std::size_t candidateOrdinal :
                     memberModel_->findMembers(*coreStringTypeOrdinal, expression->text()))
                {
                    if (candidateOrdinal >= memberModel_->members().size()) continue;
                    const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                    if (candidate.kind != ClassMemberKind::Method || !candidate.isStatic ||
                        candidate.parameters.empty())
                    {
                        continue;
                    }
                    const ProjectImplicitConversion receiverConversion = classifyImplicit(
                        receiver.typeKey,
                        candidate.parameters.front().typeKey,
                        receiver.integerConstant);
                    if (receiverConversion.exists()) methods.push_back(candidateOrdinal);
                }
                if (!methods.empty())
                {
                    result.methodGroup = true;
                    result.methodCandidates = methods;
                    result.valid = allowMethodGroup;
                    result.receiverSyntax = expression->first();
                    result.receiverTypeKey = receiver.typeKey;
                    result.receiverTypeOrdinal.reset();
                    result.ownership = receiver.ownership;
                    result.ownershipSourceLocalOrdinal = receiver.ownershipSourceLocalOrdinal;
                    result.ownershipSourceParameterOrdinal = receiver.ownershipSourceParameterOrdinal;
                    result.borrowedLocalOrdinal = receiver.borrowedLocalOrdinal;
                    result.integerConstant = receiver.integerConstant;
                    nameBindings_.push_back(MethodNameBinding{
                        MethodNameBindingKind::ExplicitObjectMethodGroup,
                        expression,
                        methodMemberOrdinal,
                        scopeOrdinal,
                        expression->text(),
                        "<native-string-method-group>",
                        std::nullopt,
                        std::nullopt,
                        methods,
                        expression->first(),
                        receiver.typeKey,
                        std::nullopt,
                        false});
                    if (!allowMethodGroup)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            expression->range(),
                            "NativeSharp.Core.String instance facade member '" + expression->text() +
                                "' can only be used as a call target");
                    }
                    return result;
                }
            }
        }
    }

    const std::optional<std::size_t> receiverType = receiver.receiverTypeOrdinal;
    const bool receiverIsStruct = receiverType.has_value() && nameLookup_ != nullptr &&
        *receiverType < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*receiverType].kind == NamedTypeDeclarationKind::Struct;
    // Stage 51.x: an enum type reference is only ever a *static* receiver (`Enum.Member`) --
    // there is no such thing as an enum instance flowing through this expression evaluator, so
    // this is safe to allow alongside class/struct/interface without widening instance-member
    // access to enums.
    const bool receiverIsEnum = typeReceiver && receiverType.has_value() && nameLookup_ != nullptr &&
        *receiverType < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*receiverType].kind == NamedTypeDeclarationKind::Enum;
    // ID 129. Un VALOR de enum, no la referencia al tipo. `receiverIsEnum` de arriba es
    // `Color.Rojo` -- el tipo como receptor estatico; esto es `c.ToString()` con `c` un valor.
    const bool receiverIsEnumValue = !typeReceiver && receiverType.has_value() &&
        nameLookup_ != nullptr && *receiverType < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*receiverType].kind == NamedTypeDeclarationKind::Enum;
    if (!receiverType.has_value() ||
        (!inheritanceModel_->isClass(*receiverType) && !receiverIsStruct && !receiverIsEnum &&
         !receiverIsEnumValue &&
         (interfaceModel_ == nullptr || !interfaceModel_->isInterface(*receiverType))))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "member access requires a class/interface reference, struct value, or enum type receiver; receiver has type '" +
                receiver.typeKey + "'");
        return result;
    }

    const std::vector<std::size_t> candidates = memberCandidatesForType(*receiverType, expression->text());
    std::vector<std::size_t> fields;
    std::vector<std::size_t> events;
    std::vector<std::size_t> methods;
    bool inaccessibleFound = false;
    for (const std::size_t candidate : candidates)
    {
        if (candidate >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& member = memberModel_->members()[candidate];
        if (member.kind == ClassMemberKind::Constructor || member.kind == ClassMemberKind::Destructor)
        {
            continue;
        }
        if (typeReceiver && !member.isStatic)
        {
            continue;
        }
        if (!typeReceiver && member.isStatic)
        {
            continue;
        }

        const std::size_t protectedReceiverType = receiver.baseReceiver
            ? accessingTypeOrdinal
            : *receiverType;
        const bool interfaceReceiver = interfaceModel_ != nullptr && interfaceModel_->isInterface(*receiverType);
        if ((!interfaceReceiver && !ProjectMemberAccessibility::isAccessible(
                member,
                accessingTypeOrdinal,
                protectedReceiverType,
                *nameLookup_,
                *inheritanceModel_)) ||
            (interfaceReceiver && member.accessibility != ClassMemberAccessibility::Public))
        {
            inaccessibleFound = true;
            continue;
        }
        if (member.kind == ClassMemberKind::Field)
        {
            fields.push_back(candidate);
        }
        else if (member.kind == ClassMemberKind::Event)
        {
            events.push_back(candidate);
        }
        else if (member.kind == ClassMemberKind::Method)
        {
            methods.push_back(candidate);
        }
    }

    const MethodNameBindingKind fieldBindingKind = typeReceiver
        ? MethodNameBindingKind::StaticField
        : receiver.baseReceiver
        ? MethodNameBindingKind::ExplicitBaseField
        : expression->first()->kind() == ast::AstNodeKind::ThisExpression
            ? MethodNameBindingKind::ExplicitThisField
            : MethodNameBindingKind::ExplicitObjectField;
    const MethodNameBindingKind methodBindingKind = typeReceiver
        ? MethodNameBindingKind::StaticMethodGroup
        : receiver.baseReceiver
        ? MethodNameBindingKind::ExplicitBaseMethodGroup
        : expression->first()->kind() == ast::AstNodeKind::ThisExpression
            ? MethodNameBindingKind::ExplicitThisMethodGroup
            : MethodNameBindingKind::ExplicitObjectMethodGroup;

    const MethodNameBindingKind eventBindingKind = typeReceiver
        ? MethodNameBindingKind::StaticEvent
        : expression->first()->kind() == ast::AstNodeKind::ThisExpression
            ? MethodNameBindingKind::ExplicitThisEvent
            : MethodNameBindingKind::ExplicitObjectEvent;

    if (!events.empty())
    {
        const std::size_t eventOrdinal = events.front();
        const ClassMemberSymbol& event = memberModel_->members()[eventOrdinal];
        result.typeKey = event.declaredTypeKey;
        result.valid = allowEvent;
        result.assignable = false;
        result.eventMemberOrdinal = eventOrdinal;
        result.readonlyReceiver = receiver.readonlyReceiver;
        result.receiverSyntax = typeReceiver ? nullptr : expression->first();
        result.receiverTypeKey = typeReceiver ? "" : receiver.typeKey;
        result.receiverTypeOrdinal = typeReceiver ? std::nullopt : receiverType;
        result.baseReceiver = receiver.baseReceiver;
        nameBindings_.push_back(MethodNameBinding{
            eventBindingKind,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            expression->text(),
            result.typeKey,
            std::nullopt,
            std::nullopt,
            events,
            typeReceiver ? nullptr : expression->first(),
            typeReceiver ? "" : receiver.typeKey,
            typeReceiver ? std::nullopt : receiverType,
            receiver.baseReceiver});
        if (!allowEvent)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "event '" + expression->text() +
                    "' may only be used with +=, -=, or invocation from within its declaring type in Stage 42");
        }
        return result;
    }

    if (!fields.empty())
    {
        result.typeKey = memberModel_->members()[fields.front()].declaredTypeKey;
        result.valid = true;
        result.assignmentMemberOrdinal = fields.front();
        const ClassMemberSymbol& field = memberModel_->members()[fields.front()];
        const bool readonlyOnThis = field.isReadonly && !typeReceiver &&
            expression->first()->kind() == ast::AstNodeKind::ThisExpression &&
            readonlyFieldAssignable(field, methodMemberOrdinal);
        result.assignable = !receiver.readonlyReceiver && !field.isConst &&
            (!field.isReadonly || readonlyOnThis);
        result.addressable = !field.isBitField;
        result.readonlyReceiver = receiver.readonlyReceiver || field.isReadonly;
        result.receiverSyntax = typeReceiver ? nullptr : expression->first();
        result.receiverTypeKey = typeReceiver ? "" : receiver.typeKey;
        result.receiverTypeOrdinal = typeReceiver ? std::nullopt : receiverType;
        result.baseReceiver = receiver.baseReceiver;
        nameBindings_.push_back(MethodNameBinding{
            fieldBindingKind,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            expression->text(),
            result.typeKey,
            std::nullopt,
            std::nullopt,
            fields,
            typeReceiver ? nullptr : expression->first(),
            typeReceiver ? "" : receiver.typeKey,
            typeReceiver ? std::nullopt : receiverType,
            receiver.baseReceiver});
        return result;
    }

    {
        const std::size_t protectedReceiverType = receiver.baseReceiver
            ? accessingTypeOrdinal : *receiverType;
        const std::vector<std::size_t> propertyGetters = stage38AccessorCandidates(
            *receiverType,
            "get_" + expression->text(),
            true,
            true,
            typeReceiver,
            accessingTypeOrdinal,
            protectedReceiverType);
        const std::vector<std::size_t> propertySetters = stage38AccessorCandidates(
            *receiverType,
            "set_" + expression->text(),
            true,
            false,
            typeReceiver,
            accessingTypeOrdinal,
            protectedReceiverType);
        if (!propertyGetters.empty())
        {
            const ClassMemberSymbol& getter = memberModel_->members()[propertyGetters.front()];
            const std::optional<std::size_t> backing =
                allowConstructorPropertyWrite && propertySetters.empty() && !typeReceiver &&
                    !getter.isStatic && caller->kind == ClassMemberKind::Constructor &&
                    caller->declaringTypeOrdinal == *receiverType &&
                    expression->first()->kind() == ast::AstNodeKind::ThisExpression
                ? automaticPropertyBackingField(*receiverType, expression->text(), false)
                : std::nullopt;
            if (backing.has_value())
            {
                const ClassMemberSymbol& field = memberModel_->members()[*backing];
                result.typeKey = field.declaredTypeKey;
                result.valid = true;
                result.assignable = true;
                result.addressable = true;
                result.assignmentMemberOrdinal = *backing;
                result.receiverSyntax = expression->first();
                result.receiverTypeKey = receiver.typeKey;
                result.receiverTypeOrdinal = receiverType;
                nameBindings_.push_back(MethodNameBinding{
                    MethodNameBindingKind::ExplicitThisField,
                    expression,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    expression->text(),
                    result.typeKey,
                    std::nullopt,
                    std::nullopt,
                    {*backing},
                    expression->first(),
                    receiver.typeKey,
                    receiverType,
                    false});
                return result;
            }
            const bool interfaceCall = !typeReceiver && interfaceModel_ != nullptr &&
                interfaceModel_->isInterface(*receiverType);
            result = resolveSyntheticCall(
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "get_" + expression->text(),
                propertyGetters,
                {},
                typeReceiver ? nullptr : expression->first(),
                typeReceiver ? "" : receiver.typeKey,
                typeReceiver ? std::nullopt : receiverType,
                receiver.baseReceiver,
                interfaceCall,
                diagnostics,
                true,
                receiver.readonlyReceiver);
            result.assignable = !receiver.readonlyReceiver && !propertySetters.empty();
            result.receiverSyntax = typeReceiver ? nullptr : expression->first();
            result.receiverTypeKey = typeReceiver ? "" : receiver.typeKey;
            result.receiverTypeOrdinal = typeReceiver ? std::nullopt : receiverType;
            result.baseReceiver = receiver.baseReceiver;
            return result;
        }
    }

    if (!methods.empty())
    {
        result.methodGroup = true;
        result.methodCandidates = methods;
        result.valid = allowMethodGroup;
        result.receiverSyntax = typeReceiver ? nullptr : expression->first();
        result.receiverTypeKey = typeReceiver ? "" : receiver.typeKey;
        result.receiverTypeOrdinal = typeReceiver ? std::nullopt : receiverType;
        result.baseReceiver = receiver.baseReceiver;
        result.readonlyReceiver = receiver.readonlyReceiver;
        nameBindings_.push_back(MethodNameBinding{
            methodBindingKind,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            expression->text(),
            "<method-group>",
            std::nullopt,
            std::nullopt,
            methods,
            typeReceiver ? nullptr : expression->first(),
            typeReceiver ? "" : receiver.typeKey,
            typeReceiver ? std::nullopt : receiverType,
            receiver.baseReceiver});
        if (!allowMethodGroup)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "instance method group '" + expression->text() + "' can only be used as a call target");
        }
        return result;
    }

    // C8 "type root", struct half. A struct is a VALUE: it has no vtable pointer and no room for
    // one (that is exactly what C6 proved and what the C host header certifies), so it cannot
    // inherit the root's three members the way a class does. It gets them synthesized instead,
    // resolved on the STATIC type -- which is what a struct is -- and lowered inline. No boxing,
    // no allocation, no layout change.
    //
    // This runs only after ordinary member lookup has already failed, so a struct that declares
    // its own ToString/Equals/GetHashCode simply wins; these fill a gap, they never shadow.
    // ID 129, la mitad de los ENUMS. Misma forma exacta que la del struct de abajo y por el mismo
    // motivo: se sintetiza SOLO despues de que la busqueda ordinaria haya fallado, asi que un enum
    // con su propio ToString gana y esto solo rellena el hueco.
    //
    // Solo `ToString`. `Equals` y `GetHashCode` sobre un enum son la comparacion y el hash del
    // entero que hay debajo, y eso ya funciona sin sintetizar nada.
    if (receiverIsEnumValue && !inaccessibleFound && expression->text() == "ToString")
    {
        result.methodGroup = true;
        result.methodCandidates.clear();
        result.valid = allowMethodGroup;
        result.receiverSyntax = expression->first();
        result.receiverTypeKey = receiver.typeKey;
        result.receiverTypeOrdinal = receiverType;
        result.typeKey = kEnumRootFacadeTypeKey;
        nameBindings_.push_back(MethodNameBinding{
            MethodNameBindingKind::ExplicitObjectMethodGroup,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            expression->text(),
            kEnumRootFacadeTypeKey,
            std::nullopt,
            std::nullopt,
            {},
            expression->first(),
            receiver.typeKey,
            receiverType,
            false});
        if (!allowMethodGroup)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "enum ToString can only be used as a call target");
        }
        return result;
    }

    if (!typeReceiver && receiverIsStruct && !inaccessibleFound &&
        (expression->text() == "ToString" || expression->text() == "Equals" ||
         expression->text() == "GetHashCode" || expression->text() == "Clone") &&
        aggregateAllowsSynthesizedMember(receiver.typeKey, expression->text()))
    {
        result.methodGroup = true;
        result.methodCandidates.clear();
        result.valid = allowMethodGroup;
        result.receiverSyntax = expression->first();
        result.receiverTypeKey = receiver.typeKey;
        result.receiverTypeOrdinal = receiverType;
        result.typeKey = kStructRootFacadeTypeKey;
        nameBindings_.push_back(MethodNameBinding{
            MethodNameBindingKind::ExplicitObjectMethodGroup,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            expression->text(),
            kStructRootFacadeTypeKey,
            std::nullopt,
            std::nullopt,
            {},
            expression->first(),
            receiver.typeKey,
            receiverType,
            false});
        if (!allowMethodGroup)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "struct root member '" + expression->text() + "' can only be used as a call target");
        }
        return result;
    }

    diagnostics.report(
        inaccessibleFound ? diagnostics::DiagnosticId::InaccessibleMember
                          : diagnostics::DiagnosticId::UnknownMember,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        inaccessibleFound
            ? "member '" + expression->text() + "' is not accessible through receiver/type '" +
                receiver.typeKey + "'"
            : "receiver/type '" + receiver.typeKey + "' has no compatible member named '" +
                expression->text() + "'");
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeNamespaceFunctionCall(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    NamespaceFunctionCallBinding call;
    call.syntax = expression;
    call.callerCallableOrdinal = callerCallableOrdinal;
    call.scopeOrdinal = scopeOrdinal;
    call.writtenName = reference.writtenName;
    call.candidateFunctionOrdinals = reference.candidateFunctionOrdinals;
    const auto store = [&]()
    {
        namespaceFunctionCallBySyntax_[expression] = namespaceFunctionCalls_.size();
        namespaceFunctionCalls_.push_back(call);
    };

    std::vector<ExpressionResult> arguments;
    arguments.reserve(expression->children().size());
    bool argumentsValid = true;
    for (const ast::AstNode* argument : expression->children())
    {
        ExpressionResult analyzed = analyzeExpression(
            argument, callerCallableOrdinal, scopeOrdinal, diagnostics,
            false, false, true);
        argumentsValid = argumentsValid && analyzed.valid && !analyzed.methodGroup;
        arguments.push_back(std::move(analyzed));
    }
    if (!argumentsValid)
    {
        store();
        return result;
    }

    bool matchingArity = false;
    for (const std::size_t functionOrdinal : reference.candidateFunctionOrdinals)
    {
        if (functionOrdinal >= namespaceCallableAdapters_.size()) continue;
        const ClassMemberSymbol& candidate = namespaceCallableAdapters_[functionOrdinal];
        MethodCallCandidate evaluated;
        evaluated.memberOrdinal = functionOrdinal;
        evaluated.isVariadicCandidate = candidate.isVariadic;
        const NamedArgumentMap mapping = mapArgumentsToParameters(expression->children(), candidate);
        if (!mapping.valid)
        {
            call.evaluatedCandidates.push_back(std::move(evaluated));
            continue;
        }
        matchingArity = true;
        evaluated.viable = true;
        evaluated.argumentParameterOrdinals = mapping.explicitParameterOrdinals;
        evaluated.defaultedParameterOrdinals = mapping.defaultedParameterOrdinals;
        evaluated.parameterPackElements = mapping.parameterPackElements;
        evaluated.parameterPackOrdinal = mapping.parameterPackOrdinal;
        evaluated.usesExpandedParameterPack = mapping.usesExpandedParameterPack;
        evaluated.synthesizesEmptyParameterPack = mapping.synthesizesEmptyParameterPack;
        if (evaluated.usesExpandedParameterPack &&
            std::ranges::count(evaluated.parameterPackElements, true) == 1 &&
            evaluated.parameterPackOrdinal.has_value())
        {
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
        evaluated.defaultedParameterCount = mapping.defaultedParameterOrdinals.size();
        for (std::size_t argumentOrdinal = 0; argumentOrdinal < arguments.size(); ++argumentOrdinal)
        {
            const std::size_t parameterOrdinal =
                evaluated.argumentParameterOrdinals[argumentOrdinal];
            ProjectImplicitConversion conversion;
            if (parameterOrdinal >= candidate.parameters.size())
            {
                conversion = classifyCVariadicPromotion(arguments[argumentOrdinal].typeKey);
                if (!conversion.exists() ||
                    arguments[argumentOrdinal].argumentPassKind != ParameterPassKind::Value ||
                    arguments[argumentOrdinal].ownership != sema::OwnershipKind::None)
                {
                    evaluated.viable = false;
                    evaluated.invalidVariadicArgumentOrdinal = argumentOrdinal;
                }
            }
            else
            {
                const MethodParameterSymbol& parameter = candidate.parameters[parameterOrdinal];
                std::string conversionTarget = parameter.typeKey;
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
                conversion = classifyImplicit(
                    arguments[argumentOrdinal].typeKey,
                    conversionTarget,
                    arguments[argumentOrdinal].integerConstant);
                if (!conversion.exists() || parameter.passKind != arguments[argumentOrdinal].argumentPassKind)
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
                }
                else if (classifyOwnershipArgument(
                             packElement ? false : typeKeyIsOwnable(parameter.typeKey),
                             packElement ? sema::OwnershipKind::None : parameter.ownership,
                             arguments[argumentOrdinal].ownership,
                             arguments[argumentOrdinal].ownershipTransfer,
                             arguments[argumentOrdinal].ownershipFreshOwner) != OwnershipArgumentFit::Ok)
                {
                    evaluated.viable = false;
                }
            }
            evaluated.conversions.push_back(std::move(conversion));
        }
        if (evaluated.viable)
            call.viableCandidateFunctionOrdinals.push_back(functionOrdinal);
        call.evaluatedCandidates.push_back(std::move(evaluated));
    }

    if (!matchingArity)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "no overload of namespace function '" + reference.writtenName + "' accepts " +
                std::to_string(arguments.size()) + " argument(s)");
        store();
        return result;
    }

    std::vector<std::size_t> viable;
    for (std::size_t index = 0; index < call.evaluatedCandidates.size(); ++index)
        if (call.evaluatedCandidates[index].viable) viable.push_back(index);
    if (viable.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "no viable overload of namespace function '" + reference.writtenName +
                "' accepts the argument types");
        store();
        return result;
    }

    std::vector<std::size_t> nonDominated;
    for (const std::size_t candidateIndex : viable)
    {
        const bool dominated = std::ranges::any_of(nonDominated, [&](std::size_t other)
        {
            return betterCandidate(
                call.evaluatedCandidates[other], call.evaluatedCandidates[candidateIndex]);
        });
        if (dominated) continue;
        std::erase_if(nonDominated, [&](std::size_t other)
        {
            return betterCandidate(
                call.evaluatedCandidates[candidateIndex], call.evaluatedCandidates[other]);
        });
        nonDominated.push_back(candidateIndex);
    }
    if (nonDominated.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "ambiguous namespace function call to '" + reference.writtenName + "'");
        call.resolution = MethodCallResolutionKind::Ambiguous;
        store();
        return result;
    }

    const MethodCallCandidate& selected = call.evaluatedCandidates[nonDominated.front()];
    const std::size_t functionOrdinal = selected.memberOrdinal;
    const ClassMemberSymbol& callable = namespaceCallableAdapters_[functionOrdinal];
    checkArgumentPassKinds(
        expression,
        callable.name,
        namespaceCallableBase_ + functionOrdinal,
        arguments,
        diagnostics,
        selected.argumentParameterOrdinals);
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedFunctionOrdinal = functionOrdinal;
    call.selectedConversions = selected.conversions;
    call.selectedArgumentParameterOrdinals = selected.argumentParameterOrdinals;
    call.selectedParameterPackElements = selected.parameterPackElements;
    call.selectedParameterPackOrdinal = selected.parameterPackOrdinal;
    call.selectedUsesExpandedParameterPack = selected.usesExpandedParameterPack;
    call.selectedSynthesizesEmptyParameterPack = selected.synthesizesEmptyParameterPack;
    for (const std::size_t parameterOrdinal : selected.defaultedParameterOrdinals)
    {
        call.selectedDefaultArguments.push_back(callable.parameters[parameterOrdinal].defaultValueSyntax);
        call.selectedDefaultArgumentParameterOrdinals.push_back(parameterOrdinal);
    }
    call.resultTypeKey = callable.declaredTypeKey;
    result.typeKey = callable.declaredTypeKey;
    result.valid = true;
    result.assignable = callable.isReferenceReturn;
    result.addressable = callable.isReferenceReturn;
    result.ownership = callable.declaredOwnership;
    result.ownershipTransfer = callable.declaredOwnership == sema::OwnershipKind::Owned;
    result.ownershipFreshOwner = result.ownershipTransfer;
    if (result.ownership != sema::OwnershipKind::None)
    {
        result.objectLifetimeOwnership = result.ownership;
        result.storageOwnership = result.ownership;
    }
    store();
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // T07-F04/F06: compiler-owned, allocation-free abort. Abort deliberately skips lexical
    // cleanup; the enclosing statement turns builtin:never into an unreachable flow state.
    if (expression->auxiliaryText() == "builtin-panic")
    {
        if (!expression->children().empty() || !expression->typeSyntaxes().empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "panic() takes no arguments or generic type arguments in the abort profile");
            return {};
        }
        MethodCoreBinding binding;
        binding.kind = MethodCoreOperationKind::Panic;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.resultTypeKey = "builtin:never";
        coreBindings_.push_back(std::move(binding));
        result.typeKey = "builtin:never";
        result.valid = true;
        return result;
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
                return analyzeNamespaceFunctionCall(
                    expression, *reference, methodMemberOrdinal, scopeOrdinal, diagnostics);
            }
        }
    }
    MethodCallBinding call;
    call.syntax = expression;
    call.callerMethodMemberOrdinal = methodMemberOrdinal;
    call.scopeOrdinal = scopeOrdinal;
    call.explicitThis = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::MemberExpression &&
        expression->first()->first() != nullptr &&
        expression->first()->first()->kind() == ast::AstNodeKind::ThisExpression;

    const ExpressionResult callee = analyzeExpression(
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
                expression->first()->kind() == ast::AstNodeKind::MemberExpression
            ? expression->first()->text()
            : "<expression>";
    }

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
            return {};
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

    // then treats it exactly like any other core operation.
    if (callee.methodGroup && callee.typeKey == kStructRootFacadeTypeKey)
    {
        const std::string& memberName = call.methodName;
        const std::string& structTypeKey = callee.receiverTypeKey;
        std::optional<MethodCoreOperationKind> structKind;
        std::string resultTypeKey;
        std::size_t expectedArity = 0;
        if (memberName == "ToString")
        {
            structKind = MethodCoreOperationKind::StructToString;
            resultTypeKey = "builtin:string";
        }
        else if (memberName == "GetHashCode")
        {
            structKind = MethodCoreOperationKind::StructHashCode;
            resultTypeKey = "builtin:int";
        }
        else if (memberName == "Equals")
        {
            structKind = MethodCoreOperationKind::StructEquals;
            resultTypeKey = "builtin:bool";
            expectedArity = 1;
        }
        else if (memberName == "Clone")
        {
            structKind = MethodCoreOperationKind::StructClone;
            resultTypeKey = structTypeKey;
        }
        if (!structKind.has_value()) return {};
        if (expression->children().size() != expectedArity)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "the compiler-provided '" + memberName + "' on struct '" + structTypeKey +
                    "' takes " + std::to_string(expectedArity) + " argument(s)");
            return {};
        }
        // A synthesized Equals compares a struct with ITS OWN type. Widening it to take a root
        // reference would mean boxing, which this language does not do; a struct that wants to
        // compare against anything else declares its own Equals.
        if (expectedArity == 1)
        {
            const ExpressionResult other = analyzeExpression(
                expression->children().front(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!other.valid) return {};
            if (other.typeKey != structTypeKey)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children().front()->range(),
                    "the compiler-provided 'Equals' on struct '" + structTypeKey +
                        "' compares against its own type, not '" + other.typeKey +
                        "'; declare an Equals of your own to compare against anything else");
                return {};
            }
        }
        // Equality and hashing read every scalar leaf of the value. A union has no such reading:
        // only one of its members is live at a time and nothing in the value says which. C refuses
        // to compare unions for the same reason, and guessing here would be worse than refusing.
        if (structKind != MethodCoreOperationKind::StructToString &&
            aggregateIsUnion(structTypeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "union '" + structTypeKey + "' has no compiler-provided '" + memberName +
                    "': only one member of a union is live at a time, so comparing or hashing "
                    "every field would be meaningless; declare your own");
            return {};
        }
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

    // Native string instance syntax forwards to a static Core.String method whose first
    // parameter is the receiver.  Named source arguments must never be allowed to bind that
    // compiler-supplied parameter.
    bool coreStringForwarding = false;
    if (callee.receiverSyntax != nullptr && !callee.receiverTypeKey.empty() &&
        memberModel_ != nullptr && nameLookup_ != nullptr && !callee.methodCandidates.empty())
    {
        coreStringForwarding = std::ranges::all_of(callee.methodCandidates, [&](std::size_t ordinal)
        {
            if (ordinal >= memberModel_->members().size()) return false;
            const ClassMemberSymbol& member = memberModel_->members()[ordinal];
            return member.kind == ClassMemberKind::Method && member.isStatic &&
                member.declaringTypeOrdinal < nameLookup_->namedTypes().size() &&
                nameLookup_->namedTypes()[member.declaringTypeOrdinal].identity.fullyQualifiedName() ==
                    "NativeSharp.Core.String";
        });
    }

    const bool hasLambdaArguments = std::ranges::any_of(
        expression->children(), [](const ast::AstNode* argument)
        {
            return argument != nullptr && argument->kind() == ast::AstNodeKind::LambdaExpression;
        });
    std::vector<std::string> lambdaTargetTypes;
    if (hasLambdaArguments) lambdaTargetTypes.resize(expression->children().size());
    const auto seedLambdaTargetsFromSignature = [&](const std::vector<std::string>& parameterTypes)
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
    };

    if (hasLambdaArguments && callee.eventMemberOrdinal.has_value() && memberModel_ != nullptr &&
        *callee.eventMemberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& event = memberModel_->members()[*callee.eventMemberOrdinal];
        std::string ignoredReturnType;
        std::vector<std::string> parameterTypes;
        std::size_t ignoredDelegateOrdinal = 0;
        if (delegateSignature(event.declaredTypeKey, ignoredReturnType, parameterTypes, ignoredDelegateOrdinal))
            seedLambdaTargetsFromSignature(parameterTypes);
    }
    else if (hasLambdaArguments && !callee.methodGroup && isDelegateTypeKey(callee.typeKey))
    {
        std::string ignoredReturnType;
        std::vector<std::string> parameterTypes;
        std::size_t ignoredDelegateOrdinal = 0;
        if (delegateSignature(callee.typeKey, ignoredReturnType, parameterTypes, ignoredDelegateOrdinal))
            seedLambdaTargetsFromSignature(parameterTypes);
    }
    else if (hasLambdaArguments && callee.methodGroup && memberModel_ != nullptr)
    {
        std::vector<std::pair<const ClassMemberSymbol*, NamedArgumentMap>> arityMatches;
        for (const std::size_t candidateOrdinal : callee.methodCandidates)
        {
            if (candidateOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
            const NamedArgumentMap mapping = mapArgumentsToParameters(
                expression->children(), candidate, coreStringForwarding ? 1U : 0U);
            if (candidate.kind == ClassMemberKind::Method && mapping.valid)
            {
                arityMatches.emplace_back(&candidate, mapping);
            }
        }
        if (!arityMatches.empty())
        {
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
    }

    std::vector<ExpressionResult> arguments;
    arguments.reserve(expression->children().size());
    bool allArgumentsValid = true;
    for (std::size_t argumentOrdinal = 0; argumentOrdinal < expression->children().size(); ++argumentOrdinal)
    {
        const ast::AstNode* argument = expression->children()[argumentOrdinal];
        ExpressionResult argumentResult;
        if (argument->kind() == ast::AstNodeKind::LambdaExpression &&
            !lambdaTargetTypes[argumentOrdinal].empty())
        {
            argumentResult = bindLambdaToDelegate(
                argument, lambdaTargetTypes[argumentOrdinal], methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        else
        {
            argumentResult = analyzeExpression(
                argument, methodMemberOrdinal, scopeOrdinal, diagnostics, false, false, true);
        }
        if (!argumentResult.valid || argumentResult.methodGroup)
        {
            allArgumentsValid = false;
        }
        arguments.push_back(std::move(argumentResult));
    }

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
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            ProjectImplicitConversion conversion = classifyImplicit(
                arguments[index].typeKey, parameterTypes[index], arguments[index].integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children()[index]->range(),
                    "event invocation argument is not implicitly convertible to delegate parameter type");
                return result;
            }
            binding.argumentConversions.push_back(std::move(conversion));
        }
        eventBindings_.push_back(std::move(binding));
        result.typeKey = returnType;
        result.valid = true;
        return result;
    }

    if (!callee.methodGroup && isDelegateTypeKey(callee.typeKey))
    {
        if (!allArgumentsValid) return result;
        std::string returnType;
        std::vector<std::string> parameterTypes;
        std::size_t delegateOrdinal = 0;
        if (!delegateSignature(callee.typeKey, returnType, parameterTypes, delegateOrdinal)) return result;
        if (parameterTypes.size() != arguments.size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "delegate invocation argument count does not match its signature");
            return result;
        }
        MethodDelegateBinding binding;
        binding.kind = MethodDelegateOperationKind::Invoke;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.delegateTypeKey = callee.typeKey;
        binding.delegateTypeOrdinal = delegateOrdinal;
        binding.resultTypeKey = returnType;
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            ProjectImplicitConversion conversion = classifyImplicit(
                arguments[index].typeKey, parameterTypes[index], arguments[index].integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children()[index]->range(),
                    "delegate invocation argument is not implicitly convertible to parameter type");
                return result;
            }
            binding.argumentConversions.push_back(std::move(conversion));
        }
        delegateBindings_.push_back(std::move(binding));
        result.typeKey = returnType;
        result.valid = true;
        return result;
    }

    call.candidateMemberOrdinals = callee.methodCandidates;
    if (!callee.methodGroup)
    {
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        calls_.push_back(std::move(call));
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
        calls_.push_back(std::move(call));
        return result;
    }

    // Stage 51 native string/primitive instance facade.  A call such as text.Trim()
    // resolves to the static Core.String signature Trim(string).  The receiver is
    // parameter zero for overload resolution and lowering, but is never an ownership
    // transfer merely because instance syntax was used.
    call.forwardedReceiver = coreStringForwarding;
    if (coreStringForwarding) call.interfaceCall = false;

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
    const auto resolvedArgumentSyntax = [&](std::size_t ordinal) -> const ast::AstNode*
    {
        if (coreStringForwarding)
        {
            if (ordinal == 0) return callee.receiverSyntax;
            --ordinal;
        }
        return ordinal < expression->children().size() ? expression->children()[ordinal] : nullptr;
    };

    // NSC-LIMIT-021. A transfer-shaped value with no named owner may be borrowed for exactly the
    // containing call. Explicit `borrowed` parameters state that contract themselves. A very
    // small Core allow-list covers the native operations whose public source signature predates
    // `borrowed` but whose implementation is compiler/runtime-owned and demonstrably does not
    // retain the pointer. User-defined unqualified parameters are deliberately NOT included: an
    // API that may retain a reference must keep rejecting a temporary owner.
    const auto isFullExpressionBorrow = [&] (
        const ClassMemberSymbol& candidate,
        std::size_t argumentOrdinal,
        const ExpressionResult& argument) -> bool
    {
        if (!argument.valid || !argument.ownershipFreshOwner || !argument.ownershipTransfer ||
            argument.ownership != sema::OwnershipKind::Owned ||
            argumentOrdinal >= candidate.parameters.size() ||
            candidate.parameters[argumentOrdinal].typeKey != "builtin:string" ||
            candidate.declaredOwnership == sema::OwnershipKind::Borrowed)
        {
            return false;
        }
        if (candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::Borrowed)
        {
            return true;
        }
        if (candidate.parameters[argumentOrdinal].ownership != sema::OwnershipKind::None ||
            nameLookup_ == nullptr || candidate.declaringTypeOrdinal >= nameLookup_->namedTypes().size() ||
            !candidate.isStatic)
        {
            return false;
        }
        const std::string declaringName =
            nameLookup_->namedTypes()[candidate.declaringTypeOrdinal].identity.fullyQualifiedName();
        if (declaringName == "NativeSharp.Core.String")
        {
            return candidate.name == "Copy" || candidate.name == "Concat" ||
                candidate.name == "Equals" || candidate.name == "Substring" ||
                candidate.name == "ZFill";
        }
        return declaringName == "NativeSharp.Core.Console" &&
            (candidate.name == "WriteLine" || candidate.name == "Write" ||
             candidate.name == "ErrorWriteLine" || candidate.name == "ErrorWrite");
    };

    // A literal's bytes have process lifetime. Passing that value to an explicitly borrowed
    // native-string parameter creates no owner and therefore needs neither a move nor a hidden
    // cleanup. Keep the exception tied to the literal type and an explicit borrowed contract.
    const auto isStringLiteralBorrow = [](
        const ClassMemberSymbol& candidate,
        std::size_t parameterOrdinal,
        const ExpressionResult& argument) noexcept
    {
        return parameterOrdinal < candidate.parameters.size() &&
            candidate.parameters[parameterOrdinal].ownership == sema::OwnershipKind::Borrowed &&
            candidate.parameters[parameterOrdinal].typeKey == "builtin:string" &&
            argument.typeKey == "string-literal";
    };

    bool hasMatchingArity = false;
    // Las candidatas que eran viables por tipos y propiedad y cayeron SOLO por su `where`. Si al
    // final no queda ninguna viable, estas son las que explican por que.
    std::vector<const ast::AstNode*> constraintRejectedSyntaxes;
    if (memberModel_ != nullptr)
    {
        call.evaluatedCandidates.reserve(callee.methodCandidates.size());
        call.viableCandidateMemberOrdinals.reserve(callee.methodCandidates.size());
        for (const std::size_t candidateOrdinal : callee.methodCandidates)
        {
            if (candidateOrdinal >= memberModel_->members().size())
            {
                continue;
            }
            const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
            if (candidate.kind != ClassMemberKind::Method)
            {
                continue;
            }

            MethodCallCandidate candidateBinding;
            candidateBinding.memberOrdinal = candidateOrdinal;
            candidateBinding.isVariadicCandidate = candidate.isVariadic;
            const NamedArgumentMap explicitMapping = mapArgumentsToParameters(
                expression->children(), candidate, coreStringForwarding ? 1U : 0U);
            if (!explicitMapping.valid)
            {
                call.evaluatedCandidates.push_back(std::move(candidateBinding));
                continue;
            }

            candidateBinding.argumentParameterOrdinals = explicitMapping.explicitParameterOrdinals;
            candidateBinding.defaultedParameterOrdinals = explicitMapping.defaultedParameterOrdinals;
            candidateBinding.parameterPackElements = explicitMapping.parameterPackElements;
            candidateBinding.parameterPackOrdinal = explicitMapping.parameterPackOrdinal;
            candidateBinding.usesExpandedParameterPack = explicitMapping.usesExpandedParameterPack;
            candidateBinding.synthesizesEmptyParameterPack =
                explicitMapping.synthesizesEmptyParameterPack;
            if (coreStringForwarding)
            {
                candidateBinding.argumentParameterOrdinals.insert(
                    candidateBinding.argumentParameterOrdinals.begin(), 0U);
                candidateBinding.parameterPackElements.insert(
                    candidateBinding.parameterPackElements.begin(), false);
            }

            if (candidateBinding.usesExpandedParameterPack &&
                std::ranges::count(candidateBinding.parameterPackElements, true) == 1 &&
                candidateBinding.parameterPackOrdinal.has_value())
            {
                const auto packElement =
                    std::ranges::find(candidateBinding.parameterPackElements, true);
                const std::size_t argumentOrdinal = static_cast<std::size_t>(
                    std::distance(candidateBinding.parameterPackElements.begin(), packElement));
                const MethodParameterSymbol& pack =
                    candidate.parameters[*candidateBinding.parameterPackOrdinal];
                if (classifyImplicit(
                        resolvedArguments[argumentOrdinal].typeKey,
                        pack.typeKey,
                        resolvedArguments[argumentOrdinal].integerConstant).exists())
                {
                    candidateBinding.parameterPackElements[argumentOrdinal] = false;
                    candidateBinding.usesExpandedParameterPack = false;
                }
            }

            hasMatchingArity = true;
            candidateBinding.viable = true;
            candidateBinding.defaultedParameterCount = explicitMapping.defaultedParameterOrdinals.size();
            for (std::size_t argumentOrdinal = 0; argumentOrdinal < resolvedArguments.size(); ++argumentOrdinal)
            {
                const std::size_t parameterOrdinal =
                    candidateBinding.argumentParameterOrdinals[argumentOrdinal];
                if (parameterOrdinal >= candidate.parameters.size())
                {
                    ProjectImplicitConversion conversion = classifyCVariadicPromotion(
                        resolvedArguments[argumentOrdinal].typeKey);
                    const ExpressionResult& argument = resolvedArguments[argumentOrdinal];
                    if (!conversion.exists() || argument.argumentPassKind != ParameterPassKind::Value ||
                        argument.ownership != sema::OwnershipKind::None ||
                        argument.ownershipTransfer || argument.ownershipFreshOwner)
                    {
                        candidateBinding.viable = false;
                        candidateBinding.invalidVariadicArgumentOrdinal = argumentOrdinal;
                    }
                    candidateBinding.conversions.push_back(std::move(conversion));
                    continue;
                }
                const bool packElement =
                    argumentOrdinal < candidateBinding.parameterPackElements.size() &&
                    candidateBinding.parameterPackElements[argumentOrdinal];
                std::string conversionTarget = candidate.parameters[parameterOrdinal].typeKey;
                if (packElement)
                {
                    const std::string& elementType =
                        candidate.parameters[parameterOrdinal].parameterPackElementTypeKey;
                    if (elementType.empty()) candidateBinding.viable = false;
                    else conversionTarget = elementType;
                }
                ProjectImplicitConversion conversion = classifyImplicit(
                    resolvedArguments[argumentOrdinal].typeKey,
                    conversionTarget,
                    resolvedArguments[argumentOrdinal].integerConstant);
                if (!conversion.exists())
                {
                    candidateBinding.viable = false;
                }
                else if (packElement &&
                    (resolvedArguments[argumentOrdinal].ownership != sema::OwnershipKind::None ||
                     resolvedArguments[argumentOrdinal].ownershipTransfer ||
                     resolvedArguments[argumentOrdinal].ownershipFreshOwner ||
                     resolvedArguments[argumentOrdinal].argumentPassKind != ParameterPassKind::Value))
                {
                    candidateBinding.viable = false;
                }
                else
                {
                    const sema::OwnershipKind parameterOwnership =
                        candidate.parameters[parameterOrdinal].ownership;
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
                    const bool parameterOwnable =
                        typeKeyIsOwnable(candidate.parameters[parameterOrdinal].typeKey);
                    // NSC-LIMIT-010: la regla vive ahora en classifyOwnershipArgument, arriba de
                    // este fichero, y la comparte ProjectConstructionModel. Lo que sigue a este
                    // bloque es el comentario historico de Stage 52 que explica POR QUE el orden
                    // de las comprobaciones es el que es; se conserva porque esa razon no esta en
                    // ningun otro sitio.
                    if (!packElement && !isStringLiteralBorrow(
                            candidate, parameterOrdinal, resolvedArguments[argumentOrdinal]) &&
                        !isFullExpressionBorrow(
                            candidate, parameterOrdinal, resolvedArguments[argumentOrdinal]) &&
                        classifyOwnershipArgument(
                            parameterOwnable,
                            parameterOwnership,
                            resolvedArguments[argumentOrdinal].ownership,
                            resolvedArguments[argumentOrdinal].ownershipTransfer,
                            resolvedArguments[argumentOrdinal].ownershipFreshOwner) !=
                        OwnershipArgumentFit::Ok)
                    {
                        candidateBinding.viable = false;
                    }
                }
                candidateBinding.conversions.push_back(std::move(conversion));
            }
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
            {
                call.viableCandidateMemberOrdinals.push_back(candidateOrdinal);
            }
            call.evaluatedCandidates.push_back(std::move(candidateBinding));
        }
    }

    if (!hasMatchingArity)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "no overload of '" + call.methodName + "' accepts " +
                std::to_string(arguments.size()) + " argument(s)");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        calls_.push_back(std::move(call));
        return result;
    }

    std::vector<std::size_t> viableEvaluationOrdinals;
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
        // Ninguna candidata sobrevivio. Si alguna cayo por su `where` --y no por tipos-- ese es
        // el error del programa, y se dice con el mensaje del constraint en vez de con un
        // «ninguna sobrecarga acepta estos argumentos» que no explica nada.
        if (!constraintRejectedSyntaxes.empty() && genericConstraintModel_ != nullptr)
        {
            for (const ast::AstNode* rejected : constraintRejectedSyntaxes)
            {
                genericConstraintModel_->reportSpecializationConstraintViolations(
                    rejected, expression->range(), diagnostics);
            }
            call.resolution = MethodCallResolutionKind::NoViableCandidate;
            calls_.push_back(std::move(call));
            return result;
        }
        for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
        {
            if (!candidate.invalidVariadicArgumentOrdinal.has_value()) continue;
            const std::size_t ordinal = *candidate.invalidVariadicArgumentOrdinal;
            const ast::AstNode* argument = resolvedArgumentSyntax(ordinal);
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                argument != nullptr ? argument->range() : expression->range(),
                "C variadic arguments must be by-value ABI scalars, raw pointers, enums, "
                "or extern C delegates");
            call.resolution = MethodCallResolutionKind::NoViableCandidate;
            calls_.push_back(std::move(call));
            return result;
        }
        bool reportedOwnershipDiagnostic = false;
        if (memberModel_ != nullptr)
        {
            for (const std::size_t candidateOrdinal : callee.methodCandidates)
            {
                if (candidateOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                if (candidate.kind != ClassMemberKind::Method ||
                    candidate.parameters.size() < resolvedArguments.size() ||
                    !std::ranges::all_of(
                        candidate.parameters.begin() +
                            static_cast<std::ptrdiff_t>(resolvedArguments.size()),
                        candidate.parameters.end(),
                        [](const MethodParameterSymbol& parameter)
                        {
                            return parameter.defaultValueSyntax != nullptr;
                        })) continue;
                for (std::size_t argumentOrdinal = 0; argumentOrdinal < resolvedArguments.size(); ++argumentOrdinal)
                {
                    const ProjectImplicitConversion physical = classifyImplicit(
                        resolvedArguments[argumentOrdinal].typeKey,
                        candidate.parameters[argumentOrdinal].typeKey,
                        resolvedArguments[argumentOrdinal].integerConstant);
                    if (!physical.exists()) continue;
                    const ast::AstNode* argumentSyntax = resolvedArgumentSyntax(argumentOrdinal);
                    // Stage 52: mirror the same value-type bypass applied in the primary
                    // candidate-matching loop above, so this diagnostic-only re-check doesn't
                    // report a misleading ownership error for a generic parameter closed over
                    // a plain value type.
                    const bool diagnosticParameterOwnable =
                        typeKeyIsOwnable(candidate.parameters[argumentOrdinal].typeKey);
                    // NSC-LIMIT-010: este re-chequeo, que existe solo para dar un mensaje bueno,
                    // llevaba escrita SU PROPIA copia de la regla -- la tercera. Ahora pregunta al
                    // mismo sitio que la viabilidad, asi que no puede acusar de un incumplimiento
                    // que la resolucion no vio, ni callarse uno que si.
                    if (candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::Owned &&
                        classifyOwnershipArgument(
                            diagnosticParameterOwnable,
                            candidate.parameters[argumentOrdinal].ownership,
                            resolvedArguments[argumentOrdinal].ownership,
                            resolvedArguments[argumentOrdinal].ownershipTransfer,
                            resolvedArguments[argumentOrdinal].ownershipFreshOwner) !=
                        OwnershipArgumentFit::Ok)
                    {
                        std::string message = "owned parameter '" + candidate.parameters[argumentOrdinal].name +
                            "' requires explicit ownership transfer";
                        if (argumentSyntax != nullptr &&
                            argumentSyntax->kind() == ast::AstNodeKind::IdentifierExpression)
                        {
                            if (resolvedArguments[argumentOrdinal].ownership == sema::OwnershipKind::Borrowed)
                            {
                                message = "cannot convert borrowed value '" + argumentSyntax->text() +
                                    "' to owned parameter";
                            }
                            else
                            {
                                message = "cannot copy owned value '" + argumentSyntax->text() +
                                    "' into owned parameter; use move(" + argumentSyntax->text() + ")";
                            }
                        }
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
                            std::move(message));
                        reportedOwnershipDiagnostic = true;
                        break;
                    }
                    if (candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::Borrowed &&
                        diagnosticParameterOwnable &&
                        resolvedArguments[argumentOrdinal].typeKey != "string-literal" &&
                        !resolvedArguments[argumentOrdinal].ownershipTransfer &&
                        resolvedArguments[argumentOrdinal].ownership == sema::OwnershipKind::None)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
                            "borrowed parameter '" + candidate.parameters[argumentOrdinal].name +
                                "' requires a checked owner or borrowed source");
                        reportedOwnershipDiagnostic = true;
                        break;
                    }
                    if (resolvedArguments[argumentOrdinal].ownership == sema::OwnershipKind::Borrowed &&
                        candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::None)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
                            "borrowed argument requires a borrowed parameter; unqualified parameter '" +
                                candidate.parameters[argumentOrdinal].name + "' may retain the value");
                        reportedOwnershipDiagnostic = true;
                        break;
                    }
                    if (resolvedArguments[argumentOrdinal].ownershipTransfer &&
                        candidate.parameters[argumentOrdinal].ownership != sema::OwnershipKind::Owned)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
                            candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::Borrowed
                                ? "cannot transfer ownership with move(...) into a borrowed parameter"
                                : "ownership-transfer argument requires an owned parameter");
                        reportedOwnershipDiagnostic = true;
                        break;
                    }
                }
                if (reportedOwnershipDiagnostic) break;
            }
        }
        // C4 "language references": same precision rule as the other overload-failure path --
        // when exactly one overload has the right arity and the ONLY thing wrong is the missing
        // (or extra) `ref`/`out`/`in`, say THAT. The type mismatch the programmer would otherwise
        // be shown (`int` versus `int*`) is an artifact of how a by-reference parameter is
        // represented, not something they wrote.
        bool reportedPassKindDiagnostic = false;
        if (!reportedOwnershipDiagnostic && memberModel_ != nullptr)
        {
            std::size_t arityMatches = 0;
            std::size_t soleCandidate = 0;
            for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
            {
                if (candidate.conversions.size() != arguments.size()) continue;
                ++arityMatches;
                soleCandidate = candidate.memberOrdinal;
            }
            if (arityMatches == 1 && soleCandidate < memberModel_->members().size())
            {
                const ClassMemberSymbol& only = memberModel_->members()[soleCandidate];
                for (std::size_t index = 0; index < arguments.size() && index < only.parameters.size(); ++index)
                {
                    if (only.parameters[index].passKind != arguments[index].argumentPassKind)
                    {
                        reportedPassKindDiagnostic = true;
                        break;
                    }
                }
                if (reportedPassKindDiagnostic)
                {
                    checkArgumentPassKinds(
                        expression, call.methodName, soleCandidate, arguments, diagnostics);
                }
            }
        }
        if (!reportedOwnershipDiagnostic && !reportedPassKindDiagnostic)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "no viable overload of '" + call.methodName + "' accepts the argument types");
        }
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        calls_.push_back(std::move(call));
        return result;
    }

    std::vector<std::size_t> nonDominated;
    nonDominated.reserve(viableEvaluationOrdinals.size());
    for (const std::size_t candidateIndex : viableEvaluationOrdinals)
    {
        const bool dominated = std::ranges::any_of(
            nonDominated, [&](const std::size_t otherIndex)
        {
            return betterCandidate(
                call.evaluatedCandidates[otherIndex],
                call.evaluatedCandidates[candidateIndex]);
        });
        if (dominated) continue;

        std::erase_if(nonDominated, [&](const std::size_t otherIndex)
        {
            return betterCandidate(
                call.evaluatedCandidates[candidateIndex],
                call.evaluatedCandidates[otherIndex]);
        });
        nonDominated.push_back(candidateIndex);
    }

    if (nonDominated.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "ambiguous instance method call to '" + call.methodName + "'");
        call.resolution = MethodCallResolutionKind::Ambiguous;
        calls_.push_back(std::move(call));
        return result;
    }

    const MethodCallCandidate& selected = call.evaluatedCandidates[nonDominated.front()];
    if (memberModel_ != nullptr && selected.memberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
        // NativeSharp 3.0 T03. Variant payload extractors have to be public so a match in a
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
            calls_.push_back(std::move(call));
            return result;
        }
        if (callee.readonlyReceiver && selectedMember.declaringTypeIsStruct &&
            !selectedMember.isStatic && !selectedMember.isReadonly)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "readonly receiver cannot invoke mutating struct member '" +
                    selectedMember.name + "'");
            call.resolution = MethodCallResolutionKind::NoViableCandidate;
            calls_.push_back(std::move(call));
            return result;
        }
    }
    checkArgumentPassKinds(
        expression, call.methodName, selected.memberOrdinal, arguments, diagnostics,
        std::span<const std::size_t>(selected.argumentParameterOrdinals).subspan(
            coreStringForwarding ? 1U : 0U));
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedMemberOrdinal = selected.memberOrdinal;
    call.selectedConversions = selected.conversions;
    call.selectedArgumentParameterOrdinals = selected.argumentParameterOrdinals;
    call.selectedParameterPackElements = selected.parameterPackElements;
    call.selectedParameterPackOrdinal = selected.parameterPackOrdinal;
    call.selectedUsesExpandedParameterPack = selected.usesExpandedParameterPack;
    call.selectedSynthesizesEmptyParameterPack = selected.synthesizesEmptyParameterPack;
    if (memberModel_ != nullptr && selected.memberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
        for (const std::size_t parameterOrdinal : selected.defaultedParameterOrdinals)
        {
            call.selectedDefaultArguments.push_back(
                selectedMember.parameters[parameterOrdinal].defaultValueSyntax);
            call.selectedDefaultArgumentParameterOrdinals.push_back(parameterOrdinal);
        }
        call.resultTypeKey = selectedMember.declaredTypeKey;
        result.ownership = selectedMember.declaredOwnership;
        if (selectedMember.declaredOwnership != sema::OwnershipKind::None)
        {
            result.objectLifetimeOwnership = selectedMember.declaredOwnership;
            result.storageOwnership = selectedMember.declaredOwnership;
        }
        result.assignable = selectedMember.isReferenceReturn;
        result.addressable = selectedMember.isReferenceReturn;
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
            {
                continue;
            }
            const ast::AstNode* argumentSyntax = resolvedArgumentSyntax(argumentOrdinal);
            if (argumentSyntax == nullptr || argumentSyntax->first() == nullptr)
            {
                continue;
            }
            const ast::AstNode* root = definiteAssignmentTargetRoot(argumentSyntax->first());
            if (root == nullptr)
            {
                continue;
            }
            const std::optional<ScopeName> target = lookupScopeName(scopeOrdinal, root->text());
            if (!target.has_value() || target->kind != ScopeNameKind::Parameter ||
                target->semanticOrdinal >= callerMember->parameters.size() ||
                callerMember->parameters[target->semanticOrdinal].passKind != ParameterPassKind::Out)
            {
                continue;
            }
            assignedOutParameters_.insert({methodMemberOrdinal, target->semanticOrdinal});
        }

        for (std::size_t argumentOrdinal = 0; argumentOrdinal < resolvedArguments.size() &&
             argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
        {
            const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
            if (parameterOrdinal >= selectedMember.parameters.size() ||
                selectedMember.parameters[parameterOrdinal].ownership != sema::OwnershipKind::Owned)
            {
                continue;
            }
            MethodOwnershipTransferBinding transfer;
            transfer.syntax = resolvedArgumentSyntax(argumentOrdinal);
            transfer.methodMemberOrdinal = methodMemberOrdinal;
            transfer.scopeOrdinal = scopeOrdinal;
            transfer.sourceLocalOrdinal = resolvedArguments[argumentOrdinal].ownershipSourceLocalOrdinal;
            transfer.sourceParameterOrdinal = resolvedArguments[argumentOrdinal].ownershipSourceParameterOrdinal;
            transfer.targetMemberOrdinal = selected.memberOrdinal;
            transfer.targetParameterOrdinal = parameterOrdinal;
            transfer.typeKey = selectedMember.parameters[parameterOrdinal].typeKey;
            if (transfer.sourceLocalOrdinal.has_value())
            {
                transfer.kind = MethodOwnershipTransferKind::MoveLocalToOwnedParameter;
            }
            else if (transfer.sourceParameterOrdinal.has_value())
            {
                transfer.kind = MethodOwnershipTransferKind::MoveParameterToOwnedParameter;
            }
            else
            {
                transfer.kind = MethodOwnershipTransferKind::OwnedReturnToOwnedParameter;
            }
            ownershipTransferBindings_.push_back(std::move(transfer));
        }

        for (std::size_t argumentOrdinal = 0; argumentOrdinal < resolvedArguments.size() &&
             argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
        {
            const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
            if (parameterOrdinal >= selectedMember.parameters.size()) continue;
            const bool freshTemporaryBorrow = isFullExpressionBorrow(
                selectedMember, parameterOrdinal, resolvedArguments[argumentOrdinal]);
            if (selectedMember.parameters[parameterOrdinal].ownership != sema::OwnershipKind::Borrowed &&
                !freshTemporaryBorrow)
            {
                continue;
            }
            MethodBorrowBinding borrow;
            borrow.syntax = resolvedArgumentSyntax(argumentOrdinal);
            borrow.methodMemberOrdinal = methodMemberOrdinal;
            borrow.scopeOrdinal = scopeOrdinal;
            borrow.sourceLocalOrdinal = resolvedArguments[argumentOrdinal].borrowedLocalOrdinal.has_value()
                ? resolvedArguments[argumentOrdinal].borrowedLocalOrdinal
                : resolvedArguments[argumentOrdinal].ownershipSourceLocalOrdinal;
            borrow.sourceParameterOrdinal = resolvedArguments[argumentOrdinal].ownershipSourceParameterOrdinal;
            borrow.targetMemberOrdinal = selected.memberOrdinal;
            borrow.targetParameterOrdinal = parameterOrdinal;
            borrow.typeKey = selectedMember.parameters[parameterOrdinal].typeKey;
            if (freshTemporaryBorrow)
            {
                borrow.kind = MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter;
            }
            else if (resolvedArguments[argumentOrdinal].borrowedLocalOrdinal.has_value())
            {
                borrow.kind = MethodBorrowKind::BorrowedLocalToBorrowedParameter;
            }
            else if (borrow.sourceLocalOrdinal.has_value())
            {
                borrow.kind = MethodBorrowKind::OwnerLocalToBorrowedParameter;
            }
            else if (borrow.sourceParameterOrdinal.has_value())
            {
                const ClassMemberSymbol* callerMember = methodSymbol(methodMemberOrdinal);
                if (callerMember != nullptr &&
                    *borrow.sourceParameterOrdinal < callerMember->parameters.size() &&
                    callerMember->parameters[*borrow.sourceParameterOrdinal].ownership == sema::OwnershipKind::Owned)
                {
                    borrow.kind = MethodBorrowKind::OwnedParameterToBorrowedParameter;
                }
                else
                {
                    borrow.kind = MethodBorrowKind::BorrowedParameterToBorrowedParameter;
                }
            }
            else
            {
                borrow.kind = MethodBorrowKind::BorrowedReturnToBorrowedParameter;
            }
            borrowBindings_.push_back(std::move(borrow));
        }

        if (nameLookup_ != nullptr && selectedMember.declaringTypeOrdinal < nameLookup_->namedTypes().size())
        {
            const NamedTypeDeclaration& declaringType = nameLookup_->namedTypes()[selectedMember.declaringTypeOrdinal];
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
            // Stage 53.3 "Filesystem/Paths" -- NativeSharp.Core.IO.File.Exists/Delete and
            // NativeSharp.Core.IO.Directory.Exists/CreateDirectory. Same one-argument recognition
            // shape as Environment.GetEnvironmentVariable (Exists) or Environment.Exit
            // (Delete/CreateDirectory, one argument + void return) above -- two classes instead
            // of one, so this is its own "if", not folded into the Environment block.
            if ((declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.File" ||
                    declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.Directory") &&
                selectedMember.isStatic)
            {
                const bool isFile = declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.File";
                std::optional<MethodCoreOperationKind> ioKind;
                if (selectedMember.name == "Exists" &&
                    selectedMember.declaredTypeKey == "builtin:bool" &&
                    selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == "builtin:string")
                {
                    ioKind = isFile ? MethodCoreOperationKind::FileExists : MethodCoreOperationKind::DirectoryExists;
                }
                else if (isFile && selectedMember.name == "Delete" &&
                    selectedMember.declaredTypeKey == "builtin:void" &&
                    selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::FileDelete;
                }
                else if (!isFile && selectedMember.name == "CreateDirectory" &&
                    selectedMember.declaredTypeKey == "builtin:void" &&
                    selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::DirectoryCreate;
                }
                // Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- ReadAllText/
                // WriteAllText/AppendAllText, File-only (Directory has no equivalent). Same
                // one-argument-returns-string shape as GetEnvironmentVariable (ReadAllText), and
                // the same two-argument-void shape as SetEnvironmentVariable
                // (WriteAllText/AppendAllText).
                else if (isFile && selectedMember.name == "ReadAllText" &&
                    selectedMember.declaredTypeKey == "builtin:string" &&
                    selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::FileReadAllText;
                }
                else if (isFile && selectedMember.name == "WriteAllText" &&
                    selectedMember.declaredTypeKey == "builtin:void" &&
                    selectedMember.parameters.size() == 2 &&
                    selectedMember.parameters[0].typeKey == "builtin:string" &&
                    selectedMember.parameters[1].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::FileWriteAllText;
                }
                // Bloque 4, fase A -- SHR-028. ReadAllBytes: misma forma de un argumento que
                // ReadAllText, y el mismo tipo de salida en memoria: un `byte[]` y un `string`
                // comparten layout, { i64 longitud; datos }.
                else if (isFile && selectedMember.name == "ReadAllBytes" &&
                    selectedMember.declaredTypeKey == "builtin:byte[]" &&
                    selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::FileReadAllBytes;
                }
                // ReadBytes: ruta + arreglo del llamador, cuantos bytes se leyeron fuera. El
                // patron "buffer provisto por el llamador" de Directory.GetFiles, que es el que
                // no asigna y por tanto el que usara el driver.
                else if (isFile && selectedMember.name == "ReadBytes" &&
                    selectedMember.declaredTypeKey == "builtin:int" &&
                    selectedMember.parameters.size() == 2 &&
                    selectedMember.parameters[0].typeKey == "builtin:string" &&
                    selectedMember.parameters[1].typeKey == "builtin:byte[]")
                {
                    ioKind = MethodCoreOperationKind::FileReadBytes;
                }
                // SHR-029. WriteAllBytes: ruta + arreglo + CUANTOS bytes escribir. El contador es
                // explicito y no la longitud del arreglo porque quien construye un buffer casi
                // siempre lo tiene mas grande que lo que ha llenado.
                else if (isFile && selectedMember.name == "WriteAllBytes" &&
                    selectedMember.declaredTypeKey == "builtin:bool" &&
                    selectedMember.parameters.size() == 3 &&
                    selectedMember.parameters[0].typeKey == "builtin:string" &&
                    selectedMember.parameters[1].typeKey == "builtin:byte[]" &&
                    selectedMember.parameters[2].typeKey == "builtin:int")
                {
                    ioKind = MethodCoreOperationKind::FileWriteAllBytes;
                }
                // Bloque 4, fase A -- SHR-030. `File.GetSize(string)` devuelve builtin:long.
                // Misma forma de un argumento que ReadAllText, otro tipo de salida.
                else if (isFile && selectedMember.name == "GetSize" &&
                    selectedMember.declaredTypeKey == "builtin:long" &&
                    selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::FileGetSize;
                }
                else if (isFile && selectedMember.name == "AppendAllText" &&
                    selectedMember.declaredTypeKey == "builtin:void" &&
                    selectedMember.parameters.size() == 2 &&
                    selectedMember.parameters[0].typeKey == "builtin:string" &&
                    selectedMember.parameters[1].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::FileAppendAllText;
                }
                // Stage 53.4 "Filesystem/Paths (cierre)" -- Directory.Delete, calco exacto de
                // File.Delete arriba (mismo one-argument/void shape), solo del lado !isFile.
                else if (!isFile && selectedMember.name == "Delete" &&
                    selectedMember.declaredTypeKey == "builtin:void" &&
                    selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::DirectoryDelete;
                }
                // Stage 53.6 "Enumeracion de directorios" -- GetFileCount/GetDirectoryCount (un
                // builtin:string entra, builtin:int sale) y GetFiles/GetDirectories (un
                // builtin:string + un builtin:string[] entran -- la "buffer provisto por el
                // caller" que GetCommandLineArgs establecio -- builtin:int sale). Ver el
                // comentario de cabecera de Directory.ns para el diseno completo.
                else if (!isFile && selectedMember.name == "GetFileCount" &&
                    selectedMember.declaredTypeKey == "builtin:int" &&
                    selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::DirectoryGetFileCount;
                }
                else if (!isFile && selectedMember.name == "GetFiles" &&
                    selectedMember.declaredTypeKey == "builtin:int" &&
                    selectedMember.parameters.size() == 2 &&
                    selectedMember.parameters[0].typeKey == "builtin:string" &&
                    selectedMember.parameters[1].typeKey == "builtin:string[]")
                {
                    ioKind = MethodCoreOperationKind::DirectoryGetFiles;
                }
                else if (!isFile && selectedMember.name == "GetDirectoryCount" &&
                    selectedMember.declaredTypeKey == "builtin:int" &&
                    selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == "builtin:string")
                {
                    ioKind = MethodCoreOperationKind::DirectoryGetDirectoryCount;
                }
                else if (!isFile && selectedMember.name == "GetDirectories" &&
                    selectedMember.declaredTypeKey == "builtin:int" &&
                    selectedMember.parameters.size() == 2 &&
                    selectedMember.parameters[0].typeKey == "builtin:string" &&
                    selectedMember.parameters[1].typeKey == "builtin:string[]")
                {
                    ioKind = MethodCoreOperationKind::DirectoryGetDirectories;
                }
                // Pre-Stage-56 "utilidades basicas" -- File.CopyRaw/MoveRaw, Directory.MoveRaw.
                // CopyRaw is File-only, three operands (source, dest, overwrite) -- joins
                // StringSubstring in the HIR bucket's expected=3U list. MoveRaw exists on BOTH
                // File and Directory (Win32's MoveFileA works identically for either -- see
                // buildMoveFile in WindowsRuntimeBuilder.cpp, the first runtime hook shared by a
                // File AND a Directory core operation, same "shared wrapper" precedent
                // Semaphore.Wait/Event.Wait established in Stage 54.5), two operands each, falls
                // through to the default expected=2U bucket.
                else if (isFile && selectedMember.name == "CopyRaw" &&
                    selectedMember.declaredTypeKey == "builtin:bool" &&
                    selectedMember.parameters.size() == 3 &&
                    selectedMember.parameters[0].typeKey == "builtin:string" &&
                    selectedMember.parameters[1].typeKey == "builtin:string" &&
                    selectedMember.parameters[2].typeKey == "builtin:bool")
                {
                    ioKind = MethodCoreOperationKind::FileCopyRaw;
                }
                else if (selectedMember.name == "MoveRaw" &&
                    selectedMember.declaredTypeKey == "builtin:bool" &&
                    selectedMember.parameters.size() == 2 &&
                    selectedMember.parameters[0].typeKey == "builtin:string" &&
                    selectedMember.parameters[1].typeKey == "builtin:string")
                {
                    ioKind = isFile ? MethodCoreOperationKind::FileMoveRaw : MethodCoreOperationKind::DirectoryMoveRaw;
                }
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
                    // si cambia el nombre del modulo de la biblioteca), resolvemos el ordinal del
                    // delegate ThreadStart por su nombre completamente calificado y comparamos
                    // contra la key() real -- el mismo patron defensivo que
                    // delegateTypeOrdinalForTypeKey (mas abajo en este archivo) usa yendo en la
                    // direccion inversa (typeKey -> ordinal).
                    bool parameterIsThreadStart = false;
                    if (nameLookup_ != nullptr)
                    {
                        for (const NamedTypeDeclaration& candidate : nameLookup_->namedTypes())
                        {
                            if (candidate.kind == NamedTypeDeclarationKind::Delegate &&
                                candidate.identity.fullyQualifiedName() ==
                                    "NativeSharp.Core.Threading.ThreadStart" &&
                                selectedMember.parameters[0].typeKey == "type:" + candidate.identity.key())
                            {
                                parameterIsThreadStart = true;
                                break;
                            }
                        }
                    }
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
            const bool publicInterlocked =
                atomicDeclaringType == "NativeSharp.Core.Threading.Interlocked" &&
                selectedMember.isStatic && !selectedMember.parameters.empty() &&
                selectedMember.parameters[0].passKind == ParameterPassKind::Ref &&
                (selectedMember.parameters[0].pointeeTypeKey == "builtin:int" ||
                 selectedMember.parameters[0].pointeeTypeKey == "builtin:long" ||
                 selectedMember.parameters[0].pointeeTypeKey == "builtin:void*");
            if ((atomicWrapper && !selectedMember.isStatic) || publicInterlocked)
            {
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
                const auto isMemoryOrderType = [&](const std::string& typeKey)
                {
                    if (inheritanceModel_ == nullptr || nameLookup_ == nullptr) return false;
                    const std::optional<std::size_t> ordinal =
                        inheritanceModel_->typeOrdinalForTypeKey(typeKey);
                    return ordinal.has_value() && *ordinal < nameLookup_->namedTypes().size() &&
                        nameLookup_->namedTypes()[*ordinal].identity.fullyQualifiedName() ==
                            "NativeSharp.Core.Threading.MemoryOrder";
                };
                const bool orderedLoad = selectedMember.name == "Load" &&
                    selectedMember.parameters.size() == parameterOffset + 1U &&
                    isMemoryOrderType(selectedMember.parameters[parameterOffset].typeKey) &&
                    selectedMember.declaredTypeKey == valueTypeKey;
                const bool orderedStore = selectedMember.name == "Store" &&
                    selectedMember.parameters.size() == parameterOffset + 2U &&
                    selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
                    isMemoryOrderType(selectedMember.parameters[parameterOffset + 1U].typeKey) &&
                    selectedMember.declaredTypeKey == "builtin:void";
                const bool orderedUnaryRmw =
                    (selectedMember.name == "Increment" || selectedMember.name == "Decrement") &&
                    selectedMember.parameters.size() == parameterOffset + 1U &&
                    isMemoryOrderType(selectedMember.parameters[parameterOffset].typeKey) &&
                    selectedMember.declaredTypeKey == valueTypeKey;
                const bool orderedValueRmw =
                    (selectedMember.name == "Add" || selectedMember.name == "Exchange") &&
                    selectedMember.parameters.size() == parameterOffset + 2U &&
                    selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
                    isMemoryOrderType(selectedMember.parameters[parameterOffset + 1U].typeKey) &&
                    selectedMember.declaredTypeKey == valueTypeKey;
                const bool orderedRmw = orderedUnaryRmw || orderedValueRmw;
                const bool orderedCompareExchange = selectedMember.name == "CompareExchange" &&
                    selectedMember.parameters.size() == parameterOffset + 4U &&
                    selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
                    selectedMember.parameters[parameterOffset + 1U].typeKey == valueTypeKey &&
                    isMemoryOrderType(selectedMember.parameters[parameterOffset + 2U].typeKey) &&
                    isMemoryOrderType(selectedMember.parameters[parameterOffset + 3U].typeKey) &&
                    selectedMember.declaredTypeKey == valueTypeKey;
                std::optional<MethodCoreOperationKind> atomicKind;
                if ((selectedMember.name == "Load" && selectedMember.parameters.size() == parameterOffset &&
                        selectedMember.declaredTypeKey == valueTypeKey) || orderedLoad)
                {
                    atomicKind = isPointer
                        ? MethodCoreOperationKind::AtomicLoadPointer
                        : isInt32 ? MethodCoreOperationKind::AtomicLoadInt32
                                  : MethodCoreOperationKind::AtomicLoadInt64;
                }
                else if ((selectedMember.name == "Store" &&
                    selectedMember.parameters.size() == parameterOffset + 1U &&
                    selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
                    selectedMember.declaredTypeKey == "builtin:void") || orderedStore)
                {
                    atomicKind = isPointer
                        ? MethodCoreOperationKind::AtomicStorePointer
                        : isInt32 ? MethodCoreOperationKind::AtomicStoreInt32
                                  : MethodCoreOperationKind::AtomicStoreInt64;
                }
                else if (!isPointer &&
                    ((selectedMember.name == "Increment" && selectedMember.parameters.size() == parameterOffset &&
                    selectedMember.declaredTypeKey == valueTypeKey) || (orderedRmw && selectedMember.name == "Increment")))
                {
                    atomicKind = isInt32
                        ? MethodCoreOperationKind::InterlockedIncrementInt32
                        : MethodCoreOperationKind::InterlockedIncrementInt64;
                }
                else if (!isPointer &&
                    ((selectedMember.name == "Decrement" && selectedMember.parameters.size() == parameterOffset &&
                    selectedMember.declaredTypeKey == valueTypeKey) || (orderedRmw && selectedMember.name == "Decrement")))
                {
                    atomicKind = isInt32
                        ? MethodCoreOperationKind::InterlockedDecrementInt32
                        : MethodCoreOperationKind::InterlockedDecrementInt64;
                }
                else if (!isPointer &&
                    ((selectedMember.name == "Add" && selectedMember.parameters.size() == parameterOffset + 1U &&
                    selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
                    selectedMember.declaredTypeKey == valueTypeKey) ||
                    (orderedRmw && selectedMember.name == "Add")))
                {
                    atomicKind = isInt32
                        ? MethodCoreOperationKind::InterlockedAddInt32
                        : MethodCoreOperationKind::InterlockedAddInt64;
                }
                else if ((selectedMember.name == "Exchange" && selectedMember.parameters.size() == parameterOffset + 1U &&
                    selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
                    selectedMember.declaredTypeKey == valueTypeKey) ||
                    (orderedRmw && selectedMember.name == "Exchange"))
                {
                    atomicKind = isPointer
                        ? MethodCoreOperationKind::InterlockedExchangePointer
                        : isInt32 ? MethodCoreOperationKind::InterlockedExchangeInt32
                                  : MethodCoreOperationKind::InterlockedExchangeInt64;
                }
                else if ((selectedMember.name == "CompareExchange" && selectedMember.parameters.size() == parameterOffset + 2U &&
                    selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
                    selectedMember.parameters[parameterOffset + 1U].typeKey == valueTypeKey &&
                    selectedMember.declaredTypeKey == valueTypeKey) || orderedCompareExchange)
                {
                    atomicKind = isPointer
                        ? MethodCoreOperationKind::InterlockedCompareExchangePointer
                        : isInt32 ? MethodCoreOperationKind::InterlockedCompareExchangeInt32
                                  : MethodCoreOperationKind::InterlockedCompareExchangeInt64;
                }
                std::optional<std::uint8_t> atomicMemoryOrder;
                std::optional<std::uint8_t> atomicFailureMemoryOrder;
                if (atomicKind.has_value() && orderedCompareExchange)
                {
                    const ast::AstNode* successSyntax = expression->children().size() >= parameterOffset + 3U
                        ? expression->children()[parameterOffset + 2U]
                        : nullptr;
                    const ast::AstNode* failureSyntax = expression->children().size() >= parameterOffset + 4U
                        ? expression->children()[parameterOffset + 3U]
                        : nullptr;
                    const std::optional<ProjectIntegerValue> foldedSuccess =
                        foldIntegerConstant(successSyntax, selectedMember.declaringTypeOrdinal);
                    const std::optional<ProjectIntegerValue> foldedFailure =
                        foldIntegerConstant(failureSyntax, selectedMember.declaringTypeOrdinal);
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
                    }
                    else
                    {
                        const std::uint64_t success = foldedSuccess->bits;
                        const std::uint64_t failure = foldedFailure->bits;
                        const bool successLegal = success <= 4U;
                        const bool failureLegal = failure == 0U || failure == 1U || failure == 4U;
                        const bool failureNotStronger =
                            (success == 0U && failure == 0U) ||
                            (success == 1U && (failure == 0U || failure == 1U)) ||
                            (success == 2U && failure == 0U) ||
                            (success == 3U && (failure == 0U || failure == 1U)) ||
                            (success == 4U && failureLegal);
                        if (!successLegal)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                successSyntax != nullptr ? successSyntax->range() : expression->range(),
                                "atomic compare-exchange success order is outside MemoryOrder");
                            atomicKind.reset();
                        }
                        else if (!failureLegal)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                failureSyntax != nullptr ? failureSyntax->range() : expression->range(),
                                "atomic compare-exchange failure order must be Relaxed, Acquire, or SequentiallyConsistent");
                            atomicKind.reset();
                        }
                        else if (!failureNotStronger)
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
                }
                else if (atomicKind.has_value() && (orderedLoad || orderedStore || orderedRmw))
                {
                    const ast::AstNode* orderSyntax = expression->children().empty()
                        ? nullptr
                        : expression->children().back();
                    const std::optional<ProjectIntegerValue> foldedOrder =
                        foldIntegerConstant(orderSyntax, selectedMember.declaringTypeOrdinal);
                    if (!foldedOrder.has_value())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::ConstRequiresConstant,
                            diagnostics::DiagnosticSeverity::Error,
                            orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
                            "atomic memory order must be a compile-time MemoryOrder constant");
                        atomicKind.reset();
                    }
                    else
                    {
                        const std::uint64_t order = foldedOrder->bits;
                        const bool legal = orderedLoad
                            ? (order == 0U || order == 1U || order == 4U)
                            : orderedStore
                                ? (order == 0U || order == 2U || order == 4U)
                                : order <= 4U;
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
                        }
                        else
                        {
                            atomicMemoryOrder = static_cast<std::uint8_t>(order);
                        }
                    }
                }
                if (atomicKind.has_value())
                {
                    std::optional<std::size_t> valueFieldOrdinal;
                    if (atomicWrapper && memberModel_ != nullptr)
                    {
                        for (std::size_t candidateOrdinal = 0;
                            candidateOrdinal < memberModel_->members().size(); ++candidateOrdinal)
                        {
                            const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                            if (candidate.kind == ClassMemberKind::Field &&
                                candidate.declaringTypeOrdinal == selectedMember.declaringTypeOrdinal &&
                                candidate.name == "value_")
                            {
                                valueFieldOrdinal = candidateOrdinal;
                                break;
                            }
                        }
                    }
                    if (publicInterlocked || valueFieldOrdinal.has_value())
                    {
                        MethodCoreBinding binding{
                            *atomicKind,
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
                }
            }
            // Item 307: static fences consume MemoryOrder during semantic analysis. The source
            // argument never becomes a runtime integer; HIR carries only the validated ABI value.
            if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.AtomicFence" &&
                selectedMember.isStatic && selectedMember.declaredTypeKey == "builtin:void" &&
                selectedMember.parameters.size() == 1 && inheritanceModel_ != nullptr &&
                nameLookup_ != nullptr)
            {
                const std::optional<std::size_t> orderTypeOrdinal =
                    inheritanceModel_->typeOrdinalForTypeKey(selectedMember.parameters[0].typeKey);
                const bool isMemoryOrder = orderTypeOrdinal.has_value() &&
                    *orderTypeOrdinal < nameLookup_->namedTypes().size() &&
                    nameLookup_->namedTypes()[*orderTypeOrdinal].identity.fullyQualifiedName() ==
                        "NativeSharp.Core.Threading.MemoryOrder";
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
                        foldIntegerConstant(orderSyntax, selectedMember.declaringTypeOrdinal);
                    if (!foldedOrder.has_value())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::ConstRequiresConstant,
                            diagnostics::DiagnosticSeverity::Error,
                            orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
                            "atomic fence order must be a compile-time MemoryOrder constant");
                    }
                    else if (foldedOrder->bits < 1U || foldedOrder->bits > 4U)
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
                    for (std::size_t candidateOrdinal = 0;
                        candidateOrdinal < memberModel_->members().size(); ++candidateOrdinal)
                    {
                        const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                        if (candidate.kind != ClassMemberKind::Field ||
                            candidate.declaringTypeOrdinal != selectedMember.declaringTypeOrdinal)
                        {
                            continue;
                        }
                        if (candidate.name == "lock_")
                        {
                            lockFieldOrdinal = candidateOrdinal;
                        }
                        else if (candidate.name == "cv_")
                        {
                            cvFieldOrdinal = candidateOrdinal;
                        }
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
    }

    result.typeKey = call.resultTypeKey;
    result.valid = !isErrorType(result.typeKey);
    calls_.push_back(std::move(call));
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

    bool hasMatchingArity = false;
    if (memberModel_ != nullptr)
    {
        for (const std::size_t candidateOrdinal : call.candidateMemberOrdinals)
        {
            if (candidateOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
            if (candidate.kind != ClassMemberKind::Method) continue;

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
                ProjectImplicitConversion conversion = classifyImplicit(
                    arguments[argumentOrdinal].typeKey,
                    candidate.parameters[argumentOrdinal].typeKey,
                    arguments[argumentOrdinal].integerConstant);
                if (!conversion.exists()) candidateBinding.viable = false;
                candidateBinding.conversions.push_back(std::move(conversion));
            }
            if (candidateBinding.viable) call.viableCandidateMemberOrdinals.push_back(candidateOrdinal);
            call.evaluatedCandidates.push_back(std::move(candidateBinding));
        }
    }

    if (!hasMatchingArity)
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
        calls_.push_back(std::move(call));
        return result;
    }

    std::vector<std::size_t> viableEvaluationOrdinals;
    for (std::size_t index = 0; index < call.evaluatedCandidates.size(); ++index)
    {
        if (call.evaluatedCandidates[index].viable) viableEvaluationOrdinals.push_back(index);
    }
    if (viableEvaluationOrdinals.empty())
    {
        if (diagnoseFailure)
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
                for (std::size_t index = 0; index < arguments.size() && index < only.parameters.size(); ++index)
                {
                    if (only.parameters[index].passKind != arguments[index].argumentPassKind)
                    {
                        reportedPassKind = true;
                        break;
                    }
                }
                if (reportedPassKind)
                {
                    checkArgumentPassKinds(syntax, call.methodName, soleCandidate, arguments, diagnostics);
                }
            }
            if (!reportedPassKind)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    syntax != nullptr ? syntax->range() : SourceRange{},
                    "no viable Stage 38 overload of '" + call.methodName + "' accepts the operand/argument types");
            }
        }
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        calls_.push_back(std::move(call));
        return result;
    }

    std::vector<std::size_t> nonDominated;
    for (const std::size_t candidateIndex : viableEvaluationOrdinals)
    {
        bool dominated = false;
        for (const std::size_t otherIndex : viableEvaluationOrdinals)
        {
            if (candidateIndex == otherIndex) continue;
            if (betterCandidate(call.evaluatedCandidates[otherIndex], call.evaluatedCandidates[candidateIndex]))
            {
                dominated = true;
                break;
            }
        }
        if (!dominated) nonDominated.push_back(candidateIndex);
    }
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
        calls_.push_back(std::move(call));
        return result;
    }

    const MethodCallCandidate& selected = call.evaluatedCandidates[nonDominated.front()];
    if (memberModel_ != nullptr && selected.memberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
        if (readonlyReceiver && selectedMember.declaringTypeIsStruct &&
            !selectedMember.isStatic && !selectedMember.isReadonly)
        {
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
            calls_.push_back(std::move(call));
            return result;
        }
    }
    if (diagnoseFailure)
    {
        checkArgumentPassKinds(syntax, call.methodName, selected.memberOrdinal, arguments, diagnostics);
    }
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedMemberOrdinal = selected.memberOrdinal;
    call.selectedConversions = selected.conversions;
    if (memberModel_ != nullptr && selected.memberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& member = memberModel_->members()[selected.memberOrdinal];
        call.resultTypeKey = member.declaredTypeKey;
        call.staticCall = member.isStatic;
        result.assignable = member.isReferenceReturn;
        result.addressable = member.isReferenceReturn;
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
    calls_.push_back(std::move(call));
    return result;
}

std::vector<std::size_t> ProjectMethodBodyModel::stage38AccessorCandidates(
    std::size_t receiverTypeOrdinal,
    std::string_view accessorName,
    bool propertyAccessor,
    bool getter,
    bool typeReceiver,
    std::size_t accessingTypeOrdinal,
    std::size_t protectedReceiverTypeOrdinal) const
{
    std::vector<std::size_t> result;
    if (memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr) return result;
    const bool interfaceReceiver = interfaceModel_ != nullptr && interfaceModel_->isInterface(receiverTypeOrdinal);
    for (const std::size_t ordinal : memberCandidatesForType(receiverTypeOrdinal, accessorName))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.kind != ClassMemberKind::Method) continue;
        if (propertyAccessor ? !member.isPropertyAccessor : !member.isIndexerAccessor) continue;
        if (getter != member.isGetter) continue;
        if (typeReceiver != member.isStatic) continue;
        if ((!interfaceReceiver && !ProjectMemberAccessibility::isAccessible(
                member,
                accessingTypeOrdinal,
                protectedReceiverTypeOrdinal,
                *nameLookup_,
                *inheritanceModel_)) ||
            (interfaceReceiver && member.accessibility != ClassMemberAccessibility::Public))
        {
            continue;
        }
        result.push_back(ordinal);
    }
    return result;
}

std::vector<std::size_t> ProjectMethodBodyModel::stage38OperatorCandidates(
    std::string_view methodName,
    std::string_view leftTypeKey,
    std::optional<std::string_view> rightTypeKey) const
{
    std::vector<std::size_t> result;
    std::unordered_set<std::size_t> seen;
    if (inheritanceModel_ == nullptr || memberModel_ == nullptr) return result;
    const auto appendForType = [&](std::string_view typeKey)
    {
        // C6 "struct members": a user-defined operator is a STATIC method, so nothing about
        // resolving or calling one depends on whether its type is a class or a struct. This used
        // to filter on `isClass`, and that single condition was the entire reason `a + b` on a
        // struct reported "binary operator requires compatible numeric operands" -- the candidate
        // was never even looked for. Measured after removing it: a Vec2 operator+ resolves, and
        // the 8-byte struct travels directly in an i64, which is the correct Win64 ABI for that
        // size. That is what makes vector/matrix math -- the reason structs exist in a game
        // framework -- writable the way it is written everywhere else.
        const std::optional<std::size_t> typeOrdinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
        if (!typeOrdinal.has_value()) return;
        for (const std::size_t ordinal : memberModel_->findMembers(*typeOrdinal, methodName))
        {
            if (ordinal >= memberModel_->members().size() || seen.contains(ordinal)) continue;
            const ClassMemberSymbol& member = memberModel_->members()[ordinal];
            if (!member.isUserOperator || !member.isStatic) continue;
            seen.insert(ordinal);
            result.push_back(ordinal);
        }
    };
    appendForType(leftTypeKey);
    if (rightTypeKey.has_value()) appendForType(*rightTypeKey);
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeUnary(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // C7 "definite assignment": `f(out x)` is the one argument form that ASSIGNS its operand
    // instead of reading it -- the callee is obliged to write it (verifyOutParameterAssignments
    // enforces that half) -- so `int x; f(out x); use(x);` must be accepted, and the `out x`
    // itself must not be reported as a read. `ref x` and `in x` are ordinary reads and get no
    // exemption: passing an unassigned local by reference is the same bug as reading it.
    const ast::AstNode* const previousWriteTarget = definiteAssignmentWriteTarget_;
    const bool assignsOperand = expression->tokenKind() == TokenKind::Amp &&
        expression->auxiliaryText() == "pass:out";
    if (assignsOperand)
    {
        markAssignmentTarget(expression->first(), scopeOrdinal);
    }
    ExpressionResult operand = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    definiteAssignmentWriteTarget_ = previousWriteTarget;
    if (!operand.valid)
    {
        return {};
    }

    // EL MENOS UNARIO SOBRE UN LITERAL SE TIPA JUNTO CON EL, y es la unica forma de escribir el
    // minimo de un ancho: `int.MinValue` vale 2147483648 en magnitud, uno mas que el maximo de
    // `int`, asi que tipar el literal por su cuenta lo hace `long` y arrastra a toda la expresion.
    // Antes habia que escribir `-2147483647 - 1`; ese rodeo estaba en la fixture c30 y en
    // NumericLimits.ns.
    //
    // Es el caso especial de C#, y con su mismo limite: se aplica al literal que esta
    // DIRECTAMENTE debajo del menos. `-(2147483648)` con parentesis no es este caso y sigue
    // siendo `long`.
    //
    // Se re-tipa despues de analizar en vez de pasar la bandera hacia abajo porque el literal es
    // una hoja: analizarlo no tiene efectos que rehacer, y una bandera de contexto tendria que
    // cruzar todo analyzeExpression para servir a un solo nodo.
    if (expression->kind() == ast::AstNodeKind::UnaryExpression &&
        expression->tokenKind() == TokenKind::Minus && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IntegerLiteralExpression)
    {
        operand.typeKey = integerLiteralType(expression->first(), true);
        // Y HAY QUE RE-REGISTRARLO, no solo corregir la copia local. El backend no lee este
        // `ExpressionResult`: lee `expressionTypes_`, donde el literal quedo anotado como `long`
        // cuando se analizo por su cuenta. Sin esta linea el modelo semantico dice `int` y el HIR
        // dice `long`, y lo que sale es un `store i64` sobre un `alloca i32` -- ocho bytes en una
        // ranura de cuatro. Se vio en el IR, no en un rojo.
        //
        // El mapa del HIR se construye con `expressionTypeBySyntax_[syntax] = &binding`, asi que
        // la ANOTACION MAS RECIENTE gana. Por eso basta con volver a anotar.
        recordExpressionType(expression->first(), methodMemberOrdinal, scopeOrdinal, operand);
    }

    ExpressionResult result;
    const TokenKind operation = expression->tokenKind();

    if (operation != TokenKind::Amp && operation != TokenKind::Star &&
        operation != TokenKind::PlusPlus && operation != TokenKind::MinusMinus)
    {
        const std::string operatorName = stage38OperatorMethodName(operation, 1);
        if (!operatorName.empty())
        {
            std::vector<std::size_t> candidates = stage38OperatorCandidates(
                operatorName, operand.typeKey, std::nullopt);
            if (!candidates.empty())
            {
                const std::vector<ExpressionResult> arguments = {operand};
                ExpressionResult overloaded = resolveSyntheticCall(
                    expression,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    operatorName,
                    std::move(candidates),
                    arguments,
                    nullptr,
                    {},
                    std::nullopt,
                    false,
                    false,
                    diagnostics,
                    false);
                if (overloaded.valid)
                {
                    overloaded.assignable = false;
                    return overloaded;
                }
            }
        }
    }
    if (operation == TokenKind::Bang)
    {
        if (operand.typeKey != "builtin:bool")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "operator ! requires bool operand");
            return result;
        }
        result.typeKey = "builtin:bool";
        result.valid = true;
        return result;
    }

    if (operation == TokenKind::Amp)
    {
        if (!operand.addressable || !operand.assignable)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAddressOf,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "operand of & must be mutable addressable storage; bit fields have no address");
            return result;
        }
        result.typeKey = operand.typeKey + '*';
        result.valid = true;
        // C4: `f(ref x)` parses as the address-of expression the callee's pointer parameter
        // expects, carrying the keyword the programmer wrote so the call can be checked against
        // the signature. An `out` argument is the one case that does NOT require the operand to
        // hold a value first -- the callee is the one that assigns it -- but it still has to be
        // assignable, which the check above already established.
        if (expression != nullptr && expression->auxiliaryText().starts_with("pass:"))
        {
            const std::string spelling = expression->auxiliaryText().substr(std::string("pass:").size());
            result.argumentPassKind = spelling == "ref"
                ? ParameterPassKind::Ref
                : (spelling == "out" ? ParameterPassKind::Out : ParameterPassKind::In);
        }
        return result;
    }

    if (operation == TokenKind::Star)
    {
        if (!ProjectTypeConversion::isPointer(operand.typeKey) || operand.typeKey == "builtin:void*")
        {
            // IDs 80/83. Desde que el parser convierte `p->x` en `(*p).x`, este `*` puede ser uno
            // que la persona NO escribio. Un mensaje que hable de `*` ahi es desorientador -- lo
            // descubrio `nsc.project-member-model`, que comprueba que `this->Value` se rechaza CON
            // DiagnosticId::InvalidMemberAccess, y dejo de pasar. El nodo sintetizado se marca en
            // el parser, asi que aqui se puede nombrar lo que de verdad se escribio, y salir con
            // el mismo id con el que salia cuando `->` tenia nodo propio.
            const bool writtenAsArrow = expression != nullptr && expression->auxiliaryText() == "arrow";
            diagnostics.report(
                writtenAsArrow
                    ? diagnostics::DiagnosticId::InvalidMemberAccess
                    : diagnostics::DiagnosticId::InvalidDereference,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                writtenAsArrow
                    ? "left side of '->' must be a non-void pointer, but '" + operand.typeKey +
                        "' is not one; use '.' for a value or a class reference"
                    : "operand of * must be a non-void pointer");
            return result;
        }
        result.typeKey = operand.typeKey.substr(0, operand.typeKey.size() - 1);
        result.valid = true;
        result.assignable = true;
        result.addressable = true;
        return result;
    }

    if (operation == TokenKind::PlusPlus || operation == TokenKind::MinusMinus)
    {
        // ID 79. Un puntero NO VOID tambien se incrementa, y avanza un ELEMENTO -- la misma regla
        // que `p + 1`, porque es la misma operacion. Dejarlo fuera repetiria al reves el error de
        // la tarea 62: `++i` andando y `++p` no, cuando lo unico que cambia es el paso.
        // `void*` queda fuera por la misma razon por la que `*p` lo rechaza: no hay tamano que
        // avanzar.
        const bool pointerOperand = ProjectTypeConversion::isPointer(operand.typeKey) &&
            operand.typeKey != "builtin:void*";
        if (!operand.assignable ||
            !(ProjectTypeConversion::isNumeric(operand.typeKey) || pointerOperand))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "increment/decrement requires a modifiable numeric or non-void pointer operand");
            return result;
        }
        result.typeKey = operand.typeKey;
        result.valid = true;
        return result;
    }

    if (operation == TokenKind::Plus || operation == TokenKind::Minus || operation == TokenKind::Tilde)
    {
        // NSC-LIMIT-012. `~enum` preserves the enum identity; unary + and - remain numeric-only.
        const bool enumComplement = operation == TokenKind::Tilde && isEnumTypeKey(operand.typeKey);
        if ((!ProjectTypeConversion::isNumeric(operand.typeKey) || operand.typeKey == "builtin:bool") &&
            !enumComplement)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "unary numeric operator requires numeric operand");
            return result;
        }
        result.typeKey = operand.typeKey;
        result.valid = true;
        if (operation == TokenKind::Plus)
        {
            result.integerConstant = operand.integerConstant;
        }
        else if (operation == TokenKind::Minus && operand.integerConstant.has_value() &&
                 !operand.integerConstant->isUnsigned && !operand.integerConstant->isNegative)
        {
            // ID 119. `-3` no conservaba su constancia: unario `+` la propagaba y unario `-` no,
            // asi que `short s = 3;` compilaba y `short s = -3;` NO -- el mismo literal, con signo,
            // dejaba de caber. La descubrio la fixture c13 al declarar `short smallS = -3;`.
            //
            // Se guarda la MAGNITUD mas la bandera, no el valor negado: `value` se compara contra
            // cotas superiores en constantFits, y 0xFFFF...FD no pasaria ninguna. Ver
            // ProjectIntegerConstant::isNegative.
            //
            // No se dobla `-(-3)` ni `-(3u)`: en el primero la constancia ya venia marcada
            // negativa y en el segundo el literal es sin signo. Los dos son raros y ninguno vale
            // el riesgo de una regla de signo escrita a medias.
            ProjectIntegerConstant negated = *operand.integerConstant;
            negated.isNegative = negated.value != 0;
            result.integerConstant = negated;
        }
        return result;
    }

    result = operand;
    result.assignable = false;
    return result;
}

// ¿ESTE DIVISOR ES UN CERO QUE EL COMPILADOR YA CONOCE?
//
// Dos formas, y la segunda es la que faltaba:
//
//   * una CONSTANTE de la propia expresion -- `x / 0`, `x / (2 - 2)`, un miembro `const` de tipo.
//     Esas llegan plegadas en `ExpressionResult::integerConstant`.
//   * un LOCAL `const` cuyo valor es cero -- `const int cero = 0; ... x / cero;`. Ese no llegaba:
//     `integerConstant` no se rellena para un identificador, y a proposito, porque ese campo lo
//     consume `commonNumericType` para decidir promociones y darle valor a cada `const` local
//     cambiaria el tipo de expresiones que hoy compilan.
//
// Por eso el valor del local se guarda aparte, al declararlo (MethodLocalSymbol::
// constantIntegerValue), y se lee aqui y en ningun otro sitio.
//
// LO QUE SIGUE FUERA, dicho para que nadie lo herede como cubierto y MEDIDO, no supuesto:
//
//   * `const int a = 5; const int b = 5; const int cero = a - b;` -- un `const` local derivado de
//     OTROS `const` locales NO se pliega, asi que `x / cero` compila. El motivo es concreto: el
//     evaluador resuelve nombres a traves de un ConstantMemberResolver que sabe de MIEMBROS
//     `const` de tipo, no de locales, y los locales dependen del ambito, que el evaluador no
//     tiene. Cerrarlo es darle al resolvedor un espacio de ordinales para locales; se puede, y no
//     se ha hecho.
//   * cualquier divisor que no sea constante.
//
// Para los dos, la guarda en tiempo de ejecucion es la respuesta y es correcta: el programa muere
// con DivisionByZero en vez de compilar a poison. Esto solo adelanta el aviso donde se puede.
bool ProjectMethodBodyModel::divisorIsConstantZero(
    const ast::AstNode* divisorSyntax,
    const ExpressionResult& divisor,
    std::size_t scopeOrdinal) const
{
    if (divisor.integerConstant.has_value() && divisor.integerConstant->value == 0) return true;
    if (divisorSyntax == nullptr ||
        divisorSyntax->kind() != ast::AstNodeKind::IdentifierExpression)
    {
        return false;
    }
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, divisorSyntax->text());
    if (!name.has_value() || name->kind != ScopeNameKind::Local ||
        name->semanticOrdinal >= locals_.size())
    {
        return false;
    }
    const MethodLocalSymbol& local = locals_[name->semanticOrdinal];
    return local.isConst && local.constantIntegerValue.has_value() &&
        *local.constantIntegerValue == 0;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeBinary(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const TokenKind operation = expression->tokenKind();

    const ExpressionResult left = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);

    // CC1-207 / CC1-208. `&&` y `||` NO evaluan siempre su operando derecho, y el backend ya lo
    // sabe: lowerShortCircuit emite un salto condicional y un phi de verdad. El analisis, en
    // cambio, analizaba los dos operandos en secuencia y despues miraba que operador era -- asi que
    // todo lo que el RHS produjera (una asignacion, un move, un borrow) quedaba registrado como si
    // hubiera corrido siempre.
    //
    // El caso concreto: `c && (x = 1)` daba `x` por definitivamente asignada. Definite assignment
    // es un conjunto MUST cuyo join es una INTERSECCION, y analizar en secuencia equivale a UNIR,
    // que es la direccion insegura. Es el mismo defecto que tenia `?:`, y se arregla con la misma
    // pieza -- no con dos parches locales distintos.
    //
    // La rama "RHS no evaluado" es el estado tal cual quedo tras el operando izquierdo: en ese
    // camino el corto-circuito ya decidio el resultado y nada mas se ejecuta.
    const bool shortCircuits = operation == TokenKind::AmpAmp || operation == TokenKind::PipePipe;
    ExpressionResult right;
    if (shortCircuits)
    {
        const OwnershipFlowSnapshot afterLeft = captureOwnershipFlow();
        const OwnershipFlowSnapshot rhsEvaluated = analyzeBranchState(
            afterLeft,
            [&]
            {
                right = analyzeExpression(
                    expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            });
        commitBranchJoin(rhsEvaluated, afterLeft);
    }
    else
    {
        right = analyzeExpression(
            expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    }

    if (!left.valid || !right.valid)
    {
        return {};
    }

    ExpressionResult result;

    const auto isStringLike = [](std::string_view typeKey) noexcept
    {
        return typeKey == "builtin:string" || typeKey == "string-literal";
    };
    if (isStringLike(left.typeKey) && isStringLike(right.typeKey))
    {
        if (operation == TokenKind::Plus)
        {
            result.typeKey = "builtin:string";
            result.valid = true;
            result.ownership = sema::OwnershipKind::Owned;
            result.ownershipTransfer = true;
            result.ownershipFreshOwner = true;
            coreBindings_.push_back(MethodCoreBinding{
                MethodCoreOperationKind::StringConcat,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                result.typeKey,
                "builtin:string",
                false, std::nullopt, std::nullopt});
            return result;
        }
        if (operation == TokenKind::EqualEqual || operation == TokenKind::BangEqual)
        {
            result.typeKey = "builtin:bool";
            result.valid = true;
            coreBindings_.push_back(MethodCoreBinding{
                MethodCoreOperationKind::StringEquals,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                result.typeKey,
                "builtin:string",
                false, std::nullopt, std::nullopt});
            return result;
        }
    }

    const std::string operatorName = stage38OperatorMethodName(operation, 2);
    // A comparison against the `null` LITERAL does not bind to a user-defined operator. Without
    // this, the guard every `operator ==` has to open with was itself the recursion:
    //
    //     public static bool operator ==(Node left, Node right)
    //     {
    //         if (left == null) { return right == null; }   // <- calls op_Equality(left, null)
    //
    // The static type of `left` is `Node`, so `left == null` resolved to the user operator like
    // any other comparison, and the emitted IR called op_Equality with a null second argument --
    // five recursive calls in a six-line operator. It compiled clean and died of stack overflow.
    //
    // This is a DELIBERATE DEVIATION from C#, and worth saying so plainly: in real C# that line
    // does recurse, and the escapes are `(object)left == null` or `left is null`. NativeSharp has
    // no `is null`, and until C8 had no `object` to cast to either, so the trap had no exit at
    // all. It is a bad default for a language without a GC and without those escapes -- the cost
    // of the deviation is that a user operator can no longer observe a comparison against the
    // null literal, which is a thing nobody wants; the benefit is that the obvious, correct-
    // looking guard stops being a hang. A comparison against null is reference identity, and
    // that is what it now lowers to.
    const bool nullLiteralOperand =
        (expression->first() != nullptr &&
         expression->first()->kind() == ast::AstNodeKind::NullLiteralExpression) ||
        (expression->second() != nullptr &&
         expression->second()->kind() == ast::AstNodeKind::NullLiteralExpression);
    if (!operatorName.empty() && !nullLiteralOperand)
    {
        std::vector<std::size_t> candidates = stage38OperatorCandidates(
            operatorName, left.typeKey, right.typeKey);
        if (!candidates.empty())
        {
            const std::vector<ExpressionResult> arguments = {left, right};
            ExpressionResult overloaded = resolveSyntheticCall(
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                operatorName,
                std::move(candidates),
                arguments,
                nullptr,
                {},
                std::nullopt,
                false,
                false,
                diagnostics,
                false);
            if (overloaded.valid)
            {
                overloaded.assignable = false;
                return overloaded;
            }
        }
    }
    if (isLogical(operation))
    {
        if (left.typeKey != "builtin:bool" || right.typeKey != "builtin:bool")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "logical operators require bool operands");
            return result;
        }
        result.typeKey = "builtin:bool";
        result.valid = true;
        return result;
    }

    // NSC-LIMIT-012. Bitwise operations between values of the exact same enum preserve that enum.
    // Mixing enum/integer or two distinct enums remains invalid; this does not introduce general
    // implicit numeric conversions for enums.
    const bool bitwise = operation == TokenKind::Amp || operation == TokenKind::Pipe ||
        operation == TokenKind::Caret;
    if (bitwise && (isEnumTypeKey(left.typeKey) || isEnumTypeKey(right.typeKey)))
    {
        if (left.typeKey != right.typeKey || !isEnumTypeKey(left.typeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "bitwise enum operands must have exactly the same enum type");
            return result;
        }
        result.typeKey = left.typeKey;
        result.valid = true;
        return result;
    }

    // C#'s narrow implicit enumeration conversion: the integer literal zero may stand for the
    // zero value of any enum. Keep this local to equality/inequality; it does not make arbitrary
    // integers comparable with enums, and it does not erase the nominal distinction between two
    // different enum types.
    const auto isZeroIntegerLiteral = [](const ast::AstNode* syntax) noexcept
    {
        return syntax != nullptr &&
            syntax->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
            syntax->integerValue() == 0;
    };
    const bool enumZeroComparison =
        (operation == TokenKind::EqualEqual || operation == TokenKind::BangEqual) &&
        ((isEnumTypeKey(left.typeKey) && isZeroIntegerLiteral(expression->second())) ||
         (isEnumTypeKey(right.typeKey) && isZeroIntegerLiteral(expression->first())));
    if (enumZeroComparison)
    {
        result.typeKey = "builtin:bool";
        result.valid = true;
        return result;
    }

    // NSC-LIMIT-004. `left == right` SOBRE DOS STRUCTS.
    //
    // Va antes del bloque generico de comparacion y despues de los operadores definidos por el
    // usuario, que es el orden correcto: un struct que declara su propio `operator ==` gana, y
    // esto es lo que hace el resto.
    //
    // LO QUE PASABA ANTES. El bloque de abajo acepta cualquier par de operandos con el MISMO
    // typeKey. Dos structs lo cumplen, asi que la comparacion salia valida, bajaba a un nodo
    // `Binary` corriente, y en el backend `applyBinaryOperation` terminaba llamando a
    // `LLVMBuildICmp` sobre un agregado -- `icmp eq [24 x i8]`, que no es una instruccion legal.
    // El defecto no era el backend: era que la semantica habia dicho que si.
    //
    // LA SEMANTICA QUE SE ELIGE ES LA QUE YA EXISTE. No se inventa una segunda: `==` sobre
    // structs es exactamente `Struct.Equals`, o sea comparacion bit a bit de cada hoja escalar
    // declarada, recursiva, SIN padding. Se registra el mismo `MethodCoreOperationKind` y el
    // lowering distingue una llamada `.Equals` de un operador por la clase del nodo de sintaxis.
    if (isComparison(operation) && (typeKeyIsStruct(left.typeKey) || typeKeyIsStruct(right.typeKey)))
    {
        if (left.typeKey != right.typeKey)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "cannot compare '" + left.typeKey + "' with '" + right.typeKey +
                    "': a struct compares only against its own type");
            return result;
        }
        if (operation != TokenKind::EqualEqual && operation != TokenKind::BangEqual)
        {
            // ORDENAR no es comparar por igualdad. Un struct no tiene orden natural: sus campos
            // no dicen cual pesa mas, y adivinarlo --orden de declaracion, por ejemplo-- daria una
            // respuesta estable y arbitraria, que es la peor clase. C tampoco ordena structs.
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "struct '" + left.typeKey + "' supports only '==' and '!=': there is no ordering "
                    "over its fields; declare your own comparison if you need one");
            return result;
        }
        if (!aggregateAllowsSynthesizedMember(left.typeKey, "Equals"))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "record '" + left.typeKey +
                    "' does not derive Equality; add derive(Equality) or declare operator ==");
            return result;
        }
        if (aggregateIsUnion(left.typeKey))
        {
            // La misma razon que rechaza `union.Equals(...)`: solo un miembro esta vivo y nada en
            // el valor dice cual. Se repite el contrato en vez de dejarlo pasar por otra puerta.
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "union '" + left.typeKey + "' cannot be compared with '==' or '!=': only one "
                    "member of a union is live at a time, so comparing every field would be "
                    "meaningless; declare your own comparison");
            return result;
        }
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::StructEquals,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            "builtin:bool",
            left.typeKey,
            false, std::nullopt, std::nullopt});
        result.typeKey = "builtin:bool";
        result.valid = true;
        result.assignable = false;
        return result;
    }

    if (isComparison(operation))
    {
        const bool referenceCompatible =
            classifyImplicit(left.typeKey, right.typeKey, left.integerConstant).exists() ||
            classifyImplicit(right.typeKey, left.typeKey, right.integerConstant).exists();
        if (left.typeKey == right.typeKey ||
            ProjectTypeConversion::commonNumericType(
                left.typeKey, right.typeKey, left.integerConstant, right.integerConstant).has_value() ||
            referenceCompatible)
        {
            result.typeKey = "builtin:bool";
            result.valid = true;
            return result;
        }
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "comparison operands have incompatible Stage 34 types");
        return result;
    }

    // A shift is not a symmetric arithmetic operator, and treating it as one was the bug. Its
    // result type is the LEFT operand's promoted type; the right operand only has to be integral
    // and contributes nothing to the type.
    //
    // Running it through commonNumericType asked instead for a type BOTH sides convert to. For
    // `1u << 28` -- uint on the left, an int literal on the right -- there is no integral type
    // both reach, so the search fell through to the preference table and landed on the first
    // entry both do reach: `builtin:double`. The result was that the perfectly ordinary line
    //
    //     uint v = 1u << 28;
    //
    // failed with "cannot initialize local 'v' of type 'builtin:uint' from 'builtin:double'",
    // a diagnostic that gives no hint whatsoever where the double came from. The same wrong rule
    // is why a shift over a type narrower than int failed as well: `(int)(someUshort >> 8)` has
    // to be int, the promoted left type, not something common to a ushort and an int literal.
    if (isShift(operation))
    {
        if (!ProjectTypeConversion::isIntegral(left.typeKey) ||
            !ProjectTypeConversion::isIntegral(right.typeKey) ||
            left.typeKey == "builtin:bool" || right.typeKey == "builtin:bool")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "shift operands must be integral in Stage 34");
            return result;
        }
        result.typeKey = shiftResultType(left.typeKey);
        result.valid = true;
        return result;
    }

    // CC1 fase 4D -- aritmetica de punteros. VA ANTES de la rama aritmetica, y el orden importa:
    // `commonNumericType` no conoce los punteros, asi que si esto fuera despues, `p + 1` ya habria
    // muerto en "binary operator requires compatible numeric operands".
    //
    // Lo que se acepta, y nada mas: `p + i`, `i + p`, `p - i` y `p - q` con q del mismo tipo. No se
    // acepta `p + q` (sumar dos direcciones no significa nada), ni multiplicar o dividir punteros,
    // ni aritmetica sobre `void*` -- un paso de tamano cero no es un paso.
    //
    // POR QUE ESTO IMPORTA MAS DE LO QUE PARECE: `p[i]` ya funcionaba (RawElement) y `*(p + i)` no
    // compilaba. Son la misma direccion escrita de dos formas, y el nivel crudo de este lenguaje
    // existe justamente para expresar formas de C, donde la segunda es tan comun como la primera.
    // La fixture c12 comprueba que las dos dan el MISMO valor, que es la unica manera de fijar que
    // siguen siendo la misma cosa.
    {
        const bool leftIsPointer = left.valid && left.typeKey.ends_with("*");
        const bool rightIsPointer = right.valid && right.typeKey.ends_with("*");
        const bool plus = operation == lexer::TokenKind::Plus;
        const bool minus = operation == lexer::TokenKind::Minus;

        if ((leftIsPointer || rightIsPointer) && (plus || minus))
        {
            const bool bothPointers = leftIsPointer && rightIsPointer;
            const std::string pointerTypeKey = leftIsPointer ? left.typeKey : right.typeKey;
            const std::string element = pointerTypeKey.substr(0, pointerTypeKey.size() - 1);

            if (element == "builtin:void")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "pointer arithmetic requires a pointer to a sized type; 'void*' has no element size");
                return result;
            }

            if (bothPointers)
            {
                // Diferencia. Solo entre punteros del MISMO tipo: restar direcciones de tipos
                // distintos no tiene un paso con el que dividir.
                if (!minus || left.typeKey != right.typeKey)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        minus ? "pointer difference requires both operands to have the same pointer type"
                              : "two pointers cannot be added; only pointer +/- integer and pointer - pointer are defined");
                    return result;
                }
                // Con signo: q - p puede ser negativo. `nint` es del ancho del puntero, que es
                // exactamente el rango que la diferencia puede alcanzar.
                result.typeKey = "builtin:nint";
            }
            else
            {
                const std::string& offsetTypeKey = leftIsPointer ? right.typeKey : left.typeKey;
                if (!ProjectTypeConversion::isIntegral(offsetTypeKey) || offsetTypeKey == "builtin:bool")
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "pointer arithmetic requires an integral offset");
                    return result;
                }
                // `i - p` no existe: restarle una direccion a un entero no significa nada.
                if (minus && !leftIsPointer)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "an integer minus a pointer is not defined; write pointer - integer");
                    return result;
                }
                result.typeKey = pointerTypeKey;
            }

            MethodCoreBinding binding;
            binding.kind = MethodCoreOperationKind::PointerArithmetic;
            binding.syntax = expression;
            binding.methodMemberOrdinal = methodMemberOrdinal;
            binding.scopeOrdinal = scopeOrdinal;
            binding.resultTypeKey = result.typeKey;
            binding.elementTypeKey = element;
            coreBindings_.push_back(std::move(binding));

            result.valid = true;
            return result;
        }
    }

    if (isArithmetic(operation))
    {
        const std::optional<std::string> common = ProjectTypeConversion::commonNumericType(
            left.typeKey, right.typeKey, left.integerConstant, right.integerConstant);
        if (!common.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "binary operator requires compatible numeric operands in Stage 34");
            return result;
        }
        result.typeKey = *common;
        result.valid = true;

        // DIVISION POR CERO CON UNA CONSTANTE: se rechaza AQUI, en compilacion.
        //
        // POR QUE, y es la mitad que faltaba del arreglo de la division (doc 113). El contrato
        // (doc 110 §2.5) dice que `x / 0` es una TRAMPA, y desde ese arreglo lo es de verdad: el
        // bajador emite la guarda en el IR en vez de dejarselo al `idiv`. Pero cuando el divisor
        // es una constante CERO, el compilador ya sabe --en compilacion, sin ejecutar nada-- que
        // ese programa muere siempre. Lo que emitia era `br i1 false` hacia el bloque de trampa:
        // un fallo demostrado, compilado en silencio.
        //
        // Que se sepa y no se diga es la parte mala. Un diagnostico aqui convierte "tu programa
        // aborta cuando llegue a esa linea" en "tu programa no compila, y esta es la linea".
        //
        // SOLO ENTEROS. En coma flotante `x / 0.0` es INFINITO y `0.0 / 0.0` es NaN: los dos son
        // valores legitimos de IEEE 754 (doc 110 §2.6), no fallos, y rechazarlos seria romper el
        // contrato de arriba.
        //
        // Y SOLO CUANDO EL CERO ES CONSTANTE. Un divisor que sale de una variable no se toca: ahi
        // el compilador no sabe nada y la guarda en tiempo de ejecucion es la respuesta correcta.
        if ((operation == TokenKind::Slash || operation == TokenKind::Percent) &&
            ProjectIntegerEvaluator::isIntegerType(result.typeKey) &&
            divisorIsConstantZero(expression->second(), right, scopeOrdinal))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                operation == TokenKind::Slash
                    ? "division by the constant zero"
                    : "remainder by the constant zero");
            result.valid = false;
            return result;
        }
        return result;
    }

    return result;
}

// ID 82. La clase de expresiones que se puede bajar DOS VECES sin que se note. El porque esta en
// la declaracion (ProjectMethodBodyModel.h); aqui esta el que.
//
// La lista es corta a proposito y cada entrada se justifica sola:
//
//   * nullptr            -- receptor implicito (`Prop += 1` dentro de la clase). Es `this`.
//   * this / base        -- un puntero del marco. Ninguna llamada puede reasignarlo.
//   * literales          -- no se evaluan; son constantes en el modulo.
//   * identificador      -- SOLO si resuelve a un LOCAL o a un PARAMETRO. Si no resuelve en el
//                          ambito es un campo implicito (`this.x` sin escribir `this`), y los
//                          campos quedan fuera por la razon escrita en la declaracion.
//   * unario             -- sobre un operando aceptado, y NUNCA `++`/`--`: esos escriben.
//   * binario            -- sobre dos operandos aceptados. `&&` y `||` incluidos: son puros si
//                          sus operandos lo son, y su cortocircuito no cambia nada porque no hay
//                          nada que cortocircuitar que tenga efecto.
//
// Todo lo demas se rechaza: llamadas, `new`, `move`, indexadores anidados, casts, ternarios,
// accesos a campo. No porque sean necesariamente impuros --un cast no lo es-- sino porque la
// regla que se puede DEMOSTRAR es esta, y ampliarla pide una demostracion nueva por cada forma.
// El diagnostico dice literalmente que hay que subir el subnodo a un local, que es la
// transformacion que el usuario haria a mano y que vuelve el caso aceptable.
bool ProjectMethodBodyModel::isAccessorDestinationReevaluable(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal,
    const ast::AstNode*& offender) const
{
    if (expression == nullptr)
    {
        return true;
    }
    switch (expression->kind())
    {
        case ast::AstNodeKind::ThisExpression:
        case ast::AstNodeKind::BaseExpression:
        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::FloatingLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::BooleanLiteralExpression:
        case ast::AstNodeKind::NullLiteralExpression:
        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::OffsetofExpression:
        case ast::AstNodeKind::DefaultValueExpression:
            return true;

        case ast::AstNodeKind::IdentifierExpression:
        {
            const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, expression->text());
            if (name.has_value() &&
                (name->kind == ScopeNameKind::Local || name->kind == ScopeNameKind::Parameter))
            {
                return true;
            }
            offender = expression;
            return false;
        }

        case ast::AstNodeKind::UnaryExpression:
        {
            if (expression->tokenKind() == TokenKind::PlusPlus ||
                expression->tokenKind() == TokenKind::MinusMinus)
            {
                offender = expression;
                return false;
            }
            return isAccessorDestinationReevaluable(expression->first(), scopeOrdinal, offender);
        }

        case ast::AstNodeKind::BinaryExpression:
            return isAccessorDestinationReevaluable(expression->first(), scopeOrdinal, offender) &&
                isAccessorDestinationReevaluable(expression->second(), scopeOrdinal, offender);

        default:
            offender = expression;
            return false;
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeAssignment(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // T01-F03 `place ??= fallback` is not a numeric compound assignment. Analyze it here before
    // the generic read/modify/write machinery asks compoundAssignmentBaseOperator for a numeric
    // operator. This deliberately accepts only real addressable storage: a property/indexer would
    // require one captured receiver shared by getter and setter, which this HIR cannot represent
    // safely yet, so it receives a source diagnostic instead of being evaluated twice.
    if (expression->tokenKind() == TokenKind::QuestionQuestionEqual)
    {
        ExpressionResult target = analyzeExpression(
            expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
            false, false, false, false);
        const auto nullCarrier = [&](std::string_view typeKey)
        {
            if (ProjectTypeConversion::isPointer(typeKey) ||
                ProjectTypeConversion::isNativeArray(typeKey) ||
                typeKey == "builtin:string" || isAnyDelegateTypeKey(typeKey))
            {
                return true;
            }
            const std::optional<std::size_t> ordinal = referenceTypeOrdinalForTypeKey(typeKey);
            return ordinal.has_value() && inheritanceModel_ != nullptr &&
                (inheritanceModel_->isClass(*ordinal) ||
                 (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)));
        };
        bool valid = target.valid;
        if (valid && (!target.assignable || !target.addressable))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                expression->first() != nullptr ? expression->first()->range() : expression->range(),
                "left operand of '?' '?=' must be addressable storage; cache a property/indexer "
                "receiver explicitly before using null-coalescing assignment");
            valid = false;
        }
        if (valid && !nullCarrier(target.typeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->first() != nullptr ? expression->first()->range() : expression->range(),
                "left operand of '?' '?=' must have a nullable pointer/reference carrier");
            valid = false;
        }

        const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
        ExpressionResult fallback;
        const OwnershipFlowSnapshot fallbackState = analyzeBranchState(
            branchEntry,
            [&]
            {
                fallback = analyzeExpression(
                    expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            });
        commitBranchJoin(branchEntry, fallbackState);
        if (!fallback.valid) valid = false;
        if (valid)
        {
            const ProjectImplicitConversion conversion = classifyImplicit(
                fallback.typeKey, target.typeKey, fallback.integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->second() != nullptr ? expression->second()->range() : expression->range(),
                    "right operand of '?' '?=' with type '" + fallback.typeKey +
                        "' is not implicitly convertible to '" + target.typeKey + "'");
                valid = false;
            }
        }
        target.valid = valid;
        target.assignable = false;
        return target;
    }

    // NativeSharp 3.0 T02-F03. Evaluate the complete tuple source first, then validate targets
    // from left to right. HIR materializes that source once before taking any destination
    // address, which makes `(a, b) = (b, a)` alias-safe and preserves source side effects.
    if (expression->tokenKind() == TokenKind::Equal && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::TupleExpression)
    {
        const ExpressionResult source = analyzeExpression(
            expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        const std::optional<std::vector<std::string>> sourceElements =
            ProjectTypeConversion::tupleElementTypeKeys(source.typeKey);
        bool valid = source.valid;
        if (!sourceElements.has_value())
        {
            if (source.valid)
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->second() != nullptr ? expression->second()->range() : expression->range(),
                    "tuple deconstruction requires a tuple value on the right-hand side");
            valid = false;
        }
        else if (sourceElements->size() != expression->first()->children().size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->first()->range(),
                "tuple deconstruction target has " +
                    std::to_string(expression->first()->children().size()) +
                    " element(s), but the source has " + std::to_string(sourceElements->size()));
            valid = false;
        }
        if (sourceElements.has_value() && !elementTypeHasTrivialLifetime(source.typeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "tuple deconstruction of owning or move-only elements requires explicit move "
                "bindings and per-element drop flags");
            valid = false;
        }
        for (std::size_t index = 0; index < expression->first()->children().size(); ++index)
        {
            const ast::AstNode* target = expression->first()->children()[index];
            if (target != nullptr && target->kind() == ast::AstNodeKind::IdentifierExpression &&
                target->text() == "_") continue;
            const ast::AstNode* previousWriteTarget = definiteAssignmentWriteTarget_;
            markAssignmentTarget(target, scopeOrdinal);
            const ExpressionResult targetResult = analyzeExpression(
                target, methodMemberOrdinal, scopeOrdinal, diagnostics,
                false, true, false, true);
            definiteAssignmentWriteTarget_ = previousWriteTarget;
            if (!targetResult.valid || !targetResult.assignable || !targetResult.addressable)
            {
                if (targetResult.valid)
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidAssignmentTarget,
                        diagnostics::DiagnosticSeverity::Error,
                        target != nullptr ? target->range() : expression->first()->range(),
                        "tuple deconstruction target must be addressable storage or '_'");
                valid = false;
                continue;
            }
            if (sourceElements.has_value() && index < sourceElements->size())
            {
                const ProjectImplicitConversion conversion = classifyImplicit(
                    (*sourceElements)[index], targetResult.typeKey, std::nullopt);
                if (!conversion.exists())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidConversion,
                        diagnostics::DiagnosticSeverity::Error,
                        target->range(),
                        "tuple element " + std::to_string(index) + " of type '" +
                            (*sourceElements)[index] + "' cannot convert to target type '" +
                            targetResult.typeKey + "'");
                    valid = false;
                }
            }
        }
        ExpressionResult result = source;
        result.valid = valid;
        result.assignable = false;
        result.addressable = false;
        return result;
    }

    std::optional<std::size_t> raiiTargetLocal;
    ExpressionResult left;
    // C4 "language references": record that an `out` parameter was written to. The obligation an
    // `out` parameter carries -- the callee MUST assign it -- is checked once the whole body has
    // been analyzed (see verifyOutParameterAssignments), and this is where the evidence is
    // collected.
    {
        // Writing THROUGH an out parameter counts as writing it, whichever way the write is
        // spelled: `rect = other;`, `rect.left = 0;`, `values[0] = 1;`, `*p = v;`. The
        // GetMessage/GetClientRect shape -- the callee filling a record the caller owns, field by
        // field -- is the whole reason `out` exists for a struct, so the check walks the
        // assignment target down to the name it is rooted at.
        const ast::AstNode* rootTarget = expression->first();
        while (rootTarget != nullptr && rootTarget->kind() != ast::AstNodeKind::IdentifierExpression)
        {
            switch (rootTarget->kind())
            {
                case ast::AstNodeKind::MemberExpression:
                case ast::AstNodeKind::IndexExpression:
                case ast::AstNodeKind::UnaryExpression:
                    rootTarget = rootTarget->first();
                    continue;
                default:
                    rootTarget = nullptr;
                    continue;
            }
        }
        if (rootTarget != nullptr)
        {
            const std::optional<ScopeName> outTarget =
                lookupScopeName(scopeOrdinal, rootTarget->text());
            if (outTarget.has_value() && outTarget->kind == ScopeNameKind::Parameter)
            {
                assignedOutParameters_.insert({methodMemberOrdinal, outTarget->semanticOrdinal});
            }
        }
    }
    if (expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> target = lookupScopeName(scopeOrdinal, expression->first()->text());
        if (target.has_value() && target->kind == ScopeNameKind::Local &&
            target->semanticOrdinal < locals_.size() && locals_[target->semanticOrdinal].isRaii)
        {
            const MethodLocalSymbol& local = locals_[target->semanticOrdinal];
            raiiTargetLocal = target->semanticOrdinal;
            left.typeKey = local.typeKey;
            left.valid = true;
            left.assignable = true;
            nameBindings_.push_back(MethodNameBinding{
                MethodNameBindingKind::Local,
                expression->first(),
                methodMemberOrdinal,
                scopeOrdinal,
                local.name,
                local.typeKey,
                target->semanticOrdinal,
                std::nullopt,
                {},
                nullptr,
                "",
                std::nullopt,
                false});
        }
    }
    // C7 "definite assignment": a plain `=` assigns its target; a compound one (`x += 1`) reads it
    // first and therefore requires it to be assigned already, so only `=` marks and suppresses.
    // markAssignmentTarget must run BEFORE the left-hand side is analyzed, because analyzing it is
    // what would otherwise report the target as a read of an unassigned local.
    const ast::AstNode* const previousWriteTarget = definiteAssignmentWriteTarget_;
    if (expression->tokenKind() == TokenKind::Equal)
    {
        markAssignmentTarget(expression->first(), scopeOrdinal);
    }
    if (!left.valid)
    {
        left = analyzeExpression(
            expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
            false, true, false, expression->tokenKind() == TokenKind::Equal);
    }
    definiteAssignmentWriteTarget_ = previousWriteTarget;
    if (!left.valid)
    {
        return {};
    }

    // Stage 52: a plain (non-RAII-local) assignment target may still be a legitimate move(...)
    // destination when it is itself a direct native array element -- e.g. EnsureCapacity's
    // "grown[i] = move(items[i]);" while growing a generic owned T[] backing store to relocate
    // an element from the old array into the new one. This is the exact "single owning slot"
    // invariant move(...) exists to preserve; it just involves two array slots rather than a
    // named RAII local/parameter on either side. The RHS move(...)'s own array-element-source
    // check (see the MoveExpression case in analyzeExpression) independently verifies the
    // source; this only widens which assignment TARGETS are allowed to receive it.
    bool arrayElementMoveTarget = false;
    bool ownedArrayElementOwnsStorage = false;
    if (!raiiTargetLocal.has_value() && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IndexExpression &&
        left.valid && left.assignable)
    {
        ExpressionResult receiver;
        if (expression->first()->first() != nullptr)
        {
            receiver = analyzeExpression(
                expression->first()->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        const std::string& receiverTypeKey = receiver.typeKey;
        // SHR-005: y el ARREGLO FIJO, por la misma razon y con la misma forma. La fuente
        // `move(buf[i])` se admite unas mil lineas mas arriba con esta misma condicion doble; si
        // solo se admitiera de un lado, `dst[i] = move(src[j])` compilaria en una direccion y no
        // en la otra. La admision de destino es simetrica a la de fuente a proposito.
        arrayElementMoveTarget = ProjectTypeConversion::isNativeArray(receiverTypeKey) ||
            ProjectMemberModel::fixedArrayElementTypeKey(receiverTypeKey).has_value() ||
            ProjectTypeConversion::isPointer(receiverTypeKey);

        // A native array is raw storage, but an array reached through an `owned`/`raii` binding is
        // also the container's owning element store. Moving into one of its reference-like slots
        // therefore has the same replacement contract as writing an `owned` field: the RHS must
        // transfer ownership and the displaced value must be released before the new value is
        // installed. Value elements remain ordinary value stores.
        bool receiverOwnsStorage = receiver.ownership == sema::OwnershipKind::Owned;
        if (!receiverOwnsStorage && receiver.assignmentMemberOrdinal.has_value() &&
            memberModel_ != nullptr &&
            *receiver.assignmentMemberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& receiverMember =
                memberModel_->members()[*receiver.assignmentMemberOrdinal];
            receiverOwnsStorage = receiverMember.kind == ClassMemberKind::Field &&
                !receiverMember.isStatic &&
                receiverMember.declaredOwnership == sema::OwnershipKind::Owned;
        }
        const bool elementOwnsSeparateStorage =
            isReferenceTypeKey(left.typeKey) || left.typeKey == "builtin:string";
        ownedArrayElementOwnsStorage =
            arrayElementMoveTarget && receiverOwnsStorage && elementOwnsSeparateStorage;
    }

    // Bloque 1 item 1 (owning field storage): a field declared `owned` is a legitimate move(...)
    // destination. Before this, `owned` FIELDS could be declared and parsed -- the Stage 50 syntax
    // tests assert exactly that -- but nothing could ever be put into one, because no assignment
    // target other than a RAII local or an array element admitted a move. The feature was
    // declarable and unusable.
    //
    // Admitting the target is only half of it: a class that owns a field has to RELEASE it, or the
    // transfer would just move the leak. The generated destruction entry now walks the type's owned
    // fields (see ProjectOopMirDispatchTable::ownedFields), so ownership handed to a field is
    // discharged when the object dies, exactly once.
    bool ownedFieldMoveTarget = false;
    // True only when the owned field also OWNS a separate allocation (reference type or native
    // string) -- the storage classes whose stores must be ownership transfers. A generic `owned T`
    // field closed over a value type keeps plain assignment: `owned` is the no-op it has always
    // been there, same rule as owned locals and owned parameters.
    //
    // SHR-005, mitad "agregado por valor" -- MEDIDA, y no es un hueco. Esta condicion es la misma
    // que `requiresOwnedDestruction` en ProjectOopLlvmLowerer.cpp:
    //
    //     requiresOwnedDestruction(t)  ==  (t == "builtin:string" || isClassReference(t))
    //     ownedFieldOwnsStorage        ==  (isReferenceTypeKey(t)  || t == "builtin:string")
    //
    // Las dos mitades de la regla --exigir una expresion de transferencia al escribir, y liberar
    // el valor desplazado al bajar-- disparan sobre EXACTAMENTE el mismo conjunto de tipos. Para
    // un struct por valor no dispara ninguna de las dos, que es lo coherente: no hay reserva
    // separada que transferir ni que liberar. Se comprueba contando destrucciones en ejecucion en
    // Stage50_11OwnershipExecutionTests, que es como este proyecto verifica ownership. Lo que
    // quedaba de SHR-005 era el ARREGLO FIJO, y esta arriba, en arrayElementMoveTarget.
    bool ownedFieldOwnsStorage = false;
    std::string ownedFieldName;
    if (!raiiTargetLocal.has_value() && !arrayElementMoveTarget &&
        left.valid && left.assignable && left.assignmentMemberOrdinal.has_value() &&
        memberModel_ != nullptr &&
        *left.assignmentMemberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& targetMember =
            memberModel_->members()[*left.assignmentMemberOrdinal];
        ownedFieldMoveTarget = targetMember.kind == ClassMemberKind::Field &&
            !targetMember.isStatic &&
            targetMember.declaredOwnership == sema::OwnershipKind::Owned;
        ownedFieldOwnsStorage = ownedFieldMoveTarget &&
            (isReferenceTypeKey(targetMember.declaredTypeKey) ||
             targetMember.declaredTypeKey == "builtin:string");
        if (ownedFieldMoveTarget) ownedFieldName = targetMember.name;
    }

    // Bloque 1 item 9 (ownership-aware properties/indexers): find the getter binding NOW, before
    // the right-hand side is analyzed, so `h.Slot = move(a)` and `h[0] = move(a)` can be admitted
    // as ownership transfers.
    //
    // An assignment to a property or indexer is analyzed by first binding the LHS as a GETTER
    // call and then, further down, re-resolving it as a synthetic `set_X` call with the RHS
    // appended as the last argument. That lookup used to live entirely below the RHS analysis,
    // which was exactly the problem: by the time anything knew a setter was involved, the RHS
    // had already been rejected -- `move(...)` is only accepted when the caller passes
    // allowMove, and no property/indexer target ever set it.
    //
    // The binding is recorded by the `left` analysis above, so it is already in calls_ here. It
    // is remembered as an INDEX rather than a pointer on purpose: analyzing the RHS can append
    // to calls_ (any call in the RHS expression does), and a vector reallocation would dangle a
    // pointer taken now. Entries are only ever appended, so the index stays valid.
    std::optional<std::size_t> getterBindingIndex;
    for (std::size_t callIndex = calls_.size(); callIndex-- > 0;)
    {
        const MethodCallBinding& candidate = calls_[callIndex];
        if (candidate.syntax != expression->first() ||
            candidate.resolution != MethodCallResolutionKind::Resolved ||
            !candidate.selectedMemberOrdinal.has_value())
        {
            continue;
        }
        const std::size_t selected = *candidate.selectedMemberOrdinal;
        if (memberModel_ != nullptr && selected < memberModel_->members().size())
        {
            const ClassMemberSymbol& member = memberModel_->members()[selected];
            if ((member.isPropertyAccessor || member.isIndexerAccessor) && member.isGetter)
            {
                getterBindingIndex = callIndex;
                break;
            }
        }
    }

    // The getter carries the property's ownership (see ClassMemberSymbol::ownsAccessorStorage):
    // when the property is declared `owned`, its setter's implicit `value` parameter is an owned
    // parameter, so storing into it through the setter is a real ownership transfer and not a
    // silent aliasing copy. This is the fourth and last legitimate move(...) destination, next to
    // a RAII local, a native array element and an `owned` field.
    bool ownedSetterMoveTarget = false;
    if (!raiiTargetLocal.has_value() && !arrayElementMoveTarget && !ownedFieldMoveTarget &&
        getterBindingIndex.has_value() && memberModel_ != nullptr)
    {
        const std::size_t getterOrdinal = *calls_[*getterBindingIndex].selectedMemberOrdinal;
        ownedSetterMoveTarget = memberModel_->members()[getterOrdinal].ownsAccessorStorage;
    }

    if (left.eventMemberOrdinal.has_value())
    {
        if (left.readonlyReceiver)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "readonly receiver cannot add or remove an event handler");
            return {};
        }
        if (expression->tokenKind() != TokenKind::PlusEqual &&
            expression->tokenKind() != TokenKind::MinusEqual)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "Stage 42 events support only += and -= subscription operations");
            return {};
        }
        if (memberModel_ == nullptr || *left.eventMemberOrdinal >= memberModel_->members().size())
        {
            return {};
        }
        const ClassMemberSymbol& event = memberModel_->members()[*left.eventMemberOrdinal];
        const std::optional<std::size_t> backing = eventBackingMember(*left.eventMemberOrdinal);
        if (!backing.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "Stage 42 event is missing its compiler-generated backing storage");
            return {};
        }

        ExpressionResult right;
        if (expression->second()->kind() == ast::AstNodeKind::LambdaExpression)
            right = bindLambdaToDelegate(
                expression->second(), event.declaredTypeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
        else
            right = analyzeExpression(
                expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics, true);
        if (right.methodGroup)
        {
            right = bindMethodGroupToDelegate(
                expression->second(), right, event.declaredTypeKey,
                methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        if (!right.valid)
        {
            return {};
        }
        if (right.typeKey == "builtin:null")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->second()->range(),
                "Stage 42 event subscription/removal requires a non-null delegate value");
            return {};
        }
        const ProjectImplicitConversion conversion = classifyImplicit(
            right.typeKey, event.declaredTypeKey, right.integerConstant);
        if (!conversion.exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->second()->range(),
                "event handler type '" + right.typeKey + "' is not compatible with event delegate type '" +
                    event.declaredTypeKey + "'");
            return {};
        }

        MethodEventBinding binding;
        binding.kind = expression->tokenKind() == TokenKind::PlusEqual
            ? MethodEventOperationKind::Add : MethodEventOperationKind::Remove;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.eventMemberOrdinal = *left.eventMemberOrdinal;
        binding.backingMemberOrdinal = *backing;
        binding.delegateTypeKey = event.declaredTypeKey;
        binding.delegateTypeOrdinal = delegateTypeOrdinalForTypeKey(event.declaredTypeKey).value_or(0);
        binding.receiverSyntax = left.receiverSyntax;
        binding.receiverTypeKey = left.receiverTypeKey;
        binding.receiverTypeOrdinal = left.receiverTypeOrdinal;
        binding.staticEvent = event.isStatic;
        binding.resultTypeKey = "builtin:void";
        binding.argumentConversions.push_back(conversion);
        eventBindings_.push_back(std::move(binding));

        ExpressionResult result;
        result.typeKey = "builtin:void";
        result.valid = true;
        return result;
    }

    if (raiiTargetLocal.has_value() &&
        locals_[*raiiTargetLocal].isPinned)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "cannot replace pinned owner '" + locals_[*raiiTargetLocal].name +
                "'; it must remain at a stable address until lexical destruction");
        return {};
    }

    if (raiiTargetLocal.has_value() &&
        hasActiveBorrowOfOwnerLocal(*raiiTargetLocal, scopeOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "cannot overwrite owner '" + locals_[*raiiTargetLocal].name +
                "' while a borrowed local is alive");
        return {};
    }

    const bool delegateTarget = isDelegateTypeKey(left.typeKey);
    ExpressionResult right;
    if (delegateTarget && expression->second()->kind() == ast::AstNodeKind::LambdaExpression)
        right = bindLambdaToDelegate(
            expression->second(), left.typeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
    else
        right = analyzeExpression(
            expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics,
            delegateTarget, false,
            raiiTargetLocal.has_value() || arrayElementMoveTarget || ownedFieldMoveTarget ||
                ownedSetterMoveTarget);
    if (right.methodGroup && delegateTarget)
    {
        right = bindMethodGroupToDelegate(
            expression->second(), right, left.typeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    if (!right.valid)
    {
        return {};
    }

    std::optional<std::size_t> assignmentTargetLocal;
    if (expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> target = lookupScopeName(
            scopeOrdinal, expression->first()->text());
        if (target.has_value() && target->kind == ScopeNameKind::Local &&
            target->semanticOrdinal < locals_.size())
        {
            assignmentTargetLocal = target->semanticOrdinal;
        }
    }
    if (assignmentTargetLocal.has_value() &&
        locals_[*assignmentTargetLocal].isVariableLengthArray)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "a variable-length array is storage, not a reassignable pointer");
        return {};
    }
    const std::optional<std::size_t> assignedVlaRoot =
        variableArrayRootLocalOfExpression(expression->second(), scopeOrdinal);
    if (assignedVlaRoot.has_value())
    {
        if (!assignmentTargetLocal.has_value())
        {
            const std::string name = *assignedVlaRoot < locals_.size()
                ? locals_[*assignedVlaRoot].name : std::string("<variable-length-array>");
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->second()->range(),
                "pointer into variable-length array '" + name +
                    "' cannot be stored outside a local alias because the stack frame owns it");
            return {};
        }
        locals_[*assignmentTargetLocal].variableLengthArrayRootLocalOrdinal = assignedVlaRoot;
    }
    else if (assignmentTargetLocal.has_value() &&
        locals_[*assignmentTargetLocal].typeKey.ends_with("*"))
    {
        locals_[*assignmentTargetLocal].variableLengthArrayRootLocalOrdinal.reset();
    }

    // Stage 52: same value-type bypass as the local-declaration case above -- a borrowed value
    // assigned into a plain local/field/parameter whose CLOSED type never needed ownership
    // tracking (a generic T monomorphized to int, bool, a struct, ...) carries no real lifetime
    // to protect.
    // SHR-013: `v = otra;` cambia de que presta `v`, asi que el enraizamiento grabado en la
    // declaracion deja de valer. Se ACUMULA en vez de reemplazarse: tras dos asignaciones en
    // ramas distintas la vista puede venir de cualquiera de las dos, y la respuesta segura es
    // quedarse con la primera que la ate a un local.
    // El destino puede ser el local a secas (`v = ...`) o un ELEMENTO de un arreglo local de
    // vistas (`tokens[i] = ...`). El segundo caso importa tanto como el primero: un
    // `StringView[]` local lleno de vistas de otro local y devuelto despues seria la misma fuga
    // con un nivel de indireccion. El enraizamiento se acumula EN EL ARREGLO, que es el valor
    // que puede escapar.
    const ast::AstNode* viewAssignmentRoot = expression->first();
    while (viewAssignmentRoot != nullptr &&
        viewAssignmentRoot->kind() == ast::AstNodeKind::IndexExpression)
    {
        viewAssignmentRoot = viewAssignmentRoot->first();
    }
    if (viewAssignmentRoot != nullptr &&
        viewAssignmentRoot->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> viewTarget =
            lookupScopeName(scopeOrdinal, viewAssignmentRoot->text());
        if (viewTarget.has_value() && viewTarget->kind == ScopeNameKind::Local &&
            viewTarget->semanticOrdinal < locals_.size() &&
            typeKeyIsViewLike(locals_[viewTarget->semanticOrdinal].typeKey) &&
            locals_[viewTarget->semanticOrdinal].viewRootLocalName.empty())
        {
            const std::optional<std::string> root =
                viewRootLocalOfExpression(expression->second(), scopeOrdinal);
            if (root.has_value())
            {
                locals_[viewTarget->semanticOrdinal].viewRootLocalName = *root;
            }
        }
    }

    const bool assignmentTargetOwnable = isReferenceTypeKey(left.typeKey) || left.typeKey == "builtin:string";
    if (right.ownership == sema::OwnershipKind::Borrowed && assignmentTargetOwnable)
    {
        if (left.assignmentMemberOrdinal.has_value() && memberModel_ != nullptr &&
            *left.assignmentMemberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& field = memberModel_->members()[*left.assignmentMemberOrdinal];
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->second()->range(),
                "borrowed value cannot be stored in field '" + field.name +
                    "' because the field may outlive its source owner");
            return {};
        }
        if (expression->first() != nullptr &&
            expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::optional<ScopeName> target = lookupScopeName(
                scopeOrdinal, expression->first()->text());
            if (target.has_value())
            {
                if (target->kind == ScopeNameKind::Local && target->semanticOrdinal < locals_.size())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->second()->range(),
                        "borrowed value cannot be stored in unqualified local '" +
                            locals_[target->semanticOrdinal].name +
                            "'; declare a borrowed local at initialization so provenance remains checked");
                    return {};
                }
                if (target->kind == ScopeNameKind::Parameter)
                {
                    const ClassMemberSymbol* owner = methodSymbol(methodMemberOrdinal);
                    const std::string targetName = owner != nullptr &&
                        target->semanticOrdinal < owner->parameters.size()
                        ? owner->parameters[target->semanticOrdinal].name
                        : expression->first()->text();
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->second()->range(),
                        "borrowed value cannot be assigned to parameter '" + targetName +
                            "' because replacement lifetime provenance is not tracked in Stage 50");
                    return {};
                }
            }
        }
    }

    if (!raiiTargetLocal.has_value() && expression->second() != nullptr &&
        expression->second()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> sourceName = lookupScopeName(scopeOrdinal, expression->second()->text());
        if (sourceName.has_value() && sourceName->kind == ScopeNameKind::Local &&
            sourceName->semanticOrdinal < locals_.size() && locals_[sourceName->semanticOrdinal].isRaii)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->second()->range(),
                "RAII owner '" + locals_[sourceName->semanticOrdinal].name +
                    "' cannot be copied by assignment; Stage 49 requires ownership transfer to another RAII local with move(...)");
            return {};
        }

        // Bloque 1: the same rule with an owned PARAMETER as the source. This was the one hole
        // left in the copy-owner contract, and it was a real double free -- measured, not
        // theorised: `set { backing = value; }` on an `owned` property compiled and destroyed the
        // object twice (the field's release plus the parameter's own end-of-scope cleanup).
        //
        // Every other destination already caught it: owned parameter into a RAII local ("RAII
        // ownership cannot be copied or borrowed"), owned parameter into an owned argument
        // ("cannot copy owned value 'value' into owned parameter; use move(value)"), and an owner
        // LOCAL into a field (the check just above). Only "owned parameter into a field" was
        // missing, which is exactly the store the owned-field work of this same block had just
        // made reachable -- the feature opened a destination and the rule that guards every other
        // destination did not follow it there.
        //
        // Same value-type narrowing as everywhere else: `owned` over a type that owns no separate
        // allocation is the no-op it has always been, so copying one is not a double free and must
        // not be reported as one.
        if (sourceName.has_value() && sourceName->kind == ScopeNameKind::Parameter)
        {
            const ClassMemberSymbol* enclosing = methodSymbol(methodMemberOrdinal);
            if (enclosing != nullptr && sourceName->semanticOrdinal < enclosing->parameters.size())
            {
                const MethodParameterSymbol& source = enclosing->parameters[sourceName->semanticOrdinal];
                const bool sourceOwnsStorage =
                    isReferenceTypeKey(source.typeKey) || source.typeKey == "builtin:string";
                if (source.ownership == sema::OwnershipKind::Owned && sourceOwnsStorage)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->second()->range(),
                        "cannot copy owned value '" + source.name +
                            "' by assignment; use move(" + source.name + ") to transfer ownership");
                    return {};
                }
            }
        }
    }

    // Bloque 1 (audit finding "overwrite sin transferencia explicita"): every write into a field
    // that OWNS storage must be an ownership-transfer expression -- move(...), new T(...), null,
    // or an owned-returning call. This is the exact set the owned-return check accepts and the
    // owned-local initializer accepts; a field cannot distinguish initialization from assignment,
    // so it takes the full transfer set rather than the locals' stricter move-only assignment
    // rule, and `field = null` is the legitimate Clear operation (release what the slot holds,
    // leave it empty).
    //
    // What this rejects is the remaining silent hole: a PLAIN alias flowing into an owning slot
    // (`h.slot = plainLocal`, `h.slot = someCall()` returning an unqualified reference). Accepting
    // it converted a non-owner reference into effective ownership -- the container would later
    // release an object somebody else still references, or that somebody else separately frees.
    // The identifier-source cases (raii/owned local, owned parameter) are caught above with their
    // own more specific messages; this catches every other non-transfer shape.
    //
    // The release of the displaced value is the OTHER half of the same rule and is handled in the
    // HIR/lowering (see the Assignment case in ProjectOopHirModel): it fires for every one of
    // these forms, not just the move-spelled one -- that asymmetry was a measured leak (0
    // destructions for `= null`, 1 of 2 for `= new`).
    const bool ownsStorageDestination =
        ownedFieldOwnsStorage || ownedArrayElementOwnsStorage;
    if (ownsStorageDestination)
    {
        const bool freshObject = expression->second() != nullptr &&
            expression->second()->kind() == ast::AstNodeKind::ObjectCreationExpression;
        const bool nullValue = expression->second() != nullptr &&
            expression->second()->kind() == ast::AstNodeKind::NullLiteralExpression;
        const bool ownershipTransferred =
            right.ownership == sema::OwnershipKind::Owned && right.ownershipTransfer;
        if (!freshObject && !nullValue && !ownershipTransferred)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->second()->range(),
                ownedFieldOwnsStorage
                    ? "assignment to owned field '" + ownedFieldName +
                          "' requires an ownership-transfer expression such as move(owner), new T(...), "
                          "null, or an owned-returning call"
                    : "assignment to an element of an owned array requires an ownership-transfer "
                          "expression such as move(owner), new T(...), null, or an owned-returning call");
            return {};
        }
    }

    if (delegateTarget && expression->tokenKind() != TokenKind::Equal)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "Stage 42 delegates are single-cast values; use an event for +=/-= multicast subscription");
        return {};
    }

    if (raiiTargetLocal.has_value())
    {
        if (expression->tokenKind() != TokenKind::Equal ||
            expression->second() == nullptr ||
            expression->second()->kind() != ast::AstNodeKind::MoveExpression)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "RAII locals are move-only; assignment requires target = move(source)");
            return {};
        }
        const MethodMoveBinding* move = nullptr;
        for (auto iterator = moveBindings_.rbegin(); iterator != moveBindings_.rend(); ++iterator)
        {
            if (iterator->syntax == expression->second())
            {
                move = &*iterator;
                break;
            }
        }
        if (move != nullptr && move->sourceLocalOrdinal == *raiiTargetLocal)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "self move-assignment of a RAII local is not allowed");
            return {};
        }
    }

    // Resolved above, before the right-hand side was analyzed, so that an `owned` property or
    // indexer could admit move(...) on its RHS. Only now is it safe to take a pointer: nothing
    // appends to calls_ between here and the last use below.
    const MethodCallBinding* getterBinding = getterBindingIndex.has_value()
        ? &calls_[*getterBindingIndex] : nullptr;
    if (getterBinding != nullptr && memberModel_ != nullptr &&
        getterBinding->selectedMemberOrdinal.has_value() &&
        *getterBinding->selectedMemberOrdinal < memberModel_->members().size() &&
        memberModel_->members()[*getterBinding->selectedMemberOrdinal].isReferenceReturn)
    {
        // A ref indexer getter exposes its returned location directly. It is an ordinary
        // addressable assignment target and must not be rewritten into a nonexistent setter.
        getterBinding = nullptr;
    }

    if (right.ownership == sema::OwnershipKind::Borrowed && getterBinding != nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->second()->range(),
            "borrowed value cannot be stored through a property/indexer setter because retention is not proven");
        return {};
    }

    if (getterBinding != nullptr)
    {
        const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
        const ClassMemberSymbol& getter = memberModel_->members()[*getterBinding->selectedMemberOrdinal];
        const std::size_t accessingTypeOrdinal = caller != nullptr
            ? caller->declaringTypeOrdinal : getter.declaringTypeOrdinal;
        const std::size_t receiverTypeOrdinal = getterBinding->receiverTypeOrdinal.value_or(
            getter.declaringTypeOrdinal);
        const std::size_t protectedReceiverType = getterBinding->baseReceiver
            ? accessingTypeOrdinal : receiverTypeOrdinal;
        const std::string setterName = getter.isPropertyAccessor
            ? "set_" + getter.associatedName : "set_Item";
        std::vector<std::size_t> setters = stage38AccessorCandidates(
            receiverTypeOrdinal,
            setterName,
            getter.isPropertyAccessor,
            false,
            getter.isStatic,
            accessingTypeOrdinal,
            protectedReceiverType);
        if (setters.empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                diagnostics::DiagnosticSeverity::Error,
                expression->first()->range(),
                "Stage 38 property/indexer has no accessible setter");
            return {};
        }

        std::vector<ExpressionResult> setterArguments;
        if (getter.isIndexerAccessor && expression->first()->kind() == ast::AstNodeKind::IndexExpression)
        {
            const ast::AstNode* indexExpression = expression->first();
            const auto appendRecordedExpression = [&](const ast::AstNode* syntax)
            {
                ExpressionResult value;
                for (auto iterator = expressionTypes_.rbegin(); iterator != expressionTypes_.rend(); ++iterator)
                {
                    if (iterator->syntax == syntax)
                    {
                        value.typeKey = iterator->typeKey;
                        value.integerConstant = iterator->integerConstant;
                        value.valid = !isErrorType(value.typeKey);
                        break;
                    }
                }
                setterArguments.push_back(std::move(value));
            };
            if (!indexExpression->children().empty())
            {
                for (const ast::AstNode* index : indexExpression->children()) appendRecordedExpression(index);
            }
            else if (indexExpression->second() != nullptr)
            {
                appendRecordedExpression(indexExpression->second());
            }
        }

        // ID 82 -- la asignacion COMPUESTA sobre propiedad e indexador. Hasta aqui esto se
        // rechazaba con «compound assignment to a Stage 38 property/indexer is deferred».
        //
        // QUE SIGNIFICA. `p.X += e` es `set_X(p, get_X(p) + e)`. El getter ya esta ligado arriba
        // (getterBinding) porque el analisis del lado izquierdo lo produjo; el setter se resuelve
        // unas lineas mas abajo. Lo unico que falta es decir de que TIPO es el valor que se le
        // pasa al setter, y decirlo con la MISMA regla que gobierna `x += e` sobre un destino
        // corriente -- no con una segunda.
        //
        // LA REGLA, y es literalmente la de mas abajo en esta misma funcion:
        //
        //   1. `commonNumericType(destino, derecho)` tiene que existir: operandos compatibles.
        //   2. el operando derecho tiene que convertir IMPLICITAMENTE al tipo del destino. Esta
        //      es la divergencia deliberada respecto de C# que el ID 81 escribio: C# mete un cast
        //      implicito de vuelta y TRUNCA (`byte b; int n; b += n;` compila); aqui se pide el
        //      cast escrito. Una propiedad no puede ser la excepcion a esa regla.
        //   3. la operacion ocurre ENTERA en el tipo del destino, que es exactamente lo que
        //      `CompoundStore` hace para un destino con direccion (ver su bajada en
        //      ProjectOopHirModel). Por eso el argumento del setter se anota con el tipo del
        //      GETTER y no con el del operando derecho: asi la conversion que registra
        //      resolveSyntheticCall es la identidad y no hay una segunda tabla de promociones.
        //
        // LO QUE HAY QUE DEMOSTRAR APARTE. El receptor y los argumentos del indexador se bajan
        // dos veces --una por llamada-- porque el arbol HIR es de `unique_ptr` y un nodo no se
        // comparte entre dos padres. Sobre la clase de expresiones que acepta
        // isAccessorDestinationReevaluable esa segunda bajada da el mismo valor por construccion;
        // fuera de ella se rechaza con el rango de fuente del subnodo culpable. Ver la
        // declaracion de esa funcion para el argumento completo.
        const bool compound = expression->tokenKind() != TokenKind::Equal;
        ExpressionResult setterValue = right;
        if (compound)
        {
            if (lexer::compoundAssignmentBaseOperator(expression->tokenKind()) == TokenKind::Invalid)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidAssignmentTarget,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "unsupported compound assignment operator on a property/indexer");
                return {};
            }

            const ast::AstNode* offender = nullptr;
            bool reevaluable = isAccessorDestinationReevaluable(
                getterBinding->receiverSyntax, scopeOrdinal, offender);
            if (reevaluable && getter.isIndexerAccessor &&
                expression->first()->kind() == ast::AstNodeKind::IndexExpression)
            {
                const ast::AstNode* indexExpression = expression->first();
                if (!indexExpression->children().empty())
                {
                    for (const ast::AstNode* index : indexExpression->children())
                    {
                        reevaluable = reevaluable &&
                            isAccessorDestinationReevaluable(index, scopeOrdinal, offender);
                    }
                }
                else
                {
                    reevaluable = isAccessorDestinationReevaluable(
                        indexExpression->second(), scopeOrdinal, offender);
                }
            }
            if (!reevaluable)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidAssignmentTarget,
                    diagnostics::DiagnosticSeverity::Error,
                    offender != nullptr ? offender->range() : expression->first()->range(),
                    "compound assignment to a property/indexer evaluates its receiver and index "
                    "arguments once for the getter and once for the setter, so each has to be a "
                    "local, a parameter, `this` or a constant expression; assign this one to a "
                    "local first and use that local");
                return {};
            }

            const TokenKind baseOperator =
                lexer::compoundAssignmentBaseOperator(expression->tokenKind());
            const bool sameEnumBitwise = isEnumTypeKey(left.typeKey) &&
                left.typeKey == right.typeKey &&
                (baseOperator == TokenKind::Amp || baseOperator == TokenKind::Pipe ||
                 baseOperator == TokenKind::Caret);
            const bool shiftAssignment = isShift(baseOperator) &&
                ProjectTypeConversion::isIntegral(left.typeKey) &&
                ProjectTypeConversion::isIntegral(right.typeKey) &&
                left.typeKey != "builtin:bool" && right.typeKey != "builtin:bool";
            const std::optional<std::string> common = sameEnumBitwise || shiftAssignment
                ? std::optional<std::string>(left.typeKey)
                : ProjectTypeConversion::commonNumericType(
                      left.typeKey, right.typeKey, left.integerConstant, right.integerConstant);
            if (!common.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "compound assignment to a property/indexer requires compatible numeric operands");
                return {};
            }

            const ProjectImplicitConversion narrowing = classifyImplicit(
                right.typeKey, left.typeKey, right.integerConstant);
            if (!shiftAssignment && !narrowing.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->second()->range(),
                    "compound assignment cannot implicitly convert its right-hand side from '" +
                        right.typeKey + "' to '" + left.typeKey +
                        "'; write the read-modify-write with an explicit cast");
                return {};
            }

            // El valor que recibe el setter es del tipo del DESTINO, no del operando derecho.
            // Fresco a proposito: copiar `left` arrastraria assignmentMemberOrdinal y la
            // propiedad de ownership del getter a un valor que ya no es ese almacenamiento.
            ExpressionResult stored;
            stored.typeKey = left.typeKey;
            stored.valid = true;
            setterValue = std::move(stored);
        }
        setterArguments.push_back(setterValue);

        ExpressionResult setterResult = resolveSyntheticCall(
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            setterName,
            std::move(setters),
            setterArguments,
            getterBinding->receiverSyntax,
            getterBinding->receiverTypeKey,
            getterBinding->receiverTypeOrdinal,
            getterBinding->baseReceiver,
            getterBinding->interfaceCall,
            diagnostics);
        if (!setterResult.valid && setterResult.typeKey != "builtin:void") return {};

        ExpressionResult assignmentResult = left;
        assignmentResult.assignable = false;
        assignmentResult.valid = true;
        return assignmentResult;
    }

    if (!left.assignable)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            diagnostics::DiagnosticSeverity::Error,
            expression->first()->range(),
            "left side of assignment is not assignable");
        return {};
    }

    if (expression->tokenKind() == TokenKind::Equal)
    {
        const ProjectImplicitConversion conversion = classifyImplicit(
            right.typeKey,
            left.typeKey,
            right.integerConstant);
        if (!conversion.exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "cannot implicitly convert assignment RHS from '" + right.typeKey + "' to '" +
                    left.typeKey + "'");
            return {};
        }
        if (raiiTargetLocal.has_value())
        {
            movedRaiiLocals_.erase(*raiiTargetLocal);
            maybeMovedRaiiLocals_.erase(*raiiTargetLocal);
        }
        left.ownsStorageDestination = ownsStorageDestination;
        left.assignable = false;
        return left;
    }

    // ID 81. DESTINO PUNTERO: `p += n` y `p -= n` avanzan y retroceden POR ELEMENTOS, igual que
    // `p + n`. Ningun otro operador compuesto tiene sentido sobre un puntero, y `void*` queda
    // fuera por la misma razon que en `*p` y en `++p`: no hay tamano que avanzar.
    if (ProjectTypeConversion::isPointer(left.typeKey))
    {
        const bool offsetOperator = expression->tokenKind() == TokenKind::PlusEqual ||
            expression->tokenKind() == TokenKind::MinusEqual;
        if (!offsetOperator || left.typeKey == "builtin:void*" ||
            !ProjectTypeConversion::isIntegral(right.typeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "a non-void pointer supports only '+=' and '-=' with an integral right-hand side");
            return {};
        }
        left.assignable = false;
        return left;
    }

    const TokenKind compoundBase = lexer::compoundAssignmentBaseOperator(expression->tokenKind());
    const bool sameEnumBitwise = isEnumTypeKey(left.typeKey) && left.typeKey == right.typeKey &&
        (compoundBase == TokenKind::Amp || compoundBase == TokenKind::Pipe ||
         compoundBase == TokenKind::Caret);
    const bool shiftAssignment = isShift(compoundBase) &&
        ProjectTypeConversion::isIntegral(left.typeKey) &&
        ProjectTypeConversion::isIntegral(right.typeKey) &&
        left.typeKey != "builtin:bool" && right.typeKey != "builtin:bool";
    const std::optional<std::string> common = sameEnumBitwise || shiftAssignment
        ? std::optional<std::string>(left.typeKey)
        : ProjectTypeConversion::commonNumericType(
              left.typeKey, right.typeKey, left.integerConstant, right.integerConstant);
    if (!common.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "compound assignment requires compatible numeric operands in Stage 34");
        return {};
    }

    // ID 81. Y ADEMAS el operando derecho tiene que caber en el tipo del destino. C# aqui mete un
    // cast implicito de vuelta (`byte b; int n; b += n;` compila y TRUNCA); este lenguaje lo
    // rechaza y pide el cast escrito. Es una DIVERGENCIA DELIBERADA de C#, anotada en el doc 79:
    // el resultado se guarda en el destino, y un estrechamiento que el codigo no escribio es
    // justo la clase de perdida silenciosa que este compilador viene rechazando en todas partes.
    // La conversion constante sigue valiendo, asi que `byte b; b += 1;` compila sin ceremonia.
    const ProjectImplicitConversion narrowing = classifyImplicit(
        right.typeKey, left.typeKey, right.integerConstant);
    if (!shiftAssignment && !narrowing.exists())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "compound assignment cannot implicitly convert its right-hand side from '" +
                right.typeKey + "' to '" + left.typeKey +
                "'; write the read-modify-write with an explicit cast");
        return {};
    }
    left.assignable = false;
    return left;
}

std::optional<ProjectMethodBodyModel::ScopeName> ProjectMethodBodyModel::lookupScopeName(
    std::size_t scopeOrdinal,
    std::string_view name) const
{
    std::optional<std::size_t> current = scopeOrdinal;
    while (current.has_value() && *current < scopes_.size())
    {
        const std::optional<ScopeName> found = lookupScopeNameInScope(*current, name);
        if (found.has_value())
        {
            return found;
        }
        current = scopes_[*current].parentScopeOrdinal;
    }
    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ScopeName> ProjectMethodBodyModel::lookupScopeNameInScope(
    std::size_t scopeOrdinal,
    std::string_view name) const
{
    if (scopeOrdinal >= scopeNames_.size())
    {
        return std::nullopt;
    }
    const std::vector<ScopeName>& names = scopeNames_[scopeOrdinal];
    for (auto iterator = names.rbegin(); iterator != names.rend(); ++iterator)
    {
        if (iterator->name == name)
        {
            return *iterator;
        }
    }
    return std::nullopt;
}

std::vector<std::size_t> ProjectMethodBodyModel::memberCandidates(
    std::size_t methodMemberOrdinal,
    std::string_view name) const
{
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr)
    {
        return {};
    }
    return memberCandidatesForType(method->declaringTypeOrdinal, name);
}

std::vector<std::size_t> ProjectMethodBodyModel::memberCandidatesForType(
    std::size_t typeOrdinal,
    std::string_view name) const
{
    if (memberModel_ == nullptr)
    {
        return {};
    }

    if (interfaceModel_ != nullptr && interfaceModel_->isInterface(typeOrdinal))
    {
        const InterfaceTypeInfo* info = interfaceModel_->interfaceInfo(typeOrdinal);
        std::vector<std::size_t> result;
        if (info == nullptr)
        {
            return result;
        }
        for (const std::size_t memberOrdinal : info->effectiveMethodMemberOrdinals)
        {
            if (memberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[memberOrdinal].name == name)
            {
                result.push_back(memberOrdinal);
            }
        }
        return result;
    }

    std::optional<std::size_t> current = typeOrdinal;
    while (current.has_value())
    {
        const std::span<const std::size_t> found = memberModel_->findMembers(*current, name);
        std::vector<std::size_t> candidates(found.begin(), found.end());
        if (!candidates.empty())
        {
            return candidates;
        }
        current = inheritanceModel_ != nullptr
            ? inheritanceModel_->baseTypeOrdinal(*current)
            : std::nullopt;
    }
    return {};
}

std::optional<std::size_t> ProjectMethodBodyModel::referenceTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (inheritanceModel_ == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<std::size_t> ordinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
    if (!ordinal.has_value())
    {
        return std::nullopt;
    }
    if (inheritanceModel_->isClass(*ordinal) ||
        (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)))
    {
        return ordinal;
    }
    return std::nullopt;
}

std::optional<std::size_t> ProjectMethodBodyModel::namedTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (inheritanceModel_ == nullptr)
    {
        return std::nullopt;
    }
    return inheritanceModel_->typeOrdinalForTypeKey(typeKey);
}

std::optional<std::size_t> ProjectMethodBodyModel::classTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (inheritanceModel_ == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<std::size_t> ordinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
    if (!ordinal.has_value() || !inheritanceModel_->isClass(*ordinal))
    {
        return std::nullopt;
    }
    return ordinal;
}

std::optional<std::size_t> ProjectMethodBodyModel::resolveTypeIdentifier(
    std::size_t contextMemberOrdinal,
    std::string_view name) const noexcept
{
    const ClassMemberSymbol* context = methodSymbol(contextMemberOrdinal);
    if (context == nullptr || nameLookup_ == nullptr || context->declaringTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return std::nullopt;
    }

    const NamedTypeDeclaration& contextType = nameLookup_->namedTypes()[context->declaringTypeOrdinal];
    std::optional<std::size_t> fallback;
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& candidate = nameLookup_->namedTypes()[ordinal];
        if ((candidate.kind != NamedTypeDeclarationKind::Class &&
             candidate.kind != NamedTypeDeclarationKind::Struct &&
             candidate.kind != NamedTypeDeclarationKind::Enum) || candidate.identity.name() != name)
        {
            continue;
        }
        if (candidate.projectOrdinal == contextType.projectOrdinal &&
            candidate.identity.namespaceSegments() == contextType.identity.namespaceSegments())
        {
            return ordinal;
        }
        if (!fallback.has_value())
        {
            fallback = ordinal;
        }
        else
        {
            // Do not guess between ambiguous simple names from other visible namespaces/modules.
            fallback.reset();
        }
    }
    return fallback;
}

bool ProjectMethodBodyModel::contextHasImplicitThis(std::size_t memberOrdinal) const noexcept
{
    const ClassMemberSymbol* member = methodSymbol(memberOrdinal);
    return member != nullptr && member->hasImplicitThis;
}

bool ProjectMethodBodyModel::contextHasReadonlyReceiver(std::size_t memberOrdinal) const noexcept
{
    const ClassMemberSymbol* member = methodSymbol(memberOrdinal);
    return member != nullptr && member->declaringTypeIsStruct && member->hasImplicitThis &&
        member->isReadonly;
}

bool ProjectMethodBodyModel::readonlyFieldAssignable(
    const ClassMemberSymbol& field,
    std::size_t contextMemberOrdinal) const noexcept
{
    if (!field.isReadonly || field.isStatic)
    {
        return false;
    }
    const ClassMemberSymbol* context = methodSymbol(contextMemberOrdinal);
    return context != nullptr && context->kind == ClassMemberKind::Constructor &&
        context->declaringTypeOrdinal == field.declaringTypeOrdinal;
}

std::optional<std::size_t> ProjectMethodBodyModel::automaticPropertyBackingField(
    std::size_t typeOrdinal,
    std::string_view propertyName,
    bool isStatic) const noexcept
{
    if (memberModel_ == nullptr) return std::nullopt;
    for (const std::size_t ordinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.kind == ClassMemberKind::Field && member.isCompilerGenerated &&
            !member.isEventBacking && member.associatedName == propertyName &&
            member.isStatic == isStatic)
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::isCompileTimeConstantExpression(
    const ast::AstNode* expression,
    std::size_t contextMemberOrdinal) const
{
    if (expression == nullptr || memberModel_ == nullptr)
    {
        return false;
    }

    const auto expressionType = [this](const ast::AstNode* syntax, std::size_t context) -> std::string
    {
        for (auto iterator = expressionTypes_.rbegin(); iterator != expressionTypes_.rend(); ++iterator)
        {
            if (iterator->syntax == syntax && iterator->methodMemberOrdinal == context)
            {
                return iterator->typeKey;
            }
        }
        return {};
    };

    const auto convertValue = [](Stage37ConstantValue value, std::string_view target)
        -> std::optional<Stage37ConstantValue>
    {
        if (target == "builtin:bool")
        {
            if (value.kind != Stage37ConstantKind::Boolean) return std::nullopt;
            value.typeKey = std::string(target);
            return value;
        }
        if (target == "builtin:string")
        {
            if (value.kind != Stage37ConstantKind::String) return std::nullopt;
            value.typeKey = std::string(target);
            return value;
        }
        if (ProjectTypeConversion::isIntegral(target))
        {
            if (value.kind != Stage37ConstantKind::Integer) return std::nullopt;
            value.integer = normalizeIntegral(value.integer, target);
            value.typeKey = std::string(target);
            return value;
        }
        if (target == "builtin:float" || target == "builtin:double")
        {
            double converted = 0.0;
            if (value.kind == Stage37ConstantKind::Floating)
            {
                converted = value.floating;
            }
            else if (value.kind == Stage37ConstantKind::Integer)
            {
                converted = isSignedIntegralType(value.typeKey)
                    ? static_cast<double>(signedIntegral(value.integer, value.typeKey))
                    : static_cast<double>(normalizeIntegral(value.integer, value.typeKey));
            }
            else
            {
                return std::nullopt;
            }
            value.kind = Stage37ConstantKind::Floating;
            value.floating = normalizeFloatingConstant(converted, target);
            value.typeKey = std::string(target);
            return value;
        }
        return std::nullopt;
    };

    // NSC-LIMIT-019. Member ordinals are not dependency order. Track the active graph so valid
    // forward/cross-project references work while direct and transitive cycles remain invalid.
    std::unordered_set<std::size_t> evaluatingFields;
    evaluatingFields.insert(contextMemberOrdinal);

    const auto evaluate = [&](auto&& self, const ast::AstNode* node, std::size_t context)
        -> std::optional<Stage37ConstantValue>
    {
        if (node == nullptr) return std::nullopt;
        std::string resultType = expressionType(node, context);

        switch (node->kind())
        {
            case ast::AstNodeKind::IntegerLiteralExpression:
            {
                if (resultType.empty()) resultType = integerLiteralType(node);
                return Stage37ConstantValue{
                    Stage37ConstantKind::Integer,
                    resultType,
                    normalizeIntegral(node->integerValue(), resultType),
                    0.0,
                    false};
            }
            case ast::AstNodeKind::CharacterLiteralExpression:
            {
                if (resultType.empty()) resultType = "builtin:char";
                return Stage37ConstantValue{
                    Stage37ConstantKind::Integer,
                    resultType,
                    normalizeIntegral(node->integerValue(), resultType),
                    0.0,
                    false};
            }
            case ast::AstNodeKind::FloatingLiteralExpression:
            {
                if (resultType.empty())
                {
                    resultType = (!node->text().empty() &&
                        (node->text().back() == 'f' || node->text().back() == 'F'))
                        ? "builtin:float"
                        : "builtin:double";
                }
                return Stage37ConstantValue{
                    Stage37ConstantKind::Floating,
                    resultType,
                    0,
                    normalizeFloatingConstant(node->floatingValue(), resultType),
                    false};
            }
            case ast::AstNodeKind::BooleanLiteralExpression:
                return Stage37ConstantValue{
                    Stage37ConstantKind::Boolean,
                    "builtin:bool",
                    0,
                    0.0,
                    node->tokenKind() == TokenKind::KwTrue};
            case ast::AstNodeKind::StringLiteralExpression:
                return Stage37ConstantValue{
                    Stage37ConstantKind::String,
                    "builtin:string",
                    0,
                    0.0,
                    false,
                    node->text()};

            case ast::AstNodeKind::IdentifierExpression:
            case ast::AstNodeKind::MemberExpression:
            {
                for (const MethodNameBinding& binding : nameBindings_)
                {
                    if (binding.syntax != node || binding.methodMemberOrdinal != context ||
                        binding.candidateMemberOrdinals.empty())
                    {
                        continue;
                    }
                    const std::size_t ordinal = binding.candidateMemberOrdinals.front();
                    if (ordinal >= memberModel_->members().size())
                    {
                        return std::nullopt;
                    }
                    const ClassMemberSymbol& field = memberModel_->members()[ordinal];
                    if (field.kind != ClassMemberKind::Field || !field.isConst ||
                        field.syntax == nullptr || field.syntax->first() == nullptr)
                    {
                        return std::nullopt;
                    }
                    if (!evaluatingFields.insert(ordinal).second) return std::nullopt;
                    std::optional<Stage37ConstantValue> value = self(self, field.syntax->first(), ordinal);
                    evaluatingFields.erase(ordinal);
                    return value.has_value() ? convertValue(*value, field.declaredTypeKey) : std::nullopt;
                }
                return std::nullopt;
            }

            case ast::AstNodeKind::CastExpression:
            {
                const std::optional<Stage37ConstantValue> operand = self(self, node->first(), context);
                if (!operand.has_value() || resultType.empty()) return std::nullopt;
                // Stage 37 deliberately excludes floating-to-integral native const casts. This is
                // the exact same boundary enforced by ProjectOopMirModel.
                return convertValue(*operand, resultType);
            }

            case ast::AstNodeKind::UnaryExpression:
            {
                const std::optional<Stage37ConstantValue> operand = self(self, node->first(), context);
                if (!operand.has_value()) return std::nullopt;
                if (resultType.empty()) resultType = operand->typeKey;
                if (node->tokenKind() == TokenKind::Bang)
                {
                    if (operand->kind != Stage37ConstantKind::Boolean) return std::nullopt;
                    return Stage37ConstantValue{
                        Stage37ConstantKind::Boolean, "builtin:bool", 0, 0.0, !operand->boolean};
                }
                if (operand->kind == Stage37ConstantKind::Floating)
                {
                    if (node->tokenKind() == TokenKind::Plus)
                    {
                        return convertValue(*operand, resultType);
                    }
                    if (node->tokenKind() == TokenKind::Minus)
                    {
                        Stage37ConstantValue value = *operand;
                        value.floating = normalizeFloatingConstant(-value.floating, resultType);
                        value.typeKey = resultType;
                        return value;
                    }
                    return std::nullopt;
                }
                if (operand->kind != Stage37ConstantKind::Integer) return std::nullopt;
                std::optional<Stage37ConstantValue> converted = convertValue(*operand, resultType);
                if (!converted.has_value()) return std::nullopt;
                switch (node->tokenKind())
                {
                    case TokenKind::Plus: break;
                    case TokenKind::Minus:
                        converted->integer = normalizeIntegral(std::uint64_t{0} - converted->integer, resultType);
                        break;
                    case TokenKind::Tilde:
                        converted->integer = normalizeIntegral(~converted->integer, resultType);
                        break;
                    default: return std::nullopt;
                }
                return converted;
            }

            case ast::AstNodeKind::BinaryExpression:
            {
                const std::optional<Stage37ConstantValue> leftRaw = self(self, node->first(), context);
                const std::optional<Stage37ConstantValue> rightRaw = self(self, node->second(), context);
                if (!leftRaw.has_value() || !rightRaw.has_value()) return std::nullopt;
                const TokenKind operation = node->tokenKind();

                if (isLogical(operation))
                {
                    if (leftRaw->kind != Stage37ConstantKind::Boolean ||
                        rightRaw->kind != Stage37ConstantKind::Boolean)
                    {
                        return std::nullopt;
                    }
                    return Stage37ConstantValue{
                        Stage37ConstantKind::Boolean,
                        "builtin:bool",
                        0,
                        0.0,
                        operation == TokenKind::AmpAmp
                            ? (leftRaw->boolean && rightRaw->boolean)
                            : (leftRaw->boolean || rightRaw->boolean)};
                }

                if (isComparison(operation))
                {
                    if (leftRaw->kind == Stage37ConstantKind::Boolean ||
                        rightRaw->kind == Stage37ConstantKind::Boolean)
                    {
                        if (leftRaw->kind != Stage37ConstantKind::Boolean ||
                            rightRaw->kind != Stage37ConstantKind::Boolean ||
                            (operation != TokenKind::EqualEqual && operation != TokenKind::BangEqual))
                        {
                            return std::nullopt;
                        }
                        const bool equal = leftRaw->boolean == rightRaw->boolean;
                        return Stage37ConstantValue{
                            Stage37ConstantKind::Boolean, "builtin:bool", 0, 0.0,
                            operation == TokenKind::EqualEqual ? equal : !equal};
                    }
                    const std::optional<std::string> common = ProjectTypeConversion::commonNumericType(
                        leftRaw->typeKey, rightRaw->typeKey);
                    if (!common.has_value()) return std::nullopt;
                    const std::optional<Stage37ConstantValue> left = convertValue(*leftRaw, *common);
                    const std::optional<Stage37ConstantValue> right = convertValue(*rightRaw, *common);
                    if (!left.has_value() || !right.has_value()) return std::nullopt;
                    bool value = false;
                    if (left->kind == Stage37ConstantKind::Floating)
                    {
                        switch (operation)
                        {
                            case TokenKind::EqualEqual: value = left->floating == right->floating; break;
                            case TokenKind::BangEqual: value = left->floating != right->floating; break;
                            case TokenKind::Less: value = left->floating < right->floating; break;
                            case TokenKind::LessEqual: value = left->floating <= right->floating; break;
                            case TokenKind::Greater: value = left->floating > right->floating; break;
                            case TokenKind::GreaterEqual: value = left->floating >= right->floating; break;
                            default: return std::nullopt;
                        }
                    }
                    else if (isSignedIntegralType(*common))
                    {
                        const std::int64_t lhs = signedIntegral(left->integer, *common);
                        const std::int64_t rhs = signedIntegral(right->integer, *common);
                        switch (operation)
                        {
                            case TokenKind::EqualEqual: value = lhs == rhs; break;
                            case TokenKind::BangEqual: value = lhs != rhs; break;
                            case TokenKind::Less: value = lhs < rhs; break;
                            case TokenKind::LessEqual: value = lhs <= rhs; break;
                            case TokenKind::Greater: value = lhs > rhs; break;
                            case TokenKind::GreaterEqual: value = lhs >= rhs; break;
                            default: return std::nullopt;
                        }
                    }
                    else
                    {
                        switch (operation)
                        {
                            case TokenKind::EqualEqual: value = left->integer == right->integer; break;
                            case TokenKind::BangEqual: value = left->integer != right->integer; break;
                            case TokenKind::Less: value = left->integer < right->integer; break;
                            case TokenKind::LessEqual: value = left->integer <= right->integer; break;
                            case TokenKind::Greater: value = left->integer > right->integer; break;
                            case TokenKind::GreaterEqual: value = left->integer >= right->integer; break;
                            default: return std::nullopt;
                        }
                    }
                    return Stage37ConstantValue{
                        Stage37ConstantKind::Boolean, "builtin:bool", 0, 0.0, value};
                }

                if (!isArithmetic(operation)) return std::nullopt;
                if (resultType.empty()) return std::nullopt;
                const std::optional<Stage37ConstantValue> left = convertValue(*leftRaw, resultType);
                const std::optional<Stage37ConstantValue> right = convertValue(*rightRaw, resultType);
                if (!left.has_value() || !right.has_value()) return std::nullopt;

                if (left->kind == Stage37ConstantKind::Floating)
                {
                    double value = 0.0;
                    switch (operation)
                    {
                        case TokenKind::Plus: value = left->floating + right->floating; break;
                        case TokenKind::Minus: value = left->floating - right->floating; break;
                        case TokenKind::Star: value = left->floating * right->floating; break;
                        case TokenKind::Slash: value = left->floating / right->floating; break;
                        case TokenKind::Percent: value = std::fmod(left->floating, right->floating); break;
                        default: return std::nullopt;
                    }
                    return Stage37ConstantValue{
                        Stage37ConstantKind::Floating,
                        resultType,
                        0,
                        normalizeFloatingConstant(value, resultType),
                        false};
                }

                if (left->kind != Stage37ConstantKind::Integer ||
                    right->kind != Stage37ConstantKind::Integer)
                {
                    return std::nullopt;
                }
                const unsigned width = integralBitWidth(resultType);
                const std::uint64_t lhs = normalizeIntegral(left->integer, resultType);
                const std::uint64_t rhs = normalizeIntegral(right->integer, resultType);
                std::uint64_t value = 0;
                switch (operation)
                {
                    case TokenKind::Plus: value = lhs + rhs; break;
                    case TokenKind::Minus: value = lhs - rhs; break;
                    case TokenKind::Star: value = lhs * rhs; break;
                    case TokenKind::Amp: value = lhs & rhs; break;
                    case TokenKind::Pipe: value = lhs | rhs; break;
                    case TokenKind::Caret: value = lhs ^ rhs; break;
                    case TokenKind::LessLess:
                        if (rhs >= width) return std::nullopt;
                        value = lhs << static_cast<unsigned>(rhs);
                        break;
                    case TokenKind::GreaterGreater:
                        if (rhs >= width) return std::nullopt;
                        value = isSignedIntegralType(resultType)
                            ? static_cast<std::uint64_t>(
                                signedIntegral(lhs, resultType) >> static_cast<unsigned>(rhs))
                            : (lhs >> static_cast<unsigned>(rhs));
                        break;
                    case TokenKind::Slash:
                    case TokenKind::Percent:
                        if (rhs == 0) return std::nullopt;
                        if (isSignedIntegralType(resultType))
                        {
                            const std::int64_t signedLeft = signedIntegral(lhs, resultType);
                            const std::int64_t signedRight = signedIntegral(rhs, resultType);
                            const std::int64_t minimum = width == 64
                                ? std::numeric_limits<std::int64_t>::min()
                                : -(std::int64_t{1} << (width - 1U));
                            if (signedLeft == minimum && signedRight == -1)
                            {
                                value = operation == TokenKind::Slash
                                    ? static_cast<std::uint64_t>(minimum)
                                    : 0U;
                            }
                            else
                            {
                                value = static_cast<std::uint64_t>(operation == TokenKind::Slash
                                    ? signedLeft / signedRight
                                    : signedLeft % signedRight);
                            }
                        }
                        else
                        {
                            value = operation == TokenKind::Slash ? lhs / rhs : lhs % rhs;
                        }
                        break;
                    default: return std::nullopt;
                }
                return Stage37ConstantValue{
                    Stage37ConstantKind::Integer,
                    resultType,
                    normalizeIntegral(value, resultType),
                    0.0,
                    false};
            }

            default:
                return std::nullopt;
        }
    };

    return evaluate(evaluate, expression, contextMemberOrdinal).has_value();
}

std::optional<std::size_t> ProjectMethodBodyModel::delegateTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return std::nullopt;
    const std::string_view key = typeKey.substr(5);
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[ordinal];
        if (type.identity.key() == key && type.kind == NamedTypeDeclarationKind::Delegate)
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::isDelegateTypeKey(std::string_view typeKey) const noexcept
{
    return delegateTypeOrdinalForTypeKey(typeKey).has_value();
}

bool ProjectMethodBodyModel::delegateSignature(
    std::string_view delegateTypeKey,
    std::string& returnTypeKey,
    std::vector<std::string>& parameterTypeKeys,
    std::size_t& delegateTypeOrdinal) const
{
    returnTypeKey.clear();
    parameterTypeKeys.clear();
    const std::optional<std::size_t> ordinal = delegateTypeOrdinalForTypeKey(delegateTypeKey);
    if (!ordinal.has_value() || nameLookup_ == nullptr || memberModel_ == nullptr ||
        *ordinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[*ordinal];
    if (type.syntax == nullptr || type.syntax->kind() != ast::AstNodeKind::DelegateDeclaration)
    {
        return false;
    }
    returnTypeKey = memberModel_->canonicalTypeKey(type.syntax->typeSyntax(), *nameLookup_);
    for (const ast::AstNode* parameter : type.syntax->children())
    {
        if (parameter != nullptr && parameter->kind() == ast::AstNodeKind::ParameterDeclaration)
        {
            parameterTypeKeys.push_back(memberModel_->canonicalTypeKey(parameter->typeSyntax(), *nameLookup_));
        }
    }
    delegateTypeOrdinal = *ordinal;
    return !returnTypeKey.empty() && !isErrorType(returnTypeKey);
}

std::optional<std::size_t> ProjectMethodBodyModel::lambdaCaptureOrdinal(
    const MethodLambdaBinding& binding,
    MethodLambdaCaptureKind kind,
    std::optional<std::size_t> localOrdinal,
    std::optional<std::size_t> parameterOrdinal) const noexcept
{
    for (std::size_t index = 0; index < binding.captures.size(); ++index)
    {
        const MethodLambdaCapture& capture = binding.captures[index];
        if (capture.kind == kind && capture.localOrdinal == localOrdinal &&
            capture.parameterOrdinal == parameterOrdinal)
        {
            return index;
        }
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::collectLambdaCaptures(
    MethodLambdaBinding& binding,
    std::size_t nameBindingBegin,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool safe = true;
    const auto addCaptureTo = [&](MethodLambdaBinding& target, MethodLambdaCapture capture) -> std::size_t
    {
        const std::optional<std::size_t> existing = lambdaCaptureOrdinal(
            target, capture.kind, capture.localOrdinal, capture.parameterOrdinal);
        if (existing.has_value()) return *existing;
        const std::size_t ordinal = target.captures.size();
        target.captures.push_back(std::move(capture));
        return ordinal;
    };
    const auto routeThroughParents = [&](MethodLambdaCapture capture) -> MethodLambdaCapture
    {
        switch (capture.kind)
        {
            case MethodLambdaCaptureKind::Local:
                capture.sourceKind = MethodLambdaCaptureSourceKind::OriginalLocal;
                capture.sourceOrdinal = capture.localOrdinal;
                break;
            case MethodLambdaCaptureKind::Parameter:
                capture.sourceKind = MethodLambdaCaptureSourceKind::OriginalParameter;
                capture.sourceOrdinal = capture.parameterOrdinal;
                break;
            case MethodLambdaCaptureKind::ThisReference:
                capture.sourceKind = MethodLambdaCaptureSourceKind::OriginalThis;
                capture.sourceOrdinal.reset();
                break;
        }

        // activeLambdaBindings_ contains the lexical ancestors after the current lambda has been
        // popped.  Walk from the outermost to the immediate parent.  Every intermediate lambda
        // either owns the local/parameter itself or receives a transitive capture slot, producing
        // a real closure chain for arbitrary nesting depth.
        for (MethodLambdaBinding* ancestor : activeLambdaBindings_)
        {
            if (ancestor == nullptr) continue;

            bool ownedByAncestor = false;
            if (capture.kind == MethodLambdaCaptureKind::Local && capture.localOrdinal.has_value() &&
                *capture.localOrdinal < locals_.size())
            {
                const MethodLocalSymbol& local = locals_[*capture.localOrdinal];
                ownedByAncestor = local.lambdaOrdinal == ancestor->lambdaOrdinal;
                if (ownedByAncestor)
                {
                    if (local.lambdaParameterOrdinal.has_value())
                    {
                        capture.sourceKind = MethodLambdaCaptureSourceKind::ParentParameter;
                        capture.sourceOrdinal = local.lambdaParameterOrdinal;
                    }
                    else
                    {
                        capture.sourceKind = MethodLambdaCaptureSourceKind::ParentLocal;
                        capture.sourceOrdinal = capture.localOrdinal;
                    }
                }
            }
            if (ownedByAncestor) continue;

            const std::size_t parentCapture = addCaptureTo(*ancestor, capture);
            capture.sourceKind = MethodLambdaCaptureSourceKind::ParentCapture;
            capture.sourceOrdinal = parentCapture;
        }
        return capture;
    };

    for (std::size_t index = nameBindingBegin; index < nameBindings_.size(); ++index)
    {
        const auto owner = lambdaByNameBindingOrdinal_.find(index);
        if (owner == lambdaByNameBindingOrdinal_.end() || owner->second != binding.lambdaOrdinal)
            continue;
        const MethodNameBinding& name = nameBindings_[index];
        if (name.methodMemberOrdinal != methodMemberOrdinal || name.syntax == nullptr) continue;

        if (name.kind == MethodNameBindingKind::Local && name.localOrdinal.has_value() &&
            *name.localOrdinal < locals_.size())
        {
            const MethodLocalSymbol& local = locals_[*name.localOrdinal];
            if (local.lambdaOrdinal == binding.lambdaOrdinal)
            {
                if (local.lambdaParameterOrdinal.has_value())
                {
                    lambdaValueBindings_.push_back(MethodLambdaValueBinding{
                        name.syntax, binding.lambdaOrdinal, true,
                        *local.lambdaParameterOrdinal < binding.parameterByReference.size() &&
                            binding.parameterByReference[*local.lambdaParameterOrdinal],
                        *local.lambdaParameterOrdinal, local.typeKey});
                }
                continue;
            }
            // SHR-013: una VISTA capturada por un lambda es el mismo defecto que las dos de
            // abajo llegando por el tipo en vez de por el calificativo. El delegate se puede
            // guardar en un campo, devolver, o llamar horas despues; lo prestado no lo sabe.
            if (typeKeyIsViewLike(local.typeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    name.syntax->range(),
                    "view local '" + local.name + "' of type '" + local.typeKey +
                        "' cannot be captured by a lambda because the delegate may outlive what "
                        "the view borrows from");
                safe = false;
            }
            if (local.variableLengthArrayRootLocalOrdinal.has_value())
            {
                const std::size_t root = *local.variableLengthArrayRootLocalOrdinal;
                const std::string rootName = root < locals_.size()
                    ? locals_[root].name : local.name;
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    name.syntax->range(),
                    "pointer into variable-length array '" + rootName +
                        "' cannot be captured by a lambda because the delegate may outlive its stack frame");
                safe = false;
            }
            if (local.isRaii || local.ownership == sema::OwnershipKind::Borrowed)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    name.syntax->range(),
                    local.isRaii
                        ? "RAII owner '" + local.name +
                            "' cannot be captured by a lambda because the delegate may outlive the owner"
                        : "borrowed local '" + local.name +
                            "' cannot be captured by a lambda because the delegate may outlive its source owner");
                safe = false;
            }
            if (local.isReference)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    name.syntax->range(),
                    "reference local '" + local.name +
                        "' cannot be captured by a lambda because the delegate may outlive the referenced storage");
                safe = false;
            }
            MethodLambdaCapture captureDescription{
                MethodLambdaCaptureKind::Local, local.name, local.typeKey,
                *name.localOrdinal, std::nullopt,
                MethodLambdaCaptureSourceKind::OriginalLocal, *name.localOrdinal};
            captureDescription = routeThroughParents(std::move(captureDescription));
            const std::size_t capture = addCaptureTo(binding, std::move(captureDescription));
            lambdaValueBindings_.push_back(MethodLambdaValueBinding{
                name.syntax, binding.lambdaOrdinal, false, false, capture, local.typeKey});
            continue;
        }

        if (name.kind == MethodNameBindingKind::Parameter && name.parameterOrdinal.has_value())
        {
            const ClassMemberSymbol* owner = methodSymbol(methodMemberOrdinal);
            if (owner == nullptr || *name.parameterOrdinal >= owner->parameters.size()) continue;
            const MethodParameterSymbol& parameter = owner->parameters[*name.parameterOrdinal];
            // SHR-013: y lo mismo para un PARAMETRO de tipo vista. Un parametro sobrevive a la
            // llamada, pero no al delegate que lo captura.
            if (typeKeyIsViewLike(parameter.typeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    name.syntax->range(),
                    "view parameter '" + parameter.name + "' of type '" + parameter.typeKey +
                        "' cannot be captured by a lambda because the delegate may outlive what "
                        "the view borrows from");
                safe = false;
            }
            if (parameter.ownership == sema::OwnershipKind::Borrowed ||
                parameter.ownership == sema::OwnershipKind::Owned)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    name.syntax->range(),
                    parameter.ownership == sema::OwnershipKind::Borrowed
                        ? "borrowed parameter '" + parameter.name +
                            "' cannot be captured by a lambda because the delegate may outlive its source owner"
                        : "owned parameter '" + parameter.name +
                            "' cannot be captured by a lambda because the delegate may outlive the owner");
                safe = false;
            }
            MethodLambdaCapture captureDescription{
                MethodLambdaCaptureKind::Parameter, parameter.name, parameter.typeKey,
                std::nullopt, *name.parameterOrdinal,
                MethodLambdaCaptureSourceKind::OriginalParameter, *name.parameterOrdinal};
            captureDescription = routeThroughParents(std::move(captureDescription));
            const std::size_t capture = addCaptureTo(binding, std::move(captureDescription));
            lambdaValueBindings_.push_back(MethodLambdaValueBinding{
                name.syntax, binding.lambdaOrdinal, false, false, capture, parameter.typeKey});
            continue;
        }

        const bool needsThis =
            name.kind == MethodNameBindingKind::ThisReference ||
            name.kind == MethodNameBindingKind::BaseReference ||
            name.kind == MethodNameBindingKind::ImplicitField ||
            name.kind == MethodNameBindingKind::ImplicitMethodGroup ||
            name.kind == MethodNameBindingKind::ExplicitThisField ||
            name.kind == MethodNameBindingKind::ExplicitThisMethodGroup ||
            name.kind == MethodNameBindingKind::ExplicitBaseField ||
            name.kind == MethodNameBindingKind::ExplicitBaseMethodGroup ||
            name.kind == MethodNameBindingKind::ImplicitEvent ||
            name.kind == MethodNameBindingKind::ExplicitThisEvent;
        if (needsThis)
        {
            const std::string typeKey = implicitThisTypeKey(methodMemberOrdinal);
            MethodLambdaCapture captureDescription{
                MethodLambdaCaptureKind::ThisReference, "this", typeKey,
                std::nullopt, std::nullopt,
                MethodLambdaCaptureSourceKind::OriginalThis, std::nullopt};
            captureDescription = routeThroughParents(std::move(captureDescription));
            const std::size_t capture = addCaptureTo(binding, std::move(captureDescription));
            if (name.kind == MethodNameBindingKind::ThisReference || name.kind == MethodNameBindingKind::BaseReference)
            {
                lambdaValueBindings_.push_back(MethodLambdaValueBinding{
                    name.syntax, binding.lambdaOrdinal, false, false, capture, typeKey});
            }
        }
    }
    return safe;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::bindLambdaToDelegate(
    const ast::AstNode* syntax,
    std::string_view delegateTypeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (syntax == nullptr || syntax->kind() != ast::AstNodeKind::LambdaExpression)
        return result;
    std::string returnType;
    std::vector<std::string> parameterTypes;
    std::size_t delegateOrdinal = 0;
    if (!delegateSignature(delegateTypeKey, returnType, parameterTypes, delegateOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion, diagnostics::DiagnosticSeverity::Error,
            syntax->range(), "Stage 43 lambda target must be a delegate type");
        return result;
    }
    if (syntax->children().size() != parameterTypes.size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount, diagnostics::DiagnosticSeverity::Error,
            syntax->range(), "lambda parameter count does not match target delegate signature");
        return result;
    }

    MethodLambdaBinding binding;
    binding.syntax = syntax;
    binding.lambdaOrdinal = nextLambdaOrdinal_++;
    binding.parentLambdaOrdinal = currentLambdaOrdinal_;
    binding.ownerMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.delegateTypeKey = std::string(delegateTypeKey);
    binding.delegateTypeOrdinal = delegateOrdinal;
    binding.returnTypeKey = returnType;
    binding.parameterTypeKeys = parameterTypes;
    binding.blockBody = syntax->flag();
    binding.generatedName = "__nsc_lambda_" + std::to_string(methodMemberOrdinal) + "_" +
        std::to_string(binding.lambdaOrdinal);

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

    const std::size_t lambdaScope = createScope(MethodScopeKind::Block, methodMemberOrdinal, scopeOrdinal);
    std::unordered_set<std::string> parameterNames;
    for (std::size_t index = 0; index < syntax->children().size(); ++index)
    {
        const ast::AstNode* parameterSyntax = syntax->children()[index];
        const std::string name = parameterSyntax != nullptr ? parameterSyntax->text() : std::string{};
        if (name.empty() || !parameterNames.insert(name).second)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol, diagnostics::DiagnosticSeverity::Error,
                parameterSyntax != nullptr ? parameterSyntax->range() : syntax->range(),
                "duplicate lambda parameter '" + name + "'");
            return result;
        }

        // E11 typed lambdas.  An omitted type is contextual; a written type is a contract and
        // must match the target delegate exactly.  ref/out/in are represented as pointer types
        // by the parser, but their spelling still matters: accepting `ref` for an `out` target
        // would make definite-assignment and caller guarantees disagree even though both have
        // the same machine representation.
        const ast::AstNode* delegateParameter =
            index < delegateParameters.size() ? delegateParameters[index] : nullptr;
        const std::string lambdaPass = parameterSyntax != nullptr ? parameterSyntax->auxiliaryText() : std::string{};
        const std::string delegatePass = delegateParameter != nullptr
            ? delegateParameter->auxiliaryText() : std::string{};
        if (!lambdaPass.empty() || !delegatePass.empty())
        {
            if (lambdaPass != delegatePass)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    parameterSyntax != nullptr ? parameterSyntax->range() : syntax->range(),
                    "lambda parameter '" + name + "' pass mode does not match target delegate");
                return result;
            }
        }
        if (parameterSyntax != nullptr && parameterSyntax->typeSyntax() != nullptr)
        {
            const std::string explicitType =
                memberModel_->canonicalTypeKey(parameterSyntax->typeSyntax(), *nameLookup_);
            if (explicitType != parameterTypes[index])
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    parameterSyntax->range(),
                    "lambda parameter '" + name + "' has type '" + explicitType +
                        "' but target delegate requires '" + parameterTypes[index] + "'");
                return result;
            }
        }

        const bool byReference = !delegatePass.empty();
        const std::string valueType = byReference && !parameterTypes[index].empty() &&
                parameterTypes[index].back() == '*'
            ? parameterTypes[index].substr(0, parameterTypes[index].size() - 1)
            : parameterTypes[index];
        binding.parameterNames.push_back(name);
        binding.parameterValueTypeKeys.push_back(valueType);
        binding.parameterByReference.push_back(byReference);
        MethodLocalSymbol local;
        local.methodMemberOrdinal = methodMemberOrdinal;
        local.scopeOrdinal = lambdaScope;
        local.declarationOrdinal = scopes_[lambdaScope].localOrdinals.size();
        const ClassMemberSymbol* owner = methodSymbol(methodMemberOrdinal);
        if (owner != nullptr) local.fileId = owner->fileId;
        local.range = parameterSyntax != nullptr ? parameterSyntax->range() : syntax->range();
        local.syntax = parameterSyntax;
        local.name = name;
        local.typeKey = valueType;
        local.lambdaOrdinal = binding.lambdaOrdinal;
        local.lambdaParameterOrdinal = index;
        const std::size_t localOrdinal = locals_.size();
        locals_.push_back(std::move(local));
        scopes_[lambdaScope].localOrdinals.push_back(localOrdinal);
        scopeNames_[lambdaScope].push_back(ScopeName{
            ScopeNameKind::Local, name, localOrdinal,
            parameterSyntax != nullptr ? parameterSyntax->range() : syntax->range()});
        // C7: a lambda parameter is bound by the caller, exactly like a method parameter.
        assignedLocals_.insert(localOrdinal);
    }

    const std::size_t nameBindingBegin = nameBindings_.size();
    const OwnershipFlowSnapshot previousOwnershipFlow = captureOwnershipFlow();
    const std::optional<std::size_t> previousLambda = currentLambdaOrdinal_;
    const std::string previousReturn = currentLambdaReturnTypeKey_;
    currentLambdaOrdinal_ = binding.lambdaOrdinal;
    currentLambdaReturnTypeKey_ = returnType;
    activeLambdaBindings_.push_back(&binding);
    const std::size_t previousCatchDepth = currentCatchDepth_;
    currentCatchDepth_ = 0;

    bool bodyValid = true;
    if (syntax->first() == nullptr)
    {
        bodyValid = false;
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedExpression, diagnostics::DiagnosticSeverity::Error,
            syntax->range(), "lambda requires a body");
    }
    else if (binding.blockBody)
    {
        analyzeBlock(syntax->first(), methodMemberOrdinal, lambdaScope, false, diagnostics);
        bodyValid = !diagnostics.hasErrors();
    }
    else
    {
        ExpressionResult body = analyzeExpression(
            syntax->first(), methodMemberOrdinal, lambdaScope, diagnostics, isDelegateTypeKey(returnType));
        if (body.methodGroup && isDelegateTypeKey(returnType))
            body = bindMethodGroupToDelegate(
                syntax->first(), body, returnType, methodMemberOrdinal, lambdaScope, diagnostics);
        bodyValid = body.valid;
        if (body.valid && returnType == "builtin:void" && body.typeKey != "builtin:void")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion, diagnostics::DiagnosticSeverity::Error,
                syntax->first()->range(), "void delegate lambda expression body must have type void");
            bodyValid = false;
        }
        if (body.valid && returnType != "builtin:void" &&
            !classifyImplicit(body.typeKey, returnType, body.integerConstant).exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion, diagnostics::DiagnosticSeverity::Error,
                syntax->first()->range(), "lambda expression body type '" + body.typeKey +
                    "' is not implicitly convertible to delegate return type '" + returnType + "'");
            bodyValid = false;
        }
    }

    currentLambdaOrdinal_ = previousLambda;
    currentLambdaReturnTypeKey_ = previousReturn;
    currentCatchDepth_ = previousCatchDepth;
    activeLambdaBindings_.pop_back();
    restoreOwnershipFlow(previousOwnershipFlow);
    // Nested lambda bodies are analyzed while their parent body is being walked.  Claim only the
    // bindings not already claimed by a child, so the parent's capture scan cannot accidentally
    // treat names used solely inside a nested lambda as direct parent uses.
    for (std::size_t index = nameBindingBegin; index < nameBindings_.size(); ++index)
    {
        if (!lambdaByNameBindingOrdinal_.contains(index))
            lambdaByNameBindingOrdinal_[index] = binding.lambdaOrdinal;
    }
    if (!collectLambdaCaptures(binding, nameBindingBegin, methodMemberOrdinal, diagnostics))
    {
        bodyValid = false;
    }
    if (isExternCDelegateTypeKey(delegateTypeKey) && !binding.captures.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "extern C lambda must not capture state; a C function pointer has no environment channel");
        bodyValid = false;
    }
    lambdaBindings_.push_back(std::move(binding));

    if (!bodyValid) return result;
    result.typeKey = std::string(delegateTypeKey);
    result.valid = true;
    result.assignable = false;
    result.ownershipFreshOwner = !isExternCDelegateTypeKey(delegateTypeKey);
    recordExpressionType(syntax, methodMemberOrdinal, scopeOrdinal, result);
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::bindMethodGroupToDelegate(
    const ast::AstNode* syntax,
    const ExpressionResult& methodGroup,
    std::string_view delegateTypeKey,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (!methodGroup.methodGroup || memberModel_ == nullptr)
    {
        return result;
    }
    std::string returnType;
    std::vector<std::string> parameterTypes;
    std::size_t delegateOrdinal = 0;
    if (!delegateSignature(delegateTypeKey, returnType, parameterTypes, delegateOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "method group target is not a Stage 42 delegate type");
        return result;
    }

    std::vector<std::size_t> compatible;
    for (const std::size_t candidateOrdinal : methodGroup.methodCandidates)
    {
        if (candidateOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
        if (candidate.kind != ClassMemberKind::Method || candidate.parameters.size() != parameterTypes.size()) continue;
        if (candidate.declaredTypeKey != returnType) continue;
        bool exact = true;
        for (std::size_t index = 0; index < parameterTypes.size(); ++index)
        {
            if (candidate.parameters[index].typeKey != parameterTypes[index])
            {
                exact = false;
                break;
            }
        }
        if (exact) compatible.push_back(candidateOrdinal);
    }
    if (compatible.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "no method in the method group exactly matches delegate signature '" +
                std::string(delegateTypeKey) + "' in Stage 42");
        return result;
    }
    if (compatible.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "ambiguous method group conversion to delegate type '" + std::string(delegateTypeKey) + "'");
        return result;
    }

    const std::size_t selected = compatible.front();
    const ClassMemberSymbol& target = memberModel_->members()[selected];
    // Interop Foundation (callbacks C -> NativeSharp): an extern C delegate value IS a raw C
    // function pointer -- no cell, no receiver channel, no closure environment. Only a STATIC
    // method can convert; an instance method would need somewhere to smuggle `this` and a C
    // function pointer has none. The pointer produced is the internal function itself, whose
    // LLVM signature is the Win64 C ABI for every C-expressible type since Stage 40.2 --
    // certified against an MSVC-ABI host executing under Wine.
    {
        const std::optional<std::size_t> delegateType = delegateTypeOrdinalForTypeKey(delegateTypeKey);
        const bool externCTarget = delegateType.has_value() && nameLookup_ != nullptr &&
            *delegateType < nameLookup_->namedTypes().size() &&
            nameLookup_->namedTypes()[*delegateType].syntax != nullptr &&
            nameLookup_->namedTypes()[*delegateType].syntax->hasModifier(lexer::TokenKind::KwExtern);
        if (externCTarget && !target.isStatic)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                syntax != nullptr ? syntax->range() : SourceRange{},
                "extern C delegate requires a static method; a C function pointer has no receiver");
            return result;
        }
    }
    if (target.isAbstract || target.isVirtual || target.isOverride)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "Stage 42 delegate method-group conversion currently requires a non-virtual concrete target; virtual delegate thunks are deferred");
        return result;
    }
    if (!target.isStatic && target.declaringTypeIsStruct)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "Stage 42 delegate instance targets must be class references; struct receiver lifetime capture is deferred");
        return result;
    }
    MethodDelegateBinding binding;
    binding.kind = MethodDelegateOperationKind::Create;
    binding.syntax = syntax;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.delegateTypeKey = std::string(delegateTypeKey);
    binding.delegateTypeOrdinal = delegateOrdinal;
    binding.selectedMemberOrdinal = selected;
    binding.staticTarget = target.isStatic;
    if (!target.isStatic)
    {
        binding.receiverSyntax = methodGroup.receiverSyntax;
        binding.receiverTypeKey = methodGroup.receiverTypeKey;
        binding.receiverTypeOrdinal = methodGroup.receiverTypeOrdinal;
        if (binding.receiverSyntax == nullptr && contextHasImplicitThis(methodMemberOrdinal))
        {
            binding.receiverTypeKey = implicitThisTypeKey(methodMemberOrdinal);
            binding.receiverTypeOrdinal = target.declaringTypeOrdinal;
        }
    }
    binding.resultTypeKey = std::string(delegateTypeKey);
    delegateBindings_.push_back(std::move(binding));

    result.typeKey = std::string(delegateTypeKey);
    result.valid = true;
    result.assignable = false;
    if (!isExternCDelegateTypeKey(delegateTypeKey))
    {
        result.ownershipFreshOwner = true;
    }
    return result;
}

std::optional<std::size_t> ProjectMethodBodyModel::eventBackingMember(
    std::size_t eventMemberOrdinal) const noexcept
{
    if (memberModel_ == nullptr || eventMemberOrdinal >= memberModel_->members().size()) return std::nullopt;
    const ClassMemberSymbol& event = memberModel_->members()[eventMemberOrdinal];
    if (event.kind != ClassMemberKind::Event) return std::nullopt;
    for (const std::size_t ordinal : memberModel_->membersOfType(event.declaringTypeOrdinal))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[ordinal];
        if (candidate.isEventBacking && candidate.associatedName == event.name && candidate.isStatic == event.isStatic)
        {
            return ordinal;
        }
    }
    return std::nullopt;
}

ProjectImplicitConversion ProjectMethodBodyModel::classifyImplicit(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey,
    std::optional<ProjectIntegerConstant> integerConstant) const
{
    if (sourceTypeKey == "builtin:null" && isDelegateTypeKey(targetTypeKey))
    {
        return ProjectImplicitConversion{
            ProjectConversionKind::NullReference, 4, std::string(sourceTypeKey),
            std::string(targetTypeKey), std::nullopt};
    }
    if (inheritanceModel_ == nullptr)
    {
        return ProjectTypeConversion::classifyImplicit(
            sourceTypeKey,
            targetTypeKey,
            integerConstant,
            isReferenceTypeKey(targetTypeKey));
    }
    return ProjectConversionClassifier(*inheritanceModel_, interfaceModel_, memberModel_).classifyImplicit(
        sourceTypeKey,
        targetTypeKey,
        integerConstant);
}

ProjectImplicitConversion ProjectMethodBodyModel::classifyCVariadicPromotion(
    std::string_view sourceTypeKey) const
{
    std::string targetTypeKey(sourceTypeKey);
    ProjectConversionKind kind = ProjectConversionKind::Identity;

    if (sourceTypeKey == "builtin:float")
    {
        targetTypeKey = "builtin:double";
        kind = ProjectConversionKind::Numeric;
    }
    else if (sourceTypeKey == "builtin:bool")
    {
        targetTypeKey = "builtin:int";
        kind = ProjectConversionKind::IntegralPromotion;
    }
    else if (const builtins::IntegerTypeInfo* integer = builtins::findIntegerType(sourceTypeKey);
             integer != nullptr)
    {
        if (integer->width < 32U)
        {
            targetTypeKey = "builtin:int";
            kind = ProjectConversionKind::IntegralPromotion;
        }
    }
    else if (isEnumTypeKey(sourceTypeKey))
    {
        const std::string_view identity = sourceTypeKey.substr(5);
        for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
        {
            if (type.identity.key() != identity) continue;
            const std::string_view underlying = enumUnderlyingTypeKey(type.syntax);
            const builtins::IntegerTypeInfo* integer = builtins::findIntegerType(underlying);
            if (integer == nullptr)
            {
                return ProjectImplicitConversion{
                    ProjectConversionKind::None, 1000, std::string(sourceTypeKey), {}, std::nullopt};
            }
            if (integer->width < 32U)
            {
                targetTypeKey = "builtin:int";
            }
            else
            {
                targetTypeKey = underlying;
            }
            // An enum is passed through its C-compatible integer representation even when the
            // representation already has the width of int or wider. Keeping the named enum key
            // here would make the call-site type depend on a NativeSharp-only identity.
            kind = ProjectConversionKind::IntegralPromotion;
            break;
        }
    }
    else if (sourceTypeKey != "builtin:double" &&
             !isRawPointerTypeKey(sourceTypeKey) &&
             !isExternCDelegateTypeKey(sourceTypeKey))
    {
        return ProjectImplicitConversion{
            ProjectConversionKind::None, 1000, std::string(sourceTypeKey), {}, std::nullopt};
    }

    // Vararg conversions deliberately rank below every conversion to a written parameter. This
    // keeps a fixed overload preferable when both it and a variadic import are otherwise viable.
    return ProjectImplicitConversion{
        kind, 10, std::string(sourceTypeKey), std::move(targetTypeKey), std::nullopt};
}

std::string ProjectMethodBodyModel::implicitThisTypeKey(std::size_t methodMemberOrdinal) const
{
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr || nameLookup_ == nullptr ||
        method->declaringTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return "<error>";
    }
    return "type:" + nameLookup_->namedTypes()[method->declaringTypeOrdinal].identity.key();
}

bool ProjectMethodBodyModel::isClassReferenceTypeKey(std::string_view typeKey) const noexcept
{
    return classTypeOrdinalForTypeKey(typeKey).has_value();
}

// C4 "language references": the call site and the signature must agree about `ref`/`out`/`in`.
// Overload resolution has already separated the candidates by TYPE -- a by-reference parameter
// takes `T*`, a by-value one takes `T` -- so what is left is the readability rule C# has for the
// same reason: a call must SHOW that its argument can change. Writing the keyword is therefore
// required where the parameter declares one, and refused where it does not.
void ProjectMethodBodyModel::checkArgumentPassKinds(
    const ast::AstNode* syntax,
    std::string_view methodName,
    std::size_t memberOrdinal,
    const std::vector<ExpressionResult>& arguments,
    diagnostics::DiagnosticEngine& diagnostics,
    std::span<const std::size_t> parameterOrdinals) const
{
    if (memberModel_ == nullptr || memberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& chosen = memberModel_->members()[memberOrdinal];
    for (std::size_t index = 0; index < arguments.size() && index < chosen.parameters.size(); ++index)
    {
        const std::size_t parameterOrdinal = parameterOrdinals.empty() ? index : parameterOrdinals[index];
        if (parameterOrdinal >= chosen.parameters.size()) continue;
        const ParameterPassKind expected = chosen.parameters[parameterOrdinal].passKind;
        const ParameterPassKind written = arguments[index].argumentPassKind;
        if (expected == written) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            expected == ParameterPassKind::Value
                ? "argument " + std::to_string(index + 1) + " of '" + std::string(methodName) +
                    "' is not declared '" + parameterPassKindKeyword(written) + "'"
                : "argument " + std::to_string(index + 1) + " of '" + std::string(methodName) +
                    "' must be passed with '" + parameterPassKindKeyword(expected) + "'");
    }
}

// C6 "struct members": the type ordinal of anything that can carry ACCESSOR members -- a class,
// an interface or a struct. Distinct from referenceTypeOrdinalForTypeKey, which answers only for
// the two that are references: a struct is a value, and that difference matters for how the
// receiver is passed (by address) but not at all for whether it may declare an indexer.
std::optional<std::size_t> ProjectMethodBodyModel::accessorReceiverTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (inheritanceModel_ == nullptr) return std::nullopt;
    const std::optional<std::size_t> ordinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
    if (!ordinal.has_value()) return std::nullopt;
    if (inheritanceModel_->isClass(*ordinal) ||
        (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)))
    {
        return ordinal;
    }
    if (nameLookup_ != nullptr && *ordinal < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*ordinal].kind == NamedTypeDeclarationKind::Struct)
    {
        return ordinal;
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::isReferenceTypeKey(std::string_view typeKey) const noexcept
{
    return referenceTypeOrdinalForTypeKey(typeKey).has_value();
}

bool ProjectMethodBodyModel::typeKeyIsOwnable(std::string_view typeKey) const noexcept
{
    // La cadena nativa entra aqui aunque no sea una referencia de clase: posee su propio
    // almacenamiento y `owned string` es legal. Es la misma expresion que la resolucion de
    // llamadas usaba en linea antes de NSC-LIMIT-010.
    return isReferenceTypeKey(typeKey) || typeKey == "builtin:string" ||
        (isAnyDelegateTypeKey(typeKey) && !isExternCDelegateTypeKey(typeKey));
}

// C1.1 "function-pointer round trip" -- the type-key predicates for the three sides of the trip.
// A named type key is "type:<identity>"; the delegate half of the answer therefore needs the name
// lookup (an enum, a struct and a delegate all share that prefix), and "is it EXTERN C" is the
// same `extern` modifier probe the backend's isExternCDelegateType already uses -- one rule, two
// places that must agree, so both read the syntax modifier rather than a cached flag.
bool ProjectMethodBodyModel::isAnyDelegateTypeKey(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view key = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != key) continue;
        return type.kind == NamedTypeDeclarationKind::Delegate;
    }
    return false;
}

std::optional<MethodOptionalCarrierBinding>
ProjectMethodBodyModel::resolveOptionalCarrier(std::string_view typeKey) const
{
    if (genericModel_ == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr)
        return std::nullopt;

    const GenericSpecializationRecord* specialization =
        genericModel_->specializationForTypeKey(typeKey);
    if (specialization == nullptr || specialization->kind != GenericTemplateKind::Struct ||
        specialization->typeArguments.size() != 1U)
    {
        return std::nullopt;
    }

    const std::span<const GenericTemplateRecord> templates = genericModel_->templates();
    const auto source = std::ranges::find_if(templates, [&](const GenericTemplateRecord& candidate)
    {
        return candidate.canonicalKey == specialization->templateKey;
    });
    if (source == templates.end() || source->namespaceName != "NativeSharp.Core" ||
        !source->ownerName.empty() || source->name != "Optional" ||
        source->parameters.size() != 1U)
    {
        return std::nullopt;
    }

    const std::optional<std::size_t> typeOrdinal =
        accessorReceiverTypeOrdinalForTypeKey(typeKey);
    if (!typeOrdinal.has_value()) return std::nullopt;

    const std::string valueTypeKey = ProjectMemberModel::canonicalGenericArgumentKey(
        specialization->typeArguments.front(), *nameLookup_);
    if (valueTypeKey.starts_with("unresolved:")) return std::nullopt;

    std::optional<std::size_t> hasValueMemberOrdinal;
    std::optional<std::size_t> uncheckedValueMemberOrdinal;
    const std::span<const ClassMemberSymbol> members = memberModel_->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const ClassMemberSymbol& member = members[ordinal];
        if (member.declaringTypeOrdinal != *typeOrdinal ||
            member.kind != ClassMemberKind::Method || member.isStatic ||
            member.accessibility != ClassMemberAccessibility::Public ||
            !member.parameters.empty())
        {
            continue;
        }

        if (member.name == "HasValue" && member.declaredTypeKey == "builtin:bool")
        {
            if (hasValueMemberOrdinal.has_value()) return std::nullopt;
            hasValueMemberOrdinal = ordinal;
        }
        else if (member.name == "UncheckedValue" && member.declaredTypeKey == valueTypeKey)
        {
            if (uncheckedValueMemberOrdinal.has_value()) return std::nullopt;
            uncheckedValueMemberOrdinal = ordinal;
        }
    }

    if (!hasValueMemberOrdinal.has_value() || !uncheckedValueMemberOrdinal.has_value())
        return std::nullopt;

    MethodOptionalCarrierBinding binding;
    binding.carrierTypeKey = std::string(typeKey);
    binding.valueTypeKey = valueTypeKey;
    binding.hasValueMemberOrdinal = *hasValueMemberOrdinal;
    binding.uncheckedValueMemberOrdinal = *uncheckedValueMemberOrdinal;
    return binding;
}

bool ProjectMethodBodyModel::isEnumTypeKey(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view key = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != key) continue;
        return type.kind == NamedTypeDeclarationKind::Enum;
    }
    return false;
}

// CC1 fases 4B/4C -- ver la declaracion para el criterio.
//
// Se pregunta por DOS cosas, y las dos por separado porque son dos razones distintas:
//
//   * un destructor declarado: alguien escribio codigo que TIENE que correr al final de la vida
//     del objeto, y en memoria cruda no hay quien lo llame;
//   * un campo `owned`: el objeto posee otra asignacion, y liberar (o abandonar) el contenedor
//     sin descargarla la filtra.
//
// Los builtins y los punteros son triviales por construccion. Un enum tambien. Una CLASE como
// elemento tampoco es problema: lo que se reserva es un buffer de REFERENCIAS, no de objetos --
// las referencias son punteros y no poseen nada por si mismas.
bool ProjectMethodBodyModel::elementTypeHasTrivialLifetime(std::string_view typeKey) const noexcept
{
    if (const std::optional<std::vector<std::string>> tuple =
            ProjectTypeConversion::tupleElementTypeKeys(typeKey);
        tuple.has_value())
    {
        return std::ranges::all_of(*tuple, [&](const std::string& element)
        {
            return elementTypeHasTrivialLifetime(element);
        });
    }
    if (memberModel_ == nullptr || nameLookup_ == nullptr) return true;
    if (!typeKey.starts_with("type:")) return true;

    const std::string_view identity = typeKey.substr(5);
    std::optional<std::size_t> typeOrdinal;
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        if (nameLookup_->namedTypes()[ordinal].identity.key() == identity)
        {
            typeOrdinal = ordinal;
            break;
        }
    }
    if (!typeOrdinal.has_value()) return true;
    if (*typeOrdinal < nameLookup_->namedTypes().size())
    {
        const NamedTypeDeclarationKind kind = nameLookup_->namedTypes()[*typeOrdinal].kind;
        // Una referencia no posee nada por si misma; un enum es un entero con nombre.
        if (kind != NamedTypeDeclarationKind::Struct) return true;
    }

    for (const ClassMemberSymbol& member : memberModel_->members())
    {
        if (member.declaringTypeOrdinal != *typeOrdinal) continue;
        if (member.kind == ClassMemberKind::Destructor) return false;
        if (member.kind == ClassMemberKind::Field && !member.isStatic &&
            member.declaredOwnership == sema::OwnershipKind::Owned)
        {
            return false;
        }
    }
    return true;
}

bool ProjectMethodBodyModel::isExternCDelegateTypeKey(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view key = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != key) continue;
        return type.kind == NamedTypeDeclarationKind::Delegate && type.syntax != nullptr &&
            type.syntax->hasModifier(lexer::TokenKind::KwExtern);
    }
    return false;
}

bool ProjectMethodBodyModel::isRawPointerTypeKey(std::string_view typeKey) noexcept
{
    // `T*` / `void*`. Native arrays ("...[]") are a different shape and are deliberately not
    // pointers here, even though both lower to `ptr`.
    return !typeKey.empty() && typeKey.back() == '*';
}

bool ProjectMethodBodyModel::isPointerSizedIntegralTypeKey(std::string_view typeKey) noexcept
{
    // Deliberately NOT every integral: only the two spellings whose whole purpose is to hold an
    // address. Casting a function pointer through `int` would silently truncate on Win64.
    return typeKey == "builtin:nint" || typeKey == "builtin:nuint";
}

bool ProjectMethodBodyModel::canRuntimeReferenceConvert(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey) const noexcept
{
    if (sourceTypeKey == "builtin:null")
    {
        return isReferenceTypeKey(targetTypeKey);
    }
    if (inheritanceModel_ == nullptr || nameLookup_ == nullptr)
    {
        return false;
    }
    const std::optional<std::size_t> source = referenceTypeOrdinalForTypeKey(sourceTypeKey);
    const std::optional<std::size_t> target = referenceTypeOrdinalForTypeKey(targetTypeKey);
    if (!source.has_value() || !target.has_value())
    {
        return false;
    }
    if (*source == *target)
    {
        return true;
    }

    const bool sourceClass = inheritanceModel_->isClass(*source);
    const bool targetClass = inheritanceModel_->isClass(*target);
    const bool sourceInterface = interfaceModel_ != nullptr && interfaceModel_->isInterface(*source);
    const bool targetInterface = interfaceModel_ != nullptr && interfaceModel_->isInterface(*target);

    if (sourceClass && targetClass)
    {
        return inheritanceModel_->isDerivedFrom(*source, *target) ||
            inheritanceModel_->isDerivedFrom(*target, *source);
    }
    if (sourceInterface && targetInterface)
    {
        return true;
    }
    if (sourceClass && targetInterface)
    {
        const bool implements = interfaceModel_ != nullptr &&
            interfaceModel_->classImplementsInterface(*source, *target);
        return implements || (*source < nameLookup_->namedTypes().size() &&
            !nameLookup_->namedTypes()[*source].isSealed);
    }
    if (sourceInterface && targetClass)
    {
        const bool implements = interfaceModel_ != nullptr &&
            interfaceModel_->classImplementsInterface(*target, *source);
        return implements || (*target < nameLookup_->namedTypes().size() &&
            !nameLookup_->namedTypes()[*target].isSealed);
    }
    return false;
}

const ClassMemberSymbol* ProjectMethodBodyModel::methodSymbol(
    std::size_t methodMemberOrdinal) const noexcept
{
    if (memberModel_ == nullptr) return nullptr;
    if (methodMemberOrdinal < memberModel_->members().size())
        return &memberModel_->members()[methodMemberOrdinal];
    if (methodMemberOrdinal < namespaceCallableBase_) return nullptr;
    const std::size_t functionOrdinal = methodMemberOrdinal - namespaceCallableBase_;
    return functionOrdinal < namespaceCallableAdapters_.size()
        ? &namespaceCallableAdapters_[functionOrdinal] : nullptr;
}

bool ProjectMethodBodyModel::betterCandidate(
    const MethodCallCandidate& left,
    const MethodCallCandidate& right) const noexcept
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
    if (!strictlyBetter && left.defaultedParameterCount != right.defaultedParameterCount)
    {
        return left.defaultedParameterCount < right.defaultedParameterCount;
    }
    if (!strictlyBetter && left.isVariadicCandidate != right.isVariadicCandidate)
    {
        return !left.isVariadicCandidate;
    }
    if (!strictlyBetter &&
        left.usesExpandedParameterPack != right.usesExpandedParameterPack)
    {
        return !left.usesExpandedParameterPack;
    }
    return strictlyBetter;
}

void ProjectMethodBodyModel::recordExpressionType(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    const ExpressionResult& result)
{
    if (expression == nullptr || !result.valid || result.methodGroup || isErrorType(result.typeKey))
    {
        return;
    }
    // NSC-LIMIT-010: se copian TAMBIEN ownership, transferencia y origen. Antes se quedaban en
    // ExpressionResult, y ProjectConstructionModel --que solo ve estos bindings-- no podia
    // distinguir `move(duenio)` de `duenio`. Los campos se nombran en vez de ir por posicion: la
    // lista crecio a nueve, y un inicializador agregado posicional de nueve campos es un error de
    // una linea esperando a que alguien inserte un miembro en medio.
    MethodExpressionTypeBinding binding;
    binding.syntax = expression;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.typeKey = result.typeKey;
    binding.integerConstant = result.integerConstant;
    binding.ownership = result.ownership;
    binding.ownershipTransfer = result.ownershipTransfer;
    binding.ownershipFreshOwner = result.ownershipFreshOwner;
    binding.ownershipSourceLocalOrdinal = result.ownershipSourceLocalOrdinal;
    binding.ownershipSourceParameterOrdinal = result.ownershipSourceParameterOrdinal;
    binding.ownsStorageDestination = result.ownsStorageDestination;
    binding.objectLifetimeOwnership = result.objectLifetimeOwnership;
    binding.storageOwnership = result.storageOwnership;
    expressionTypes_.push_back(std::move(binding));
}

void ProjectMethodBodyModel::reportPrevious(
    SourceRange range,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string message) const
{
    diagnostics.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        range,
        std::move(message));
}

const char* methodScopeKindName(MethodScopeKind kind) noexcept
{
    switch (kind)
    {
        case MethodScopeKind::Method: return "method";
        case MethodScopeKind::Constructor: return "constructor";
        case MethodScopeKind::Destructor: return "destructor";
        case MethodScopeKind::FieldInitializer: return "field-initializer";
        case MethodScopeKind::Block: return "block";
        case MethodScopeKind::For: return "for";
        case MethodScopeKind::Foreach: return "foreach";
        case MethodScopeKind::Catch: return "catch";
        case MethodScopeKind::Using: return "using";
    }
    return "method";
}

const char* methodNameBindingKindName(MethodNameBindingKind kind) noexcept
{
    switch (kind)
    {
        case MethodNameBindingKind::Parameter: return "parameter";
        case MethodNameBindingKind::Local: return "local";
        case MethodNameBindingKind::ImplicitField: return "implicit-field";
        case MethodNameBindingKind::ImplicitMethodGroup: return "implicit-method-group";
        case MethodNameBindingKind::ThisReference: return "this";
        case MethodNameBindingKind::BaseReference: return "base";
        case MethodNameBindingKind::ExplicitThisField: return "this-field";
        case MethodNameBindingKind::ExplicitThisMethodGroup: return "this-method-group";
        case MethodNameBindingKind::ExplicitObjectField: return "object-field";
        case MethodNameBindingKind::ExplicitObjectMethodGroup: return "object-method-group";
        case MethodNameBindingKind::ExplicitBaseField: return "base-field";
        case MethodNameBindingKind::ExplicitBaseMethodGroup: return "base-method-group";
        case MethodNameBindingKind::StaticField: return "static-field";
        case MethodNameBindingKind::NamespaceState: return "namespace-state";
        case MethodNameBindingKind::StaticMethodGroup: return "static-method-group";
        case MethodNameBindingKind::ImplicitEvent: return "implicit-event";
        case MethodNameBindingKind::ExplicitThisEvent: return "this-event";
        case MethodNameBindingKind::ExplicitObjectEvent: return "object-event";
        case MethodNameBindingKind::StaticEvent: return "static-event";
        case MethodNameBindingKind::TypeReference: return "type-reference";
    }
    return "local";
}

const char* methodShadowingKindName(MethodShadowingKind kind) noexcept
{
    switch (kind)
    {
        case MethodShadowingKind::ParameterShadowsMember: return "parameter-shadows-member";
        case MethodShadowingKind::LocalShadowsMember: return "local-shadows-member";
        case MethodShadowingKind::LocalShadowsParameter: return "local-shadows-parameter";
        case MethodShadowingKind::LocalShadowsLocal: return "local-shadows-local";
    }
    return "local-shadows-local";
}

const char* methodCallResolutionKindName(MethodCallResolutionKind kind) noexcept
{
    switch (kind)
    {
        case MethodCallResolutionKind::Resolved: return "resolved";
        case MethodCallResolutionKind::NoViableCandidate: return "no-viable-candidate";
        case MethodCallResolutionKind::Ambiguous: return "ambiguous";
    }
    return "no-viable-candidate";
}

const char* methodRuntimeTypeOperationKindName(MethodRuntimeTypeOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodRuntimeTypeOperationKind::TypeTest: return "is";
        case MethodRuntimeTypeOperationKind::SafeCast: return "as";
        case MethodRuntimeTypeOperationKind::CheckedCast: return "checked-cast";
    }
    return "is";
}

const char* methodExceptionOperationKindName(MethodExceptionOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodExceptionOperationKind::Catch: return "catch";
        case MethodExceptionOperationKind::Throw: return "throw";
        case MethodExceptionOperationKind::Rethrow: return "rethrow";
    }
    return "throw";
}

const char* methodCleanupRegionKindName(MethodCleanupRegionKind kind) noexcept
{
    switch (kind)
    {
        case MethodCleanupRegionKind::Finally: return "finally";
        case MethodCleanupRegionKind::UsingDispose: return "using-dispose";
        case MethodCleanupRegionKind::RaiiDestroy: return "raii-destroy";
        case MethodCleanupRegionKind::OwnedParameterDestroy: return "owned-parameter-destroy";
    }
    return "finally";
}

const char* methodStructuredExitKindName(MethodStructuredExitKind kind) noexcept
{
    switch (kind)
    {
        case MethodStructuredExitKind::Return: return "return";
        case MethodStructuredExitKind::Break: return "break";
        case MethodStructuredExitKind::Continue: return "continue";
    }
    return "return";
}

const char* methodCleanupContinuationKindName(MethodCleanupContinuationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCleanupContinuationKind::Normal: return "normal";
        case MethodCleanupContinuationKind::Return: return "return";
        case MethodCleanupContinuationKind::Break: return "break";
        case MethodCleanupContinuationKind::Continue: return "continue";
        case MethodCleanupContinuationKind::Exception: return "exception";
    }
    return "normal";
}

const char* methodOwnershipTransferKindName(MethodOwnershipTransferKind kind) noexcept
{
    switch (kind)
    {
        case MethodOwnershipTransferKind::MoveLocalToOwnedParameter: return "move-local-to-owned-parameter";
        case MethodOwnershipTransferKind::MoveParameterToOwnedParameter: return "move-parameter-to-owned-parameter";
        case MethodOwnershipTransferKind::OwnedReturnToOwnedParameter: return "owned-return-to-owned-parameter";
        case MethodOwnershipTransferKind::MoveLocalToOwnedReturn: return "move-local-to-owned-return";
        case MethodOwnershipTransferKind::MoveParameterToOwnedReturn: return "move-parameter-to-owned-return";
        case MethodOwnershipTransferKind::OwnedReturnForward: return "owned-return-forward";
    }
    return "unknown";
}

const char* methodBorrowKindName(MethodBorrowKind kind) noexcept
{
    switch (kind)
    {
        case MethodBorrowKind::OwnerLocalToBorrowedParameter: return "owner-local-to-borrowed-parameter";
        case MethodBorrowKind::OwnedParameterToBorrowedParameter: return "owned-parameter-to-borrowed-parameter";
        case MethodBorrowKind::BorrowedParameterToBorrowedParameter: return "borrowed-parameter-to-borrowed-parameter";
        case MethodBorrowKind::BorrowedLocalToBorrowedParameter: return "borrowed-local-to-borrowed-parameter";
        case MethodBorrowKind::BorrowedReturnToBorrowedParameter: return "borrowed-return-to-borrowed-parameter";
        case MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter:
            return "fresh-owned-temporary-to-borrowed-parameter";
        case MethodBorrowKind::BorrowedParameterToBorrowedReturn: return "borrowed-parameter-to-borrowed-return";
        case MethodBorrowKind::BorrowedLocalToBorrowedReturn: return "borrowed-local-to-borrowed-return";
        case MethodBorrowKind::BorrowedReturnForward: return "borrowed-return-forward";
    }
    return "unknown";
}

const char* methodLocalBorrowSourceKindName(MethodLocalBorrowSourceKind kind) noexcept
{
    switch (kind)
    {
        case MethodLocalBorrowSourceKind::OwnerLocal: return "owner-local";
        case MethodLocalBorrowSourceKind::OwnedParameter: return "owned-parameter";
        case MethodLocalBorrowSourceKind::BorrowedParameter: return "borrowed-parameter";
        case MethodLocalBorrowSourceKind::BorrowedLocal: return "borrowed-local";
    }
    return "unknown";
}

const char* methodBorrowStateName(MethodBorrowState state) noexcept
{
    switch (state)
    {
        case MethodBorrowState::Valid: return "valid";
        case MethodBorrowState::Invalidated: return "invalidated";
        case MethodBorrowState::MaybeInvalid: return "maybe-invalid";
    }
    return "unknown";
}

const char* methodCoreOperationKindName(MethodCoreOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodCoreOperationKind::ArrayAllocation: return "array-allocation";
        case MethodCoreOperationKind::RangeArrayAllocation: return "range-array-allocation";
        case MethodCoreOperationKind::ArrayLength: return "array-length";
        case MethodCoreOperationKind::FixedArrayLength: return "fixed-array-length";
        case MethodCoreOperationKind::ArrayElement: return "array-element";
        case MethodCoreOperationKind::RangeArrayElement: return "range-array-element";
        case MethodCoreOperationKind::RawElement: return "raw-element";
        case MethodCoreOperationKind::StringLiteral: return "string-literal";
        case MethodCoreOperationKind::StringLength: return "string-length";
        case MethodCoreOperationKind::StringElement: return "string-element";
        case MethodCoreOperationKind::StringConcat: return "string-concat";
        case MethodCoreOperationKind::StringEquals: return "string-equals";
        case MethodCoreOperationKind::StringSubstring: return "string-substring";
        case MethodCoreOperationKind::StringCharacterPointer: return "string-character-pointer";
        case MethodCoreOperationKind::StringFromCharArray: return "string-from-char-array";
        case MethodCoreOperationKind::ConsoleWriteLine: return "console-writeline";
        case MethodCoreOperationKind::ConsoleWrite: return "console-write";
        case MethodCoreOperationKind::ConsoleErrorWriteLine: return "console-error-writeline";
        case MethodCoreOperationKind::ConsoleErrorWrite: return "console-error-write";
        case MethodCoreOperationKind::ConsoleReadLine: return "console-readline";
        case MethodCoreOperationKind::StructToString: return "struct-to-string";
        case MethodCoreOperationKind::EnumToString: return "enum-to-string";
        case MethodCoreOperationKind::StructEquals: return "struct-equals";
        case MethodCoreOperationKind::StructHashCode: return "struct-hash-code";
        case MethodCoreOperationKind::StructClone: return "struct-clone";
        case MethodCoreOperationKind::ObjectTypeName: return "object-type-name";
        case MethodCoreOperationKind::ObjectReferenceHash: return "object-reference-hash";
        case MethodCoreOperationKind::SizeofConstant: return "sizeof-constant";
        case MethodCoreOperationKind::AlignofConstant: return "alignof-constant";
        case MethodCoreOperationKind::OffsetofConstant: return "offsetof-constant";
        case MethodCoreOperationKind::StackAllocation: return "stack-allocation";
        case MethodCoreOperationKind::RawAllocation: return "raw-allocation";
        case MethodCoreOperationKind::PointerArithmetic: return "pointer-arithmetic";
        case MethodCoreOperationKind::ClockQueryCounter: return "clock-query-counter";
        case MethodCoreOperationKind::ClockQueryFrequency: return "clock-query-frequency";
        case MethodCoreOperationKind::ClockQuerySystemFileTime: return "clock-query-system-filetime";
        case MethodCoreOperationKind::EnvironmentTickCount64: return "environment-tick-count64";
        case MethodCoreOperationKind::EnvironmentProcessorCount: return "environment-processor-count";
        case MethodCoreOperationKind::EnvironmentExit: return "environment-exit";
        case MethodCoreOperationKind::EnvironmentGetVariable: return "environment-get-variable";
        case MethodCoreOperationKind::EnvironmentSetVariable: return "environment-set-variable";
        case MethodCoreOperationKind::FileExists: return "file-exists";
        case MethodCoreOperationKind::FileDelete: return "file-delete";
        case MethodCoreOperationKind::DirectoryExists: return "directory-exists";
        case MethodCoreOperationKind::DirectoryCreate: return "directory-create";
        case MethodCoreOperationKind::FileReadAllText: return "file-read-all-text";
        case MethodCoreOperationKind::FileGetSize: return "file-get-size";
        case MethodCoreOperationKind::FileReadAllBytes: return "file-read-all-bytes";
        case MethodCoreOperationKind::FileReadBytes: return "file-read-bytes";
        case MethodCoreOperationKind::FileWriteAllBytes: return "file-write-all-bytes";
        case MethodCoreOperationKind::EnvironmentCurrentDirectory: return "environment-current-directory";
        case MethodCoreOperationKind::ProcessRun: return "process-run";
        case MethodCoreOperationKind::Panic: return "panic";
        case MethodCoreOperationKind::FileWriteAllText: return "file-write-all-text";
        case MethodCoreOperationKind::FileAppendAllText: return "file-append-all-text";
        case MethodCoreOperationKind::DirectoryDelete: return "directory-delete";
        case MethodCoreOperationKind::PathGetFullPath: return "path-get-full-path";
        case MethodCoreOperationKind::EnvironmentGetCommandLineArgCount: return "environment-get-command-line-arg-count";
        case MethodCoreOperationKind::EnvironmentGetCommandLineArgs: return "environment-get-command-line-args";
        case MethodCoreOperationKind::DirectoryGetFileCount: return "directory-get-file-count";
        case MethodCoreOperationKind::DirectoryGetFiles: return "directory-get-files";
        case MethodCoreOperationKind::DirectoryGetDirectoryCount: return "directory-get-directory-count";
        case MethodCoreOperationKind::DirectoryGetDirectories: return "directory-get-directories";
        case MethodCoreOperationKind::ThreadCreate: return "thread-create";
        case MethodCoreOperationKind::ThreadJoin: return "thread-join";
        case MethodCoreOperationKind::ThreadDetach: return "thread-detach";
        case MethodCoreOperationKind::AtomicLoadInt32: return "atomic-load-int32";
        case MethodCoreOperationKind::AtomicStoreInt32: return "atomic-store-int32";
        case MethodCoreOperationKind::InterlockedIncrementInt32: return "interlocked-increment-int32";
        case MethodCoreOperationKind::InterlockedDecrementInt32: return "interlocked-decrement-int32";
        case MethodCoreOperationKind::InterlockedAddInt32: return "interlocked-add-int32";
        case MethodCoreOperationKind::InterlockedExchangeInt32: return "interlocked-exchange-int32";
        case MethodCoreOperationKind::InterlockedCompareExchangeInt32: return "interlocked-compare-exchange-int32";
        case MethodCoreOperationKind::AtomicLoadInt64: return "atomic-load-int64";
        case MethodCoreOperationKind::AtomicStoreInt64: return "atomic-store-int64";
        case MethodCoreOperationKind::InterlockedIncrementInt64: return "interlocked-increment-int64";
        case MethodCoreOperationKind::InterlockedDecrementInt64: return "interlocked-decrement-int64";
        case MethodCoreOperationKind::InterlockedAddInt64: return "interlocked-add-int64";
        case MethodCoreOperationKind::InterlockedExchangeInt64: return "interlocked-exchange-int64";
        case MethodCoreOperationKind::InterlockedCompareExchangeInt64: return "interlocked-compare-exchange-int64";
        case MethodCoreOperationKind::AtomicLoadPointer: return "atomic-load-pointer";
        case MethodCoreOperationKind::AtomicStorePointer: return "atomic-store-pointer";
        case MethodCoreOperationKind::InterlockedExchangePointer: return "interlocked-exchange-pointer";
        case MethodCoreOperationKind::InterlockedCompareExchangePointer: return "interlocked-compare-exchange-pointer";
        case MethodCoreOperationKind::AtomicThreadFence: return "atomic-thread-fence";
        case MethodCoreOperationKind::AtomicCompilerFence: return "atomic-compiler-fence";
        case MethodCoreOperationKind::SimdFloat32x4Add: return "simd-float32x4-add";
        case MethodCoreOperationKind::SimdFloat32x4Subtract: return "simd-float32x4-subtract";
        case MethodCoreOperationKind::SimdFloat32x4Multiply: return "simd-float32x4-multiply";
        case MethodCoreOperationKind::SimdInt128Add: return "simd-int128-add";
        case MethodCoreOperationKind::SimdInt128Subtract: return "simd-int128-subtract";
        case MethodCoreOperationKind::SimdInt128Multiply: return "simd-int128-multiply";
        case MethodCoreOperationKind::SimdFloat256Add: return "simd-float256-add";
        case MethodCoreOperationKind::SimdFloat256Subtract: return "simd-float256-subtract";
        case MethodCoreOperationKind::SimdFloat256Multiply: return "simd-float256-multiply";
        case MethodCoreOperationKind::SimdInt256Add: return "simd-int256-add";
        case MethodCoreOperationKind::SimdInt256Subtract: return "simd-int256-subtract";
        case MethodCoreOperationKind::SimdInt256Multiply: return "simd-int256-multiply";
        case MethodCoreOperationKind::SimdLoadAligned128: return "simd-load-aligned-128";
        case MethodCoreOperationKind::SimdStoreAligned128: return "simd-store-aligned-128";
        case MethodCoreOperationKind::SimdLoadUnaligned128: return "simd-load-unaligned-128";
        case MethodCoreOperationKind::SimdStoreUnaligned128: return "simd-store-unaligned-128";
        case MethodCoreOperationKind::HalfFromFloat: return "half-from-float";
        case MethodCoreOperationKind::HalfToFloat: return "half-to-float";
        case MethodCoreOperationKind::SimdSwizzleWzyx: return "simd-swizzle-wzyx";
        case MethodCoreOperationKind::SimdShuffleLowHigh: return "simd-shuffle-low-high";
        case MethodCoreOperationKind::SimdSqrt: return "simd-sqrt";
        case MethodCoreOperationKind::SimdReciprocalSqrt: return "simd-reciprocal-sqrt";
        case MethodCoreOperationKind::SimdFusedMultiplyAdd: return "simd-fused-multiply-add";
        case MethodCoreOperationKind::BitPopulationCount: return "bit-popcount";
        case MethodCoreOperationKind::BitLeadingZeroCount: return "bit-clz";
        case MethodCoreOperationKind::BitTrailingZeroCount: return "bit-ctz";
        case MethodCoreOperationKind::BitRotateLeft: return "bit-rotate-left";
        case MethodCoreOperationKind::BitRotateRight: return "bit-rotate-right";
        case MethodCoreOperationKind::BitByteSwap: return "bit-byte-swap";
        case MethodCoreOperationKind::CpuPrefetchRead: return "cpu-prefetch-read";
        case MethodCoreOperationKind::CpuPause: return "cpu-pause";
        case MethodCoreOperationKind::CpuReadTimestamp: return "cpu-read-timestamp";
        case MethodCoreOperationKind::MutexCreate: return "mutex-create";
        case MethodCoreOperationKind::MutexCreateNamed: return "mutex-create-named";
        case MethodCoreOperationKind::MutexLock: return "mutex-lock";
        case MethodCoreOperationKind::MutexUnlock: return "mutex-unlock";
        case MethodCoreOperationKind::MutexClose: return "mutex-close";
        case MethodCoreOperationKind::MonitorEnter: return "monitor-enter";
        case MethodCoreOperationKind::MonitorExit: return "monitor-exit";
        case MethodCoreOperationKind::MonitorWait: return "monitor-wait";
        case MethodCoreOperationKind::MonitorPulse: return "monitor-pulse";
        case MethodCoreOperationKind::MonitorPulseAll: return "monitor-pulse-all";
        case MethodCoreOperationKind::SemaphoreCreate: return "semaphore-create";
        case MethodCoreOperationKind::SemaphoreCreateNamed: return "semaphore-create-named";
        case MethodCoreOperationKind::SemaphoreWait: return "semaphore-wait";
        case MethodCoreOperationKind::SemaphoreRelease: return "semaphore-release";
        case MethodCoreOperationKind::SemaphoreClose: return "semaphore-close";
        case MethodCoreOperationKind::EventCreate: return "event-create";
        case MethodCoreOperationKind::EventCreateNamed: return "event-create-named";
        case MethodCoreOperationKind::EventSet: return "event-set";
        case MethodCoreOperationKind::EventReset: return "event-reset";
        case MethodCoreOperationKind::EventWait: return "event-wait";
        case MethodCoreOperationKind::EventClose: return "event-close";
        case MethodCoreOperationKind::ProcessGetCurrentProcessIdRaw: return "process-get-current-process-id";
        case MethodCoreOperationKind::ProcessGetWorkingSetSizeRaw: return "process-get-working-set-size";
        case MethodCoreOperationKind::ProcessGetKernelTimeRaw: return "process-get-kernel-time";
        case MethodCoreOperationKind::ProcessGetUserTimeRaw: return "process-get-user-time";
        case MethodCoreOperationKind::ProcessGetPriorityClassRaw: return "process-get-priority-class";
        case MethodCoreOperationKind::ProcessSetPriorityClassRaw: return "process-set-priority-class";
        case MethodCoreOperationKind::SystemInfoGetPageSizeRaw: return "system-info-get-page-size";
        case MethodCoreOperationKind::SystemInfoGetProcessorArchitectureRaw: return "system-info-get-processor-architecture";
        case MethodCoreOperationKind::SystemInfoGetAllocationGranularityRaw: return "system-info-get-allocation-granularity";
        case MethodCoreOperationKind::SystemInfoGetTotalPhysicalMemoryBytesRaw: return "system-info-get-total-physical-memory-bytes";
        case MethodCoreOperationKind::SystemInfoGetAvailablePhysicalMemoryBytesRaw: return "system-info-get-available-physical-memory-bytes";
        case MethodCoreOperationKind::SystemInfoGetMemoryLoadPercentRaw: return "system-info-get-memory-load-percent";
        case MethodCoreOperationKind::DebugIsDebuggerPresentRaw: return "debug-is-debugger-present";
        case MethodCoreOperationKind::DebugCaptureStackFrameCountRaw: return "debug-capture-stack-frame-count";
        case MethodCoreOperationKind::DebugOutputStringRaw: return "debug-output-string";
        case MethodCoreOperationKind::ThreadSleepRaw: return "thread-sleep";
        case MethodCoreOperationKind::FileCopyRaw: return "file-copy";
        case MethodCoreOperationKind::FileMoveRaw: return "file-move";
        case MethodCoreOperationKind::DirectoryMoveRaw: return "directory-move";
    }
    return "array-allocation";
}

const char* methodLambdaCaptureKindName(MethodLambdaCaptureKind kind) noexcept
{
    switch (kind)
    {
        case MethodLambdaCaptureKind::Local: return "local";
        case MethodLambdaCaptureKind::Parameter: return "parameter";
        case MethodLambdaCaptureKind::ThisReference: return "this";
    }
    return "unknown";
}

const char* methodLambdaCaptureSourceKindName(MethodLambdaCaptureSourceKind kind) noexcept
{
    switch (kind)
    {
        case MethodLambdaCaptureSourceKind::OriginalLocal: return "original-local";
        case MethodLambdaCaptureSourceKind::OriginalParameter: return "original-parameter";
        case MethodLambdaCaptureSourceKind::OriginalThis: return "original-this";
        case MethodLambdaCaptureSourceKind::ParentLocal: return "parent-local";
        case MethodLambdaCaptureSourceKind::ParentParameter: return "parent-parameter";
        case MethodLambdaCaptureSourceKind::ParentCapture: return "parent-capture";
    }
    return "unknown";
}

const char* methodDelegateOperationKindName(MethodDelegateOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodDelegateOperationKind::Create: return "create";
        case MethodDelegateOperationKind::Invoke: return "invoke";
    }
    return "create";
}

const char* methodEventOperationKindName(MethodEventOperationKind kind) noexcept
{
    switch (kind)
    {
        case MethodEventOperationKind::Add: return "add";
        case MethodEventOperationKind::Remove: return "remove";
        case MethodEventOperationKind::Invoke: return "invoke";
    }
    return "add";
}
}
