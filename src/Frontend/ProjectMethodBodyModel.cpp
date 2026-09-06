#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
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
    Boolean
};

struct Stage37ConstantValue final
{
    Stage37ConstantKind kind = Stage37ConstantKind::Integer;
    std::string typeKey;
    std::uint64_t integer = 0;
    double floating = 0.0;
    bool boolean = false;
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
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    diagnostics::DiagnosticEngine& diagnostics,
    const ProjectGenericConstraintModel* genericConstraintModel)
{
    genericConstraintModel_ = genericConstraintModel;
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
    inheritanceModel_ = &inheritanceModel;
    interfaceModel_ = &interfaceModel;

    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
        if ((member.kind != ClassMemberKind::Method && member.kind != ClassMemberKind::Constructor &&
             member.kind != ClassMemberKind::Destructor) ||
            member.syntax == nullptr || member.syntax->first() == nullptr)
        {
            continue;
        }
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
        if (genericConstraintModel_ != nullptr &&
            (!genericConstraintModel_->specializationSatisfiesConstraints(member.syntax) ||
             (member.declaringTypeOrdinal < nameLookup.namedTypes().size() &&
              !genericConstraintModel_->specializationSatisfiesConstraints(
                  nameLookup.namedTypes()[member.declaringTypeOrdinal].syntax))))
        {
            continue;
        }

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
            field.declaredTypeKey != "builtin:double")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                field.syntax->range(),
                "Stage 37 const fields are limited to integral, bool, float, and double types");
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
        if (member.kind != ClassMemberKind::Field || member.isStatic || member.isConst) continue;
        elements.push_back(member.declaredTypeKey);
        if (isUnion) break;
    }
    return elements;
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
    if (syntax->children().size() > elements->size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "aggregate initializer for '" + targetTypeKey + "' has " +
                std::to_string(syntax->children().size()) + " values but the type takes " +
                std::to_string(elements->size()));
        return result;
    }

    // Fewer values than elements is legal and means exactly what it means in C: everything the
    // list does not reach keeps the zero the backend already wrote. `{}` is therefore the
    // explicit spelling of "all zero".
    bool valid = true;
    for (std::size_t index = 0; index < syntax->children().size(); ++index)
    {
        const ast::AstNode* element = syntax->children()[index];
        const std::string& elementTypeKey = (*elements)[index];
        if (element->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            const ExpressionResult nested = bindAggregateInitializer(
                element, elementTypeKey, methodMemberOrdinal, scopeOrdinal, diagnostics);
            valid = valid && nested.valid;
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
                "cannot initialize element " + std::to_string(index) + " of '" + targetTypeKey +
                    "' of type '" + elementTypeKey + "' from '" + value.typeKey + "'");
            valid = false;
        }
    }

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

std::span<const MethodRuntimeTypeBinding> ProjectMethodBodyModel::runtimeTypeBindings() const noexcept
{
    return runtimeTypeBindings_;
}

