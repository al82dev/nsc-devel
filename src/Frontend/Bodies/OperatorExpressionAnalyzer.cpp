#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "Support/OwnershipArgumentClassification.h"
#include "Support/OperatorClassification.h"
#include "Support/IntegralConstantArithmetic.h"
#include "Support/OwnershipFlowSnapshotQueries.h"


#include "MethodBodyInternals.h"
#include "../Internal/Stage37ConstantValue.h"

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
#include "nativesharp/Frontend/ProjectCallableResultMerger.h"
#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
using lexer::TokenKind;

using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeUnary(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::optional<ExpressionResult> operand = analyzeUnaryOperand(
        expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!operand.has_value()) return {};
    const std::optional<ExpressionResult> overloaded = analyzeOverloadedUnary(
        expression, *operand, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (overloaded.has_value()) return *overloaded;
    const TokenKind operation = expression->tokenKind();
    if (operation == TokenKind::Bang)
        return analyzeLogicalNot(expression, *operand, diagnostics);
    if (operation == TokenKind::Amp)
        return analyzeAddressOf(expression, *operand, diagnostics);
    if (operation == TokenKind::Star)
        return analyzeDereference(expression, *operand, diagnostics);
    if (operation == TokenKind::PlusPlus || operation == TokenKind::MinusMinus)
        return analyzeIncrementOrDecrement(expression, *operand, diagnostics);
    return analyzeUnaryNumeric(expression, *operand, diagnostics);
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

void ProjectMethodBodyModel::analyzeLogicalBinaryStep(
    ExpressionResult& accumulated,
    const ast::AstNode* logical,
    TokenKind operation,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const OwnershipFlowSnapshot afterLeft = captureOwnershipFlow();
    OwnershipFlowSnapshot leftTrue = afterLeft;
    OwnershipFlowSnapshot leftFalse = afterLeft;
    if (const auto flow = conditionFlowBySyntax_.find(logical->first());
        flow != conditionFlowBySyntax_.end())
    {
        leftTrue = flow->second.whenTrue;
        leftFalse = flow->second.whenFalse;
    }
    ExpressionResult right;
    const OwnershipFlowSnapshot& rhsEntry = operation == TokenKind::AmpAmp
        ? leftTrue : leftFalse;
    const OwnershipFlowSnapshot rhsEvaluated = analyzeBranchState(
        rhsEntry,
        [&]
        {
            right = analyzeExpression(
                logical->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        });
    OwnershipFlowSnapshot rightTrue = rhsEvaluated;
    OwnershipFlowSnapshot rightFalse = rhsEvaluated;
    if (const auto flow = conditionFlowBySyntax_.find(logical->second());
        flow != conditionFlowBySyntax_.end())
    {
        rightTrue = flow->second.whenTrue;
        rightFalse = flow->second.whenFalse;
    }
    ConditionFlowStates conditionFlow;
    if (operation == TokenKind::AmpAmp)
    {
        conditionFlow.whenTrue = rightTrue;
        conditionFlow.whenFalse = joinOwnershipFlow(leftFalse, rightFalse);
    }
    else
    {
        conditionFlow.whenTrue = joinOwnershipFlow(leftTrue, rightTrue);
        conditionFlow.whenFalse = rightFalse;
    }
    conditionFlowBySyntax_.insert_or_assign(logical, conditionFlow);
    commitBranchJoin(conditionFlow.whenTrue, conditionFlow.whenFalse);
    if (!accumulated.valid || !right.valid)
    {
        accumulated = {};
        return;
    }
    if (accumulated.typeKey != "builtin:bool" || right.typeKey != "builtin:bool")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            logical->range(),
            "logical operators require bool operands");
        accumulated = {};
        return;
    }
    accumulated = {};
    accumulated.typeKey = "builtin:bool";
    accumulated.valid = true;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeLogicalBinary(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const TokenKind operation = expression->tokenKind();
    // Long left-associative logical chains must not consume one native stack frame per operand.
    // Besides exhausting the Windows stack in O0 host builds, recursive descent is unnecessary:
    // every node in a homogeneous &&/|| left spine has the same ordered short-circuit contract.
    std::vector<const ast::AstNode*> logicalChain;
    const ast::AstNode* leftmost = expression;
    while (leftmost != nullptr && leftmost->kind() == ast::AstNodeKind::BinaryExpression &&
        leftmost->tokenKind() == operation)
    {
        logicalChain.push_back(leftmost);
        leftmost = leftmost->first();
    }
    std::ranges::reverse(logicalChain);
    ExpressionResult accumulated = analyzeExpression(
        leftmost, methodMemberOrdinal, scopeOrdinal, diagnostics);
    for (const ast::AstNode* logical : logicalChain)
    {
        analyzeLogicalBinaryStep(
            accumulated, logical, operation,
            methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    return accumulated;
}

bool ProjectMethodBodyModel::analyzeStringBinary(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    const bool leftStringLike = left.typeKey == "builtin:string" ||
        left.typeKey == "string-literal";
    const bool rightStringLike = right.typeKey == "builtin:string" ||
        right.typeKey == "string-literal";
    // A string compared with the null literal is reference identity, not content equality.
    // Compute this before the string intrinsic branch so both operand orders and both equality
    // operators share the same null-safe path as every other reference type.
    const bool nullLiteralOperand =
        (expression->first() != nullptr &&
         expression->first()->kind() == ast::AstNodeKind::NullLiteralExpression) ||
        (expression->second() != nullptr &&
         expression->second()->kind() == ast::AstNodeKind::NullLiteralExpression);
    if (nullLiteralOperand || !leftStringLike || !rightStringLike) return false;
    if (operation == TokenKind::Plus)
    {
        result.typeKey = "builtin:string";
        result.valid = true;
        result.ownership = sema::OwnershipKind::Owned;
        result.ownershipTransfer = true;
        result.ownershipFreshOwner = true;
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::StringConcat, expression, methodMemberOrdinal,
            scopeOrdinal, result.typeKey, "builtin:string", false,
            std::nullopt, std::nullopt});
        return true;
    }
    if (operation != TokenKind::EqualEqual && operation != TokenKind::BangEqual) return false;
    result.typeKey = "builtin:bool";
    result.valid = true;
    coreBindings_.push_back(MethodCoreBinding{
        MethodCoreOperationKind::StringEquals, expression, methodMemberOrdinal,
        scopeOrdinal, result.typeKey, "builtin:string", false,
        std::nullopt, std::nullopt});
    return true;
}

bool ProjectMethodBodyModel::analyzeOverloadedBinary(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::string operatorName = stage38OperatorMethodName(operation, 2);
    const std::string operatorTraitKey = stage38OperatorTraitKey(operation, 2);
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
    if (operatorName.empty() || operatorTraitKey.empty() || nullLiteralOperand) return false;
    std::vector<std::size_t> candidates = stage38OperatorCandidates(
        operatorTraitKey, operatorName, left.typeKey, right.typeKey);
    if (candidates.empty()) return false;
    const std::vector<ExpressionResult> arguments = {left, right};
    ExpressionResult overloaded = resolveSyntheticCall(
        expression, methodMemberOrdinal, scopeOrdinal, operatorName, std::move(candidates),
        arguments, nullptr, {}, std::nullopt, false, false, diagnostics, true);
    if (overloaded.valid)
    {
        overloaded.assignable = false;
        overloaded.addressable = false;
        result = std::move(overloaded);
    }
    return true;
}

bool ProjectMethodBodyModel::analyzePortableSimdBinary(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // NativeSharp 3 portable vectors are structural value types. Operations never mix lane
    // shapes implicitly: that keeps AVX2 lowering and scalar fallback observably identical.
    const auto leftSimd = language::parsePortableSimdType(left.typeKey);
    const auto rightSimd = language::parsePortableSimdType(right.typeKey);
    if (!leftSimd.has_value() && !rightSimd.has_value()) return false;
    if (!leftSimd.has_value() || !rightSimd.has_value() || left.typeKey != right.typeKey)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "portable SIMD operands must have exactly the same lane type and lane count");
        return true;
    }
    const bool bitwiseOperation = operation == TokenKind::Amp || operation == TokenKind::Pipe ||
        operation == TokenKind::Caret;
    const bool arithmeticOperation = operation == TokenKind::Plus ||
        operation == TokenKind::Minus || operation == TokenKind::Star ||
        operation == TokenKind::Slash || operation == TokenKind::Percent;
    if (isComparison(operation))
    {
        language::PortableSimdType mask = *leftSimd;
        mask.mask = true;
        mask.laneKind = language::SimdLaneKind::Mask;
        result.typeKey = language::portableSimdTypeIdentity(mask);
        result.valid = true;
        return true;
    }
    if ((leftSimd->mask && bitwiseOperation) ||
        (!leftSimd->mask && (arithmeticOperation ||
            (bitwiseOperation && leftSimd->laneKind != language::SimdLaneKind::FloatingPoint))))
    {
        result.typeKey = left.typeKey;
        result.valid = true;
        return true;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "operator is not defined for this portable SIMD lane kind");
    return true;
}

