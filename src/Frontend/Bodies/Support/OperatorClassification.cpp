#include "OwnershipArgumentClassification.h"
#include "OperatorClassification.h"
#include "IntegralConstantArithmetic.h"
#include "OwnershipFlowSnapshotQueries.h"

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

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
using lexer::TokenKind;

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
    const auto spelling = [](TokenKind operation) noexcept -> std::string_view
    {
        switch (operation)
        {
            case TokenKind::Plus: return "+";
            case TokenKind::Minus: return "-";
            case TokenKind::Star: return "*";
            case TokenKind::Slash: return "/";
            case TokenKind::Percent: return "%";
            case TokenKind::Bang: return "!";
            case TokenKind::Tilde: return "~";
            case TokenKind::Amp: return "&";
            case TokenKind::Pipe: return "|";
            case TokenKind::Caret: return "^";
            case TokenKind::LessLess: return "<<";
            case TokenKind::GreaterGreater: return ">>";
            case TokenKind::EqualEqual: return "==";
            case TokenKind::BangEqual: return "!=";
            case TokenKind::Less: return "<";
            case TokenKind::LessEqual: return "<=";
            case TokenKind::Greater: return ">";
            case TokenKind::GreaterEqual: return ">=";
            default: return {};
        }
    };
    return std::string(language::operatorMethodName(spelling(kind), arity));
}

std::string stage38OperatorTraitKey(TokenKind kind, std::size_t arity)
{
    switch (kind)
    {
        case TokenKind::Plus: return std::string(language::operatorTrait("+", arity));
        case TokenKind::Minus: return std::string(language::operatorTrait("-", arity));
        case TokenKind::Star: return std::string(language::operatorTrait("*", arity));
        case TokenKind::Slash: return std::string(language::operatorTrait("/", arity));
        case TokenKind::Percent: return std::string(language::operatorTrait("%", arity));
        case TokenKind::Bang: return std::string(language::operatorTrait("!", arity));
        case TokenKind::Tilde: return std::string(language::operatorTrait("~", arity));
        case TokenKind::Amp: return std::string(language::operatorTrait("&", arity));
        case TokenKind::Pipe: return std::string(language::operatorTrait("|", arity));
        case TokenKind::Caret: return std::string(language::operatorTrait("^", arity));
        case TokenKind::LessLess: return std::string(language::operatorTrait("<<", arity));
        case TokenKind::GreaterGreater: return std::string(language::operatorTrait(">>", arity));
        case TokenKind::EqualEqual: return std::string(language::operatorTrait("==", arity));
        case TokenKind::BangEqual: return std::string(language::operatorTrait("!=", arity));
        case TokenKind::Less: return std::string(language::operatorTrait("<", arity));
        case TokenKind::LessEqual: return std::string(language::operatorTrait("<=", arity));
        case TokenKind::Greater: return std::string(language::operatorTrait(">", arity));
        case TokenKind::GreaterEqual: return std::string(language::operatorTrait(">=", arity));
        default: return {};
    }
}
}