std::span<const MethodCoreBinding> ProjectMethodBodyModel::coreBindings() const noexcept
{
    return coreBindings_;
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

std::span<const MethodCallBinding> ProjectMethodBodyModel::calls() const noexcept
{
    return calls_;
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
        case ast::AstNodeKind::ExpressionStatement:
            analyzeExpression(statement->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
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

    const std::string typeKey = isVarInferred
        ? varInferredTypeKey
        : memberModel_->canonicalTypeKey(variable->typeSyntax(), *nameLookup_);
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
    const bool ownedQualifierOwnsStorage =
        isOwnedQualified && (isReferenceTypeKey(typeKey) || typeKey == "builtin:string");
    const bool isRaii = variable->hasModifier(TokenKind::KwRaii) || ownedQualifierOwnsStorage;
    // Wording helper: every owner diagnostic below has to name the spelling the user actually
    // wrote, otherwise an `owned` local reports errors about "raii local" and sends them looking at
    // the wrong keyword.
    const char* const ownerSpelling = ownedQualifierOwnsStorage ? "owned local" : "raii local";
    std::optional<MethodLocalBorrowBinding> pendingLocalBorrow;
    if (isRaii && !isReferenceTypeKey(typeKey) && typeKey != "builtin:string")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            variable->range(),
            std::string(ownerSpelling) +
                " requires a NativeSharp class/interface reference type or native string");
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
        const bool localTypeOwnable = isReferenceTypeKey(typeKey) || typeKey == "builtin:string";
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
            variable->first()->kind() != ast::AstNodeKind::MoveExpression &&
            variable->first()->kind() != ast::AstNodeKind::NullLiteralExpression &&
            !ownedReturningInitializer)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                variable->first()->range(),
                "RAII ownership cannot be copied or borrowed; initialize with new, null, move(other), or an owned-returning call");
        }
        if (initializer.valid)
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
    local.ownership = isBorrowed ? sema::OwnershipKind::Borrowed : sema::OwnershipKind::None;
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
    scopes_[scopeOrdinal].localOrdinals.push_back(localOrdinal);
    scopeNames_[scopeOrdinal].push_back(ScopeName{
        ScopeNameKind::Local,
        variable->text(),
        localOrdinal,
        variable->range()});
    // C7 "definite assignment": a declaration with an initializer assigns the local, full stop.
    // Without one, only the types the analysis does not track (aggregates, which the backend
    // zeroes) start out assigned; everything else must be written before it is read. A local
    // re-entering scope on a later loop iteration is re-declared through this same path, so its
    // assigned state is correctly reset rather than inherited from the previous iteration.
    if (variable->first() != nullptr || !definiteAssignmentTracksLocal(localOrdinal))
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
        analyzeExpression(statement->third(), methodMemberOrdinal, forScope, diagnostics);
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
    const bool returnTypeOwnable =
        isReferenceTypeKey(method->declaredTypeKey) || method->declaredTypeKey == "builtin:string";

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
            freshOwnedObject || nullOwnedValue;
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
    bool allowMove)
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
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup, allowEvent);
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
                        ProjectMemberModel::fixedArrayElementTypeKey(elementBaseTypeKey).has_value();
                }
                if (!acceptableElementSource)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "move(...) array-element source must be a direct native array or fixed array element access");
                    break;
                }
                MethodMoveBinding move;
                move.syntax = expression;
                move.methodMemberOrdinal = methodMemberOrdinal;
                move.scopeOrdinal = scopeOrdinal;
                move.typeKey = element.typeKey;
                move.sourceIsArrayElement = true;
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
            const auto tryOwnedFieldMoveSource = [&](const ExpressionResult& fieldSource) -> bool
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
                const ExpressionResult member = analyzeExpression(
                    expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
                if (tryOwnedFieldMoveSource(member)) break;
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
                if (tryOwnedFieldMoveSource(implicitField)) break;
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
                    isReferenceTypeKey(local.typeKey) || local.typeKey == "builtin:string";
                moveBindings_.push_back(std::move(move));
                maybeMovedRaiiLocals_.erase(source->semanticOrdinal);
                movedRaiiLocals_.insert(source->semanticOrdinal);
                invalidateHistoricalBorrowsOfOwnerLocal(source->semanticOrdinal);
                result.typeKey = local.typeKey;
                result.ownershipSourceLocalOrdinal = source->semanticOrdinal;
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
                    isReferenceTypeKey(parameter.typeKey) || parameter.typeKey == "builtin:string";
                moveBindings_.push_back(std::move(move));
                maybeMovedOwnedParameters_.erase(source->semanticOrdinal);
                movedOwnedParameters_.insert(source->semanticOrdinal);
                invalidateHistoricalBorrowsOfOwnedParameter(source->semanticOrdinal);
                result.typeKey = parameter.typeKey;
                result.ownershipSourceParameterOrdinal = source->semanticOrdinal;
            }

            result.valid = true;
            result.assignable = false;
            result.ownership = sema::OwnershipKind::Owned;
            result.ownershipTransfer = true;
            break;
        }
        case ast::AstNodeKind::MemberExpression:
            result = analyzeMember(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup, allowEvent);
            break;
        // IDs 80 y 83. `p->campo` y `p->Metodo()` ya no llegan aqui: el parser los convierte en
        // `(*p).campo` y `(*p).Metodo()`, que es la MISMA operacion y la que siempre funciono.
        // Ver Parser::parsePostfixExpression. El caso se queda -- sin rechazo -- porque el nodo
        // sigue existiendo en el AST y un `default` mudo seria peor que una ruta explicita.
        case ast::AstNodeKind::PointerMemberExpression:
            result = analyzeMember(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup, allowEvent);
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
                if (indices.size() == 1 && indices.front().valid &&
                    ProjectTypeConversion::isIntegral(indices.front().typeKey))
                {
                    const std::optional<std::string> element =
                        ProjectTypeConversion::nativeArrayElementType(base.typeKey);
                    if (element.has_value())
                    {
                        result.typeKey = *element;
                        result.valid = true;
                        result.assignable = true;
                        coreBindings_.push_back(MethodCoreBinding{
                            MethodCoreOperationKind::ArrayElement,
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
                        "native array indexing requires exactly one integral index");
                }
                break;
            }

            if (base.valid &&
                (ProjectTypeConversion::isNativeString(base.typeKey) ||
                 base.typeKey == "string-literal"))
            {
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
                        diagnostics);
                    const std::vector<std::size_t> setters = stage38AccessorCandidates(
                        *receiverType, "set_Item", false, false, false,
                        accessingTypeOrdinal, *receiverType);
                    result.assignable = result.valid && !setters.empty();
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
            const bool rawPointerBase = ProjectTypeConversion::isPointer(base.typeKey);
            if (base.valid && indices.size() == 1 && indices.front().valid &&
                ProjectTypeConversion::isIntegral(indices.front().typeKey) &&
                (fixedElementType.has_value() || rawPointerBase))
            {
                result.typeKey = fixedElementType.has_value()
                    ? *fixedElementType
                    : std::string(base.typeKey.substr(0, base.typeKey.size() - 1));
                result.valid = true;
                result.assignable = true;
                MethodCoreBinding binding;
                binding.kind = MethodCoreOperationKind::RawElement;
                binding.syntax = expression;
                binding.methodMemberOrdinal = methodMemberOrdinal;
                binding.scopeOrdinal = scopeOrdinal;
                binding.resultTypeKey = result.typeKey;
                binding.elementTypeKey = result.typeKey;
                binding.assignable = true;
                coreBindings_.push_back(std::move(binding));
            }
            else if (base.valid)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidIndex,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "index expression requires an indexer reference or a pointer with one integral index");
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
        {
            std::string measuredTypeKey;
            if (expression->kind() == ast::AstNodeKind::SizeofExpression)
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
                        "sizeof requires a value or a type; a method group has no size");
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
                    "sizeof(void) has no size");
                break;
            }

            MethodCoreBinding binding;
            binding.kind = MethodCoreOperationKind::SizeofConstant;
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
                    const bool countValid = count.valid &&
                        ProjectTypeConversion::isIntegral(count.typeKey);
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
                    result.typeKey = element + "[]";
                    result.valid = !isErrorType(element) && element != "builtin:void" && countValid;
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
                const bool classNew = expression->kind() == ast::AstNodeKind::NewExpression &&
                    declared != nullptr && declared->kind == NamedTypeDeclarationKind::Class;
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
    bool allowEvent)
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
                const bool capturedOuterLocal = currentLambdaOrdinal_.has_value() &&
                    (!local.lambdaOrdinal.has_value() || *local.lambdaOrdinal != *currentLambdaOrdinal_);
                result.assignable = !local.isConst && !capturedOuterLocal &&
                    local.ownership != sema::OwnershipKind::Borrowed;
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
            if (parameter.ownership != sema::OwnershipKind::None)
            {
                result.ownershipSourceParameterOrdinal = scopeName->semanticOrdinal;
            }
            // `in` is read-only by definition: it exists to avoid a copy, not to give the callee
            // a channel back to the caller. `ref`/`out` are assignable, and for `out` that is not
            // a permission but an obligation -- see the definite-assignment check on returns.
            result.assignable = !currentLambdaOrdinal_.has_value() &&
                parameter.passKind != ParameterPassKind::In;
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
        result.typeKey = field.declaredTypeKey;
        result.valid = true;
        result.assignable = !field.isConst &&
            (!field.isReadonly || readonlyFieldAssignable(field, methodMemberOrdinal));
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
                diagnostics);
            result.assignable = !propertySetters.empty();
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

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeMember(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent)
{
    ExpressionResult result;
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
        result.assignable = !field.isConst && (!field.isReadonly || readonlyOnThis);
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
                diagnostics);
            result.assignable = !propertySetters.empty();
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
         expression->text() == "GetHashCode"))
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

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
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

    std::vector<std::string> lambdaTargetTypes(expression->children().size());
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

    if (callee.eventMemberOrdinal.has_value() && memberModel_ != nullptr &&
        *callee.eventMemberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& event = memberModel_->members()[*callee.eventMemberOrdinal];
        std::string ignoredReturnType;
        std::vector<std::string> parameterTypes;
        std::size_t ignoredDelegateOrdinal = 0;
        if (delegateSignature(event.declaredTypeKey, ignoredReturnType, parameterTypes, ignoredDelegateOrdinal))
            seedLambdaTargetsFromSignature(parameterTypes);
    }
    else if (!callee.methodGroup && isDelegateTypeKey(callee.typeKey))
    {
        std::string ignoredReturnType;
        std::vector<std::string> parameterTypes;
        std::size_t ignoredDelegateOrdinal = 0;
        if (delegateSignature(callee.typeKey, ignoredReturnType, parameterTypes, ignoredDelegateOrdinal))
            seedLambdaTargetsFromSignature(parameterTypes);
    }
    else if (callee.methodGroup && memberModel_ != nullptr)
    {
        std::vector<const ClassMemberSymbol*> arityMatches;
        for (const std::size_t candidateOrdinal : callee.methodCandidates)
        {
            if (candidateOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
            if (candidate.kind == ClassMemberKind::Method &&
                candidate.parameters.size() == expression->children().size())
            {
                arityMatches.push_back(&candidate);
            }
        }
        if (!arityMatches.empty())
        {
            for (std::size_t index = 0; index < expression->children().size(); ++index)
            {
                if (expression->children()[index]->kind() != ast::AstNodeKind::LambdaExpression) continue;
                const std::string& firstType = arityMatches.front()->parameters[index].typeKey;
                const bool agreed = std::ranges::all_of(arityMatches, [&](const ClassMemberSymbol* candidate)
                {
                    return candidate->parameters[index].typeKey == firstType;
                });
                if (agreed && isDelegateTypeKey(firstType)) lambdaTargetTypes[index] = firstType;
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

    bool hasMatchingArity = false;
    // Las candidatas que eran viables por tipos y propiedad y cayeron SOLO por su `where`. Si al
    // final no queda ninguna viable, estas son las que explican por que.
    std::vector<const ast::AstNode*> constraintRejectedSyntaxes;
    if (memberModel_ != nullptr)
    {
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
            if (candidate.parameters.size() != resolvedArguments.size())
            {
                call.evaluatedCandidates.push_back(std::move(candidateBinding));
                continue;
            }

            hasMatchingArity = true;
            candidateBinding.viable = true;
            for (std::size_t argumentOrdinal = 0; argumentOrdinal < resolvedArguments.size(); ++argumentOrdinal)
            {
                ProjectImplicitConversion conversion = classifyImplicit(
                    resolvedArguments[argumentOrdinal].typeKey,
                    candidate.parameters[argumentOrdinal].typeKey,
                    resolvedArguments[argumentOrdinal].integerConstant);
                if (!conversion.exists())
                {
                    candidateBinding.viable = false;
                }
                else
                {
                    const sema::OwnershipKind parameterOwnership =
                        candidate.parameters[argumentOrdinal].ownership;
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
                        typeKeyIsOwnable(candidate.parameters[argumentOrdinal].typeKey);
                    // NSC-LIMIT-010: la regla vive ahora en classifyOwnershipArgument, arriba de
                    // este fichero, y la comparte ProjectConstructionModel. Lo que sigue a este
                    // bloque es el comentario historico de Stage 52 que explica POR QUE el orden
                    // de las comprobaciones es el que es; se conserva porque esa razon no esta en
                    // ningun otro sitio.
                    if (classifyOwnershipArgument(
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
        bool reportedOwnershipDiagnostic = false;
        if (memberModel_ != nullptr)
        {
            for (const std::size_t candidateOrdinal : callee.methodCandidates)
            {
                if (candidateOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                if (candidate.kind != ClassMemberKind::Method ||
                    candidate.parameters.size() != resolvedArguments.size()) continue;
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
    for (const std::size_t candidateIndex : viableEvaluationOrdinals)
    {
        bool dominated = false;
        for (const std::size_t otherIndex : viableEvaluationOrdinals)
        {
            if (candidateIndex == otherIndex)
            {
                continue;
            }
            if (betterCandidate(
                    call.evaluatedCandidates[otherIndex],
                    call.evaluatedCandidates[candidateIndex]))
            {
                dominated = true;
                break;
            }
        }
        if (!dominated)
        {
            nonDominated.push_back(candidateIndex);
        }
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
    checkArgumentPassKinds(
        expression, call.methodName, selected.memberOrdinal, arguments, diagnostics);
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedMemberOrdinal = selected.memberOrdinal;
    call.selectedConversions = selected.conversions;
    if (memberModel_ != nullptr && selected.memberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
        call.resultTypeKey = selectedMember.declaredTypeKey;
        result.ownership = selectedMember.declaredOwnership;
        result.ownershipTransfer = selectedMember.declaredOwnership == sema::OwnershipKind::Owned;
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
             argumentOrdinal < selectedMember.parameters.size(); ++argumentOrdinal)
        {
            if (selectedMember.parameters[argumentOrdinal].passKind != ParameterPassKind::Out ||
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
             argumentOrdinal < selectedMember.parameters.size(); ++argumentOrdinal)
        {
            if (selectedMember.parameters[argumentOrdinal].ownership != sema::OwnershipKind::Owned)
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
            transfer.targetParameterOrdinal = argumentOrdinal;
            transfer.typeKey = selectedMember.parameters[argumentOrdinal].typeKey;
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
             argumentOrdinal < selectedMember.parameters.size(); ++argumentOrdinal)
        {
            if (selectedMember.parameters[argumentOrdinal].ownership != sema::OwnershipKind::Borrowed)
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
            borrow.targetParameterOrdinal = argumentOrdinal;
            borrow.typeKey = selectedMember.parameters[argumentOrdinal].typeKey;
            if (resolvedArguments[argumentOrdinal].borrowedLocalOrdinal.has_value())
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
            // Stage 54.2 "Atomics" -- NativeSharp.Core.Threading.AtomicInt32/AtomicInt64. Diez
            // metodos DE INSTANCIA (a diferencia del diseno original de esta ronda, que planeaba
            // metodos estaticos con un parametro `int*`/`long*` calcado de C#'s
            // System.Threading.Interlocked -- descartado durante la implementacion al confirmar
            // que la tuberia OOP de este compilador NO baja los operadores `&`/`*` en absoluto,
            // ver ProjectOopHirModel.cpp's UnaryExpression case: "Stage 34 OOP lowering keeps raw
            // pointer address/dereference in the procedural backend boundary" -- un limite
            // deliberado y duro, no un bug, que hace inviable cualquier API que le exija al
            // codigo NativeSharp escribir `&algo`). El nuevo diseno evita esa operacion por
            // completo: cada metodo opera implicitamente sobre el campo privado `value_` DEL
            // RECEPTOR (`contador.Increment()`, nunca `Interlocked.Increment(&contador)`) --
            // mismo espiritu que java.util.concurrent.atomic.AtomicInteger/AtomicLong, el
            // precedente real mas cercano para un lenguaje sin `ref`/punteros expuestos al
            // programador para este tipo de API. Cinco operaciones (Increment/Decrement/Add/
            // Exchange/CompareExchange) x dos anchos (Int32/Int64, dos clases separadas en vez de
            // un solo parametro de ancho variable). Reconocidas por clase+nombre+firma exacta,
            // igual que siempre; la unica diferencia real es que ADEMAS de fijar el
            // MethodCoreOperationKind, este bloque resuelve el ordinal del campo `value_` DENTRO
            // del tipo declarante (buscandolo por nombre en memberModel_->members()) y lo guarda
            // en MethodCoreBinding::implicitFieldMemberOrdinal -- HIR lowering
            // (ProjectOopHirModel.cpp) lo usa para construir el nodo FieldAddress sintetico que
            // reemplaza el operando `&value_` que el usuario NUNCA escribe.
            if ((declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.AtomicInt32" ||
                    declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.AtomicInt64") &&
                !selectedMember.isStatic)
            {
                const bool isInt32 =
                    declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.AtomicInt32";
                const std::string valueTypeKey = isInt32 ? "builtin:int" : "builtin:long";
                std::optional<MethodCoreOperationKind> atomicKind;
                if (selectedMember.name == "Increment" && selectedMember.parameters.empty() &&
                    selectedMember.declaredTypeKey == valueTypeKey)
                {
                    atomicKind = isInt32
                        ? MethodCoreOperationKind::InterlockedIncrementInt32
                        : MethodCoreOperationKind::InterlockedIncrementInt64;
                }
                else if (selectedMember.name == "Decrement" && selectedMember.parameters.empty() &&
                    selectedMember.declaredTypeKey == valueTypeKey)
                {
                    atomicKind = isInt32
                        ? MethodCoreOperationKind::InterlockedDecrementInt32
                        : MethodCoreOperationKind::InterlockedDecrementInt64;
                }
                else if (selectedMember.name == "Add" && selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == valueTypeKey &&
                    selectedMember.declaredTypeKey == valueTypeKey)
                {
                    atomicKind = isInt32
                        ? MethodCoreOperationKind::InterlockedAddInt32
                        : MethodCoreOperationKind::InterlockedAddInt64;
                }
                else if (selectedMember.name == "Exchange" && selectedMember.parameters.size() == 1 &&
                    selectedMember.parameters[0].typeKey == valueTypeKey &&
                    selectedMember.declaredTypeKey == valueTypeKey)
                {
                    atomicKind = isInt32
                        ? MethodCoreOperationKind::InterlockedExchangeInt32
                        : MethodCoreOperationKind::InterlockedExchangeInt64;
                }
                else if (selectedMember.name == "CompareExchange" && selectedMember.parameters.size() == 2 &&
                    selectedMember.parameters[0].typeKey == valueTypeKey &&
                    selectedMember.parameters[1].typeKey == valueTypeKey &&
                    selectedMember.declaredTypeKey == valueTypeKey)
                {
                    atomicKind = isInt32
                        ? MethodCoreOperationKind::InterlockedCompareExchangeInt32
                        : MethodCoreOperationKind::InterlockedCompareExchangeInt64;
                }
                if (atomicKind.has_value() && memberModel_ != nullptr)
                {
                    std::optional<std::size_t> valueFieldOrdinal;
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
                    if (valueFieldOrdinal.has_value())
                    {
                        MethodCoreBinding binding{
                            *atomicKind,
                            expression,
                            methodMemberOrdinal,
                            scopeOrdinal,
                            selectedMember.declaredTypeKey,
                            valueTypeKey,
                            false, std::nullopt, std::nullopt};
                        binding.implicitFieldMemberOrdinal = valueFieldOrdinal;
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
    bool diagnoseFailure)
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
        if (!operand.assignable)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidAddressOf,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "operand of & must be an addressable local, parameter or field");
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
            expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics, false, true);
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
    if (!raiiTargetLocal.has_value() && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IndexExpression &&
        left.valid && left.assignable)
    {
        const std::string receiverTypeKey = expression->first()->first() != nullptr
            ? analyzeExpression(
                  expression->first()->first(), methodMemberOrdinal, scopeOrdinal, diagnostics)
                  .typeKey
            : "<error>";
        // SHR-005: y el ARREGLO FIJO, por la misma razon y con la misma forma. La fuente
        // `move(buf[i])` se admite unas mil lineas mas arriba con esta misma condicion doble; si
        // solo se admitiera de un lado, `dst[i] = move(src[j])` compilaria en una direccion y no
        // en la otra. La admision de destino es simetrica a la de fuente a proposito.
        arrayElementMoveTarget = ProjectTypeConversion::isNativeArray(receiverTypeKey) ||
            ProjectMemberModel::fixedArrayElementTypeKey(receiverTypeKey).has_value();
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
    if (ownedFieldOwnsStorage)
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
                "assignment to owned field '" + ownedFieldName +
                    "' requires an ownership-transfer expression such as move(owner), new T(...), "
                    "null, or an owned-returning call");
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
        std::vector<std::size_t> candidates = memberModel_->findMembers(*current, name);
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
                            if (signedLeft == minimum && signedRight == -1) return std::nullopt;
                            value = static_cast<std::uint64_t>(operation == TokenKind::Slash
                                ? signedLeft / signedRight
                                : signedLeft % signedRight);
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
    const auto addCapture = [&](MethodLambdaCapture capture) -> std::size_t
    {
        const std::optional<std::size_t> existing = lambdaCaptureOrdinal(
            binding, capture.kind, capture.localOrdinal, capture.parameterOrdinal);
        if (existing.has_value()) return *existing;
        const std::size_t ordinal = binding.captures.size();
        binding.captures.push_back(std::move(capture));
        return ordinal;
    };

    for (std::size_t index = nameBindingBegin; index < nameBindings_.size(); ++index)
    {
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
                        name.syntax, binding.lambdaOrdinal, true, *local.lambdaParameterOrdinal, local.typeKey});
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
            const std::size_t capture = addCapture(MethodLambdaCapture{
                MethodLambdaCaptureKind::Local, local.name, local.typeKey, *name.localOrdinal, std::nullopt});
            lambdaValueBindings_.push_back(MethodLambdaValueBinding{
                name.syntax, binding.lambdaOrdinal, false, capture, local.typeKey});
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
            const std::size_t capture = addCapture(MethodLambdaCapture{
                MethodLambdaCaptureKind::Parameter, parameter.name, parameter.typeKey,
                std::nullopt, *name.parameterOrdinal});
            lambdaValueBindings_.push_back(MethodLambdaValueBinding{
                name.syntax, binding.lambdaOrdinal, false, capture, parameter.typeKey});
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
            const std::size_t capture = addCapture(MethodLambdaCapture{
                MethodLambdaCaptureKind::ThisReference, "this", typeKey, std::nullopt, std::nullopt});
            if (name.kind == MethodNameBindingKind::ThisReference || name.kind == MethodNameBindingKind::BaseReference)
            {
                lambdaValueBindings_.push_back(MethodLambdaValueBinding{
                    name.syntax, binding.lambdaOrdinal, false, capture, typeKey});
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
    // Interop Foundation: a lambda always closes over an environment cell, and an extern C
    // delegate is a bare C function pointer with no environment channel -- the two shapes are
    // incompatible by construction. Bind a named static method instead.
    {
        const std::optional<std::size_t> delegateType = delegateTypeOrdinalForTypeKey(delegateTypeKey);
        if (delegateType.has_value() && nameLookup_ != nullptr &&
            *delegateType < nameLookup_->namedTypes().size() &&
            nameLookup_->namedTypes()[*delegateType].syntax != nullptr &&
            nameLookup_->namedTypes()[*delegateType].syntax->hasModifier(lexer::TokenKind::KwExtern))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                syntax != nullptr ? syntax->range() : SourceRange{},
                "extern C delegate cannot bind a lambda; a C function pointer has no closure environment -- use a static method");
            return result;
        }
    }
    if (syntax == nullptr || syntax->kind() != ast::AstNodeKind::LambdaExpression)
        return result;
    if (currentLambdaOrdinal_.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion, diagnostics::DiagnosticSeverity::Error,
            syntax->range(), "nested lambdas are deferred beyond Stage 43");
        return result;
    }

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
    binding.lambdaOrdinal = lambdaBindings_.size();
    binding.ownerMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.delegateTypeKey = std::string(delegateTypeKey);
    binding.delegateTypeOrdinal = delegateOrdinal;
    binding.returnTypeKey = returnType;
    binding.parameterTypeKeys = parameterTypes;
    binding.blockBody = syntax->flag();
    binding.generatedName = "__nsc_lambda_" + std::to_string(methodMemberOrdinal) + "_" +
        std::to_string(binding.lambdaOrdinal);

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
        binding.parameterNames.push_back(name);
        MethodLocalSymbol local;
        local.methodMemberOrdinal = methodMemberOrdinal;
        local.scopeOrdinal = lambdaScope;
        local.declarationOrdinal = scopes_[lambdaScope].localOrdinals.size();
        const ClassMemberSymbol* owner = methodSymbol(methodMemberOrdinal);
        if (owner != nullptr) local.fileId = owner->fileId;
        local.range = parameterSyntax != nullptr ? parameterSyntax->range() : syntax->range();
        local.syntax = parameterSyntax;
        local.name = name;
        local.typeKey = parameterTypes[index];
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
    restoreOwnershipFlow(previousOwnershipFlow);
    if (!collectLambdaCaptures(binding, nameBindingBegin, methodMemberOrdinal, diagnostics))
    {
        bodyValid = false;
    }
    lambdaBindings_.push_back(std::move(binding));

    if (!bodyValid) return result;
    result.typeKey = std::string(delegateTypeKey);
    result.valid = true;
    result.assignable = false;
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
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr || memberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& chosen = memberModel_->members()[memberOrdinal];
    for (std::size_t index = 0; index < arguments.size() && index < chosen.parameters.size(); ++index)
    {
        const ParameterPassKind expected = chosen.parameters[index].passKind;
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
    return isReferenceTypeKey(typeKey) || typeKey == "builtin:string";
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
    if (memberModel_ == nullptr || inheritanceModel_ == nullptr) return true;
    if (!typeKey.starts_with("type:")) return true;

    const std::optional<std::size_t> typeOrdinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
    if (!typeOrdinal.has_value()) return true;
    if (nameLookup_ != nullptr && *typeOrdinal < nameLookup_->namedTypes().size())
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
    if (memberModel_ == nullptr || methodMemberOrdinal >= memberModel_->members().size())
    {
        return nullptr;
    }
    return &memberModel_->members()[methodMemberOrdinal];
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
        case MethodCoreOperationKind::ArrayLength: return "array-length";
        case MethodCoreOperationKind::ArrayElement: return "array-element";
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
        case MethodCoreOperationKind::StructToString: return "struct-to-string";
        case MethodCoreOperationKind::EnumToString: return "enum-to-string";
        case MethodCoreOperationKind::StructEquals: return "struct-equals";
        case MethodCoreOperationKind::StructHashCode: return "struct-hash-code";
        case MethodCoreOperationKind::ObjectTypeName: return "object-type-name";
        case MethodCoreOperationKind::ObjectReferenceHash: return "object-reference-hash";
        case MethodCoreOperationKind::SizeofConstant: return "sizeof-constant";
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
        case MethodCoreOperationKind::InterlockedIncrementInt32: return "interlocked-increment-int32";
        case MethodCoreOperationKind::InterlockedDecrementInt32: return "interlocked-decrement-int32";
        case MethodCoreOperationKind::InterlockedAddInt32: return "interlocked-add-int32";
        case MethodCoreOperationKind::InterlockedExchangeInt32: return "interlocked-exchange-int32";
        case MethodCoreOperationKind::InterlockedCompareExchangeInt32: return "interlocked-compare-exchange-int32";
        case MethodCoreOperationKind::InterlockedIncrementInt64: return "interlocked-increment-int64";
        case MethodCoreOperationKind::InterlockedDecrementInt64: return "interlocked-decrement-int64";
        case MethodCoreOperationKind::InterlockedAddInt64: return "interlocked-add-int64";
        case MethodCoreOperationKind::InterlockedExchangeInt64: return "interlocked-exchange-int64";
        case MethodCoreOperationKind::InterlockedCompareExchangeInt64: return "interlocked-compare-exchange-int64";
        case MethodCoreOperationKind::MutexCreate: return "mutex-create";
        case MethodCoreOperationKind::MutexLock: return "mutex-lock";
        case MethodCoreOperationKind::MutexUnlock: return "mutex-unlock";
        case MethodCoreOperationKind::MutexClose: return "mutex-close";
        case MethodCoreOperationKind::MonitorEnter: return "monitor-enter";
        case MethodCoreOperationKind::MonitorExit: return "monitor-exit";
        case MethodCoreOperationKind::MonitorWait: return "monitor-wait";
        case MethodCoreOperationKind::MonitorPulse: return "monitor-pulse";
        case MethodCoreOperationKind::MonitorPulseAll: return "monitor-pulse-all";
        case MethodCoreOperationKind::SemaphoreCreate: return "semaphore-create";
        case MethodCoreOperationKind::SemaphoreWait: return "semaphore-wait";
        case MethodCoreOperationKind::SemaphoreRelease: return "semaphore-release";
        case MethodCoreOperationKind::SemaphoreClose: return "semaphore-close";
        case MethodCoreOperationKind::EventCreate: return "event-create";
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