bool ProjectMethodBodyModel::analyzeEnumBinary(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    diagnostics::DiagnosticEngine& diagnostics) const
{
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
            return true;
        }
        result.typeKey = left.typeKey;
        result.valid = true;
        return true;
    }
    // C#'s narrow implicit enumeration conversion: the integer literal zero may stand for the
    // zero value of any enum. Keep this local to equality/inequality; it does not make arbitrary
    // integers comparable with enums, and it does not erase the nominal distinction between two
    // different enum types.
    const bool leftZero = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
        expression->first()->integerValue() == 0;
    const bool rightZero = expression->second() != nullptr &&
        expression->second()->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
        expression->second()->integerValue() == 0;
    const bool enumZeroComparison =
        (operation == TokenKind::EqualEqual || operation == TokenKind::BangEqual) &&
        ((isEnumTypeKey(left.typeKey) && rightZero) ||
         (isEnumTypeKey(right.typeKey) && leftZero));
    if (!enumZeroComparison) return false;
    result.typeKey = "builtin:bool";
    result.valid = true;
    return true;
}

bool ProjectMethodBodyModel::analyzeStructComparison(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
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
    if (!isComparison(operation) ||
        (!typeKeyIsStruct(left.typeKey) && !typeKeyIsStruct(right.typeKey))) return false;
    if (left.typeKey != right.typeKey)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot compare '" + left.typeKey + "' with '" + right.typeKey +
                "': a struct compares only against its own type");
        return true;
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
        return true;
    }
    if (!aggregateAllowsSynthesizedMember(left.typeKey, "Equals"))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "record '" + left.typeKey +
                "' does not derive Equality; add derive(Equality) or declare operator ==");
        return true;
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
        return true;
    }
    std::string unsupportedReason;
    if (!aggregateSupportsSynthesizedMember(left.typeKey, "Equals", unsupportedReason))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            unsupportedReason);
        return true;
    }
    coreBindings_.push_back(MethodCoreBinding{
        MethodCoreOperationKind::StructEquals, expression, methodMemberOrdinal,
        scopeOrdinal, "builtin:bool", left.typeKey, false, std::nullopt, std::nullopt});
    result.typeKey = "builtin:bool";
    result.valid = true;
    result.assignable = false;
    return true;
}

