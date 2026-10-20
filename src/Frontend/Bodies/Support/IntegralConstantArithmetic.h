#pragma once

#include "../../Internal/Stage37ConstantValue.h"

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Lexer/TokenKind.h"

namespace nativesharp::frontend
{
using method_body_detail::Stage37ConstantValue;

bool isErrorType(std::string_view key) noexcept;
std::string integerLiteralType(const ast::AstNode* node, bool negated = false);
bool isUnsignedIntegerLiteral(const ast::AstNode* node) noexcept;
bool isSignedIntegralType(std::string_view typeKey) noexcept;
unsigned integralBitWidth(std::string_view typeKey) noexcept;
std::uint64_t integralMask(unsigned width) noexcept;
std::uint64_t normalizeIntegral(std::uint64_t value, std::string_view typeKey) noexcept;
std::optional<std::pair<std::string, std::size_t>> decodeResultCarrierMarker(
    std::string_view marker,
    std::string_view prefix);
std::int64_t signedIntegral(std::uint64_t bits, std::string_view typeKey) noexcept;
double normalizeFloatingConstant(double value, std::string_view typeKey) noexcept;
std::optional<Stage37ConstantValue> convertStage37Constant(
    Stage37ConstantValue value,
    std::string_view target);
std::string stage37ExpressionType(
    const ProjectMethodBodyModel& model,
    const ast::AstNode* syntax,
    std::size_t context);
std::optional<Stage37ConstantValue> evaluateStage37Constant(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* node,
    std::size_t context,
    std::unordered_set<std::size_t>& evaluatingFields);
std::optional<Stage37ConstantValue> evaluateStage37Identifier(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* node,
    std::size_t context,
    std::unordered_set<std::size_t>& evaluatingFields);
std::optional<Stage37ConstantValue> evaluateStage37Unary(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* node,
    std::size_t context,
    std::string resultType,
    std::unordered_set<std::size_t>& evaluatingFields);
std::optional<Stage37ConstantValue> evaluateStage37Comparison(
    const Stage37ConstantValue& leftRaw,
    const Stage37ConstantValue& rightRaw,
    lexer::TokenKind operation);
std::optional<Stage37ConstantValue> evaluateStage37FloatingArithmetic(
    const Stage37ConstantValue& left,
    const Stage37ConstantValue& right,
    lexer::TokenKind operation,
    std::string_view resultType);
std::optional<Stage37ConstantValue> evaluateStage37IntegerArithmetic(
    const Stage37ConstantValue& left,
    const Stage37ConstantValue& right,
    lexer::TokenKind operation,
    std::string_view resultType);
std::optional<Stage37ConstantValue> evaluateStage37Binary(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* node,
    std::size_t context,
    std::string_view resultType,
    std::unordered_set<std::size_t>& evaluatingFields);
}
