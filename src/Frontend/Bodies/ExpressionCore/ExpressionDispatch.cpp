#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "../Support/OwnershipArgumentClassification.h"
#include "../Support/OperatorClassification.h"
#include "../Support/IntegralConstantArithmetic.h"
#include "../Support/OwnershipFlowSnapshotQueries.h"


#include "../MethodBodyInternals.h"
#include "../../Internal/Stage37ConstantValue.h"

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

bool ProjectMethodBodyModel::analyzeExpressionCoreFlowKinds(
    ExpressionResult& result,
    const ExpressionAnalysisRequest& request,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* expression = request.syntax;
    switch (expression->kind())
    {
        case ast::AstNodeKind::LoopExpression:
            result = analyzeExpressionKindLoopExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::MatchExpression:
            result = analyzeExpressionKindMatchExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::MatchValueExpression:
            result = analyzeExpressionKindMatchValueExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::MatchArmExpression:
            result = analyzeExpressionKindMatchArmExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::AwaitExpression:
            result = analyzeExpressionKindAwaitExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        default:
            return false;
    }
}

bool ProjectMethodBodyModel::analyzeExpressionCorePrimaryKinds(
    ExpressionResult& result,
    const ExpressionAnalysisRequest& request,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* expression = request.syntax;
    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
            result = analyzeExpressionKindIdentifierExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::ThisExpression:
            result = analyzeExpressionKindThisExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::BaseExpression:
            result = analyzeExpressionKindBaseExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        default:
            return false;
    }
}

bool ProjectMethodBodyModel::analyzeExpressionCoreLiteralKinds(
    ExpressionResult& result,
    const ExpressionAnalysisRequest& request,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* expression = request.syntax;
    switch (expression->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
            result = analyzeExpressionKindIntegerLiteralExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::FloatingLiteralExpression:
            result = analyzeExpressionKindFloatingLiteralExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::CharacterLiteralExpression:
            result = analyzeExpressionKindCharacterLiteralExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::OverflowPolicyExpression:
            result = analyzeExpressionKindOverflowPolicyExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::NameOfExpression:
            result = analyzeExpressionKindNameOfExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::StringLiteralExpression:
            result = analyzeExpressionKindStringLiteralExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::InterpolatedStringExpression:
            result = analyzeExpressionKindInterpolatedStringExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::BooleanLiteralExpression:
            result = analyzeExpressionKindBooleanLiteralExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::NullLiteralExpression:
            result = analyzeExpressionKindNullLiteralExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        default:
            return false;
    }
}

bool ProjectMethodBodyModel::analyzeExpressionCoreAccessKinds(
    ExpressionResult& result,
    const ExpressionAnalysisRequest& request,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* expression = request.syntax;
    switch (expression->kind())
    {
        case ast::AstNodeKind::LambdaExpression:
            result = analyzeExpressionKindLambdaExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::MoveExpression:
            result = analyzeExpressionKindMoveExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::NullConditionalMemberExpression:
            result = analyzeExpressionKindNullConditionalMemberExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::MemberExpression:
            result = analyzeExpressionKindMemberExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::PointerMemberExpression:
            result = analyzeExpressionKindPointerMemberExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::CallExpression:
            result = analyzeExpressionKindCallExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::ResultPropagationExpression:
            result = analyzeExpressionKindResultPropagationExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        default:
            return false;
    }
}

bool ProjectMethodBodyModel::analyzeExpressionCoreOperatorKinds(
    ExpressionResult& result,
    const ExpressionAnalysisRequest& request,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* expression = request.syntax;
    switch (expression->kind())
    {
        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
            result = analyzeExpressionKindUnaryExpressionOrPostfixExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::BinaryExpression:
            result = analyzeExpressionKindBinaryExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::NullCoalescingExpression:
            result = analyzeExpressionKindNullCoalescingExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::TupleExpression:
            result = analyzeExpressionKindTupleExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::WithExpression:
            result = analyzeExpressionKindWithExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::ConditionalExpression:
            result = analyzeExpressionKindConditionalExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::AssignmentExpression:
            result = analyzeExpressionKindAssignmentExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        default:
            return false;
    }
}

bool ProjectMethodBodyModel::analyzeExpressionCoreTypeKinds(
    ExpressionResult& result,
    const ExpressionAnalysisRequest& request,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* expression = request.syntax;
    switch (expression->kind())
    {
        case ast::AstNodeKind::TypeTestExpression:
        case ast::AstNodeKind::SafeCastExpression:
            result = analyzeExpressionKindTypeTestExpressionOrSafeCastExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::CastExpression:
            result = analyzeExpressionKindCastExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::FromEndExpression:
            result = analyzeExpressionKindFromEndExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::RangeExpression:
            result = analyzeExpressionKindRangeExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::IndexExpression:
            result = analyzeExpressionKindIndexExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::SizeofExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::AlignofExpression:
            result =
                analyzeExpressionKindSizeofTypeExpressionOrSizeofExpressionOrAlignofTypeExpressionOrAlignofExpression(
                    expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                    request.allowMethodGroup, request.allowEvent,
                    request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::OffsetofExpression:
            result = analyzeExpressionKindOffsetofExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::DefaultValueExpression:
            result = analyzeExpressionKindDefaultValueExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        default:
            return false;
    }
}

bool ProjectMethodBodyModel::analyzeExpressionCoreConstructionKinds(
    ExpressionResult& result,
    const ExpressionAnalysisRequest& request,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* expression = request.syntax;
    switch (expression->kind())
    {
        case ast::AstNodeKind::ObjectCreationExpression:
            result = analyzeExpressionKindObjectCreationExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::PlacementConstructionExpression:
            result = analyzeExpressionKindPlacementConstructionExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::NewExpression:
        case ast::AstNodeKind::StackallocExpression:
            result = analyzeExpressionKindNewExpressionOrStackallocExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::DeleteExpression:
            result = analyzeExpressionKindDeleteExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::DestroyExpression:
            result = analyzeExpressionKindDestroyExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::NullConditionalIndexExpression:
            result = analyzeExpressionKindNullConditionalIndexExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        case ast::AstNodeKind::CollectionExpression:
            result = analyzeExpressionKindCollectionExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                request.allowMethodGroup, request.allowEvent,
                request.allowMove, request.allowConstructorPropertyWrite);
            return true;
        default:
            return false;
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionCore(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    if (expression == nullptr)
    {
        ExpressionResult empty;
        empty.typeKey = "builtin:void";
        empty.valid = true;
        return empty;
    }

    ExpressionResult result;
    const ExpressionAnalysisRequest request{
        expression,
        allowMethodGroup,
        allowEvent,
        allowMove,
        allowConstructorPropertyWrite};
    bool handled = analyzeExpressionCoreFlowKinds(
        result, request, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!handled) handled = analyzeExpressionCorePrimaryKinds(
        result, request, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!handled) handled = analyzeExpressionCoreLiteralKinds(
        result, request, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!handled) handled = analyzeExpressionCoreAccessKinds(
        result, request, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!handled) handled = analyzeExpressionCoreOperatorKinds(
        result, request, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!handled) handled = analyzeExpressionCoreTypeKinds(
        result, request, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!handled)
    {
        analyzeExpressionCoreConstructionKinds(
            result, request, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
    recordExpressionType(expression, methodMemberOrdinal, scopeOrdinal, result);
    return result;
}
}