std::optional<std::size_t> ProjectMethodBodyModel::binaryFlowStorageKey(
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

std::optional<std::int64_t> ProjectMethodBodyModel::binarySignedConstant(
    const std::optional<ProjectIntegerConstant>& constant)
{
    if (!constant.has_value() ||
        constant->value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return std::nullopt;
    const std::int64_t magnitude = static_cast<std::int64_t>(constant->value);
    return constant->isNegative ? -magnitude : magnitude;
}

void ProjectMethodBodyModel::refineBinaryIntegerRange(
    OwnershipFlowSnapshot& edge,
    std::size_t key,
    std::optional<std::int64_t> minimum,
    std::optional<std::int64_t> maximum,
    bool excludesZero)
{
    OwnershipFlowStorageFacts* facts = mutableSnapshotStorageFacts(edge, key);
    if (facts == nullptr) return;
    if (!facts->integerRange.has_value()) facts->integerRange.emplace();
    OwnershipFlowIntegerRange& range = *facts->integerRange;
    if (minimum.has_value())
        range.minimum = range.minimum.has_value()
            ? std::max(*range.minimum, *minimum) : minimum;
    if (maximum.has_value())
        range.maximum = range.maximum.has_value()
            ? std::min(*range.maximum, *maximum) : maximum;
    range.excludesZero = range.excludesZero || excludesZero ||
        (range.minimum.has_value() && *range.minimum > 0) ||
        (range.maximum.has_value() && *range.maximum < 0);
}

void ProjectMethodBodyModel::publishBinaryNullFlow(
    ConditionFlowStates& flow,
    bool& published,
    const ast::AstNode* expression,
    TokenKind operation,
    std::size_t scopeOrdinal) const
{
    const bool leftNull = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullLiteralExpression;
    const bool rightNull = expression->second() != nullptr &&
        expression->second()->kind() == ast::AstNodeKind::NullLiteralExpression;
    if ((operation != TokenKind::EqualEqual && operation != TokenKind::BangEqual) ||
        leftNull == rightNull) return;
    const std::optional<std::size_t> key = binaryFlowStorageKey(
        leftNull ? expression->second() : expression->first(), scopeOrdinal);
    if (!key.has_value()) return;
    OwnershipFlowSnapshot& nonNull = operation == TokenKind::BangEqual
        ? flow.whenTrue : flow.whenFalse;
    OwnershipFlowSnapshot& nullEdge = operation == TokenKind::BangEqual
        ? flow.whenFalse : flow.whenTrue;
    if (OwnershipFlowStorageFacts* facts = mutableSnapshotStorageFacts(nonNull, *key);
        facts != nullptr)
        facts->nonNull = true;
    if (OwnershipFlowStorageFacts* facts = mutableSnapshotStorageFacts(nullEdge, *key);
        facts != nullptr)
        facts->nonNull = false;
    published = true;
}

void ProjectMethodBodyModel::publishBinaryIntegerFlow(
    ConditionFlowStates& flow,
    bool& published,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    std::size_t scopeOrdinal) const
{
    std::optional<std::size_t> integerKey = binaryFlowStorageKey(
        expression->first(), scopeOrdinal);
    std::optional<std::int64_t> constant = binarySignedConstant(right.integerConstant);
    TokenKind relation = operation;
    if (!integerKey.has_value())
    {
        integerKey = binaryFlowStorageKey(expression->second(), scopeOrdinal);
        constant = binarySignedConstant(left.integerConstant);
        if (relation == TokenKind::Less) relation = TokenKind::Greater;
        else if (relation == TokenKind::LessEqual) relation = TokenKind::GreaterEqual;
        else if (relation == TokenKind::Greater) relation = TokenKind::Less;
        else if (relation == TokenKind::GreaterEqual) relation = TokenKind::LessEqual;
    }
    if (!integerKey.has_value() || !constant.has_value()) return;
    const std::int64_t value = *constant;
    switch (relation)
    {
        case TokenKind::EqualEqual:
            refineBinaryIntegerRange(flow.whenTrue, *integerKey, value, value, value != 0);
            if (value == 0) refineBinaryIntegerRange(flow.whenFalse, *integerKey, {}, {}, true);
            break;
        case TokenKind::BangEqual:
            if (value == 0) refineBinaryIntegerRange(flow.whenTrue, *integerKey, {}, {}, true);
            refineBinaryIntegerRange(flow.whenFalse, *integerKey, value, value, value != 0);
            break;
        case TokenKind::Less:
            if (value != std::numeric_limits<std::int64_t>::min())
                refineBinaryIntegerRange(flow.whenTrue, *integerKey, {}, value - 1, value <= 0);
            refineBinaryIntegerRange(flow.whenFalse, *integerKey, value, {}, value > 0);
            break;
        case TokenKind::LessEqual:
            refineBinaryIntegerRange(flow.whenTrue, *integerKey, {}, value, value < 0);
            if (value != std::numeric_limits<std::int64_t>::max())
                refineBinaryIntegerRange(flow.whenFalse, *integerKey, value + 1, {}, value >= 0);
            break;
        case TokenKind::Greater:
            if (value != std::numeric_limits<std::int64_t>::max())
                refineBinaryIntegerRange(flow.whenTrue, *integerKey, value + 1, {}, value >= 0);
            refineBinaryIntegerRange(flow.whenFalse, *integerKey, {}, value, value < 0);
            break;
        case TokenKind::GreaterEqual:
            refineBinaryIntegerRange(flow.whenTrue, *integerKey, value, {}, value > 0);
            if (value != std::numeric_limits<std::int64_t>::min())
                refineBinaryIntegerRange(flow.whenFalse, *integerKey, {}, value - 1, value <= 0);
            break;
        default:
            return;
    }
    published = true;
}

bool ProjectMethodBodyModel::analyzeBinaryComparison(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!isComparison(operation)) return false;
    const bool referenceCompatible =
        classifyImplicit(left.typeKey, right.typeKey, left.integerConstant).exists() ||
        classifyImplicit(right.typeKey, left.typeKey, right.integerConstant).exists();
    const bool compatible = left.typeKey == right.typeKey ||
        ProjectTypeConversion::commonNumericType(
            left.typeKey, right.typeKey,
            left.integerConstant, right.integerConstant).has_value() || referenceCompatible;
    if (!compatible)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "comparison operands have incompatible Stage 34 types");
        return true;
    }
    result.typeKey = "builtin:bool";
    result.valid = true;
    // T03-F05. Publish facts on both truth edges. These are deliberately limited to
    // named locals/parameters: fields and properties can be changed through aliases or
    // calls, while a named value has a stable storage key that joins can reason about.
    ConditionFlowStates flow{captureOwnershipFlow(), captureOwnershipFlow()};
    bool published = false;
    publishBinaryNullFlow(flow, published, expression, operation, scopeOrdinal);
    publishBinaryIntegerFlow(
        flow, published, expression, left, right, operation, scopeOrdinal);
    if (published) conditionFlowBySyntax_.insert_or_assign(expression, std::move(flow));
    return true;
}

