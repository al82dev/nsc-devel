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
using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

+
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


bool ProjectMethodBodyModel::isCompileTimeConstantExpression(
    const ast::AstNode* expression,
    std::size_t contextMemberOrdinal) const
{
    if (expression == nullptr || memberModel_ == nullptr)
    {
        return false;
    }

    // NSC-LIMIT-019. Member ordinals are not dependency order. Track the active graph so valid
    // forward/cross-project references work while direct and transitive cycles remain invalid.
    std::unordered_set<std::size_t> evaluatingFields;
    evaluatingFields.insert(contextMemberOrdinal);

    return evaluateStage37Constant(
        *this, memberModel_, expression, contextMemberOrdinal, evaluatingFields).has_value();
}

}