bool ProjectMethodBodyModel::analyzeShiftBinary(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    diagnostics::DiagnosticEngine& diagnostics) const
{
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
    if (!isShift(operation)) return false;
    if (!ProjectTypeConversion::isIntegral(left.typeKey) ||
        !ProjectTypeConversion::isIntegral(right.typeKey) ||
        left.typeKey == "builtin:bool" || right.typeKey == "builtin:bool")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "shift operands must be integral in Stage 34");
        return true;
    }
    result.typeKey = shiftResultType(left.typeKey);
    result.valid = true;
    return true;
}

bool ProjectMethodBodyModel::analyzePointerArithmetic(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
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
    const bool leftIsPointer = left.valid && left.typeKey.ends_with("*");
    const bool rightIsPointer = right.valid && right.typeKey.ends_with("*");
    const bool plus = operation == lexer::TokenKind::Plus;
    const bool minus = operation == lexer::TokenKind::Minus;
    if ((!leftIsPointer && !rightIsPointer) || (!plus && !minus)) return false;
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
        return true;
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
                      : "two pointers cannot be added; only pointer +/- integer and pointer - "
                        "pointer are defined");
            return true;
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
            return true;
        }
        // `i - p` no existe: restarle una direccion a un entero no significa nada.
        if (minus && !leftIsPointer)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "an integer minus a pointer is not defined; write pointer - integer");
            return true;
        }
        result.typeKey = pointerTypeKey;
        const ExpressionResult& sourcePointer = leftIsPointer ? left : right;
        result.pointerPointeeReadonly = sourcePointer.pointerPointeeReadonly;
        result.pointerNullable = sourcePointer.pointerNullable;
        result.pointerHasProvenance = sourcePointer.pointerHasProvenance;
        // An arbitrary element offset preserves provenance and address space, but only
        // byte alignment is guaranteed without proving the offset is a multiple of the
        // pointee alignment.
        result.pointerKnownAlignment = 1;
        result.pointerAddressSpace = sourcePointer.pointerAddressSpace;
        result.onePastEndPointer = sourcePointer.onePastEndPointer;
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
    return true;
}

bool ProjectMethodBodyModel::analyzeArithmeticBinary(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& left,
    const ExpressionResult& right,
    TokenKind operation,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!isArithmetic(operation)) return false;
    const std::optional<std::string> common = ProjectTypeConversion::commonNumericType(
        left.typeKey, right.typeKey, left.integerConstant, right.integerConstant);
    if (!common.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "binary operator requires compatible numeric operands in Stage 34");
        return true;
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
                ? "division by the constant zero" : "remainder by the constant zero");
        result.valid = false;
        return true;
    }
    if ((operation == TokenKind::Slash || operation == TokenKind::Percent) &&
        ProjectIntegerEvaluator::isIntegerType(result.typeKey) && right.flowIntegerExcludesZero)
        result.flowRuntimeCheckElided = true;
    return true;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeBinary(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const TokenKind operation = expression->tokenKind();

    if (isLogical(operation))
        return analyzeLogicalBinary(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);

    const ExpressionResult left = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    const ExpressionResult right = analyzeExpression(
        expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics);

    if (!left.valid || !right.valid)
    {
        return {};
    }

    ExpressionResult result;

    if (analyzeStringBinary(
            result, expression, left, right, operation,
            methodMemberOrdinal, scopeOrdinal)) return result;
    if (analyzeOverloadedBinary(
            result, expression, left, right, operation,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;

    if (analyzePortableSimdBinary(
            result, expression, left, right, operation, diagnostics)) return result;
    if (analyzeEnumBinary(
            result, expression, left, right, operation, diagnostics)) return result;

    if (analyzeStructComparison(
            result, expression, left, right, operation,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;

    if (analyzeBinaryComparison(
            result, expression, left, right, operation,
            scopeOrdinal, diagnostics)) return result;
    if (analyzeShiftBinary(
            result, expression, left, right, operation, diagnostics)) return result;
    if (analyzePointerArithmetic(
            result, expression, left, right, operation,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;
    if (analyzeArithmeticBinary(
            result, expression, left, right, operation,
            scopeOrdinal, diagnostics)) return result;
    return result;
}
}
