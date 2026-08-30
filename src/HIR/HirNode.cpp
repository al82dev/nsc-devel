#include "nativesharp/HIR/HirNode.h"

#include <utility>

namespace nativesharp::hir
{
HirNode::HirNode(HirNodeKind kind, SourceRange range)
    : kind_(kind), range_(range)
{
}

HirNodeKind HirNode::kind() const noexcept { return kind_; }
SourceRange HirNode::range() const noexcept { return range_; }
sema::Type* HirNode::type() const noexcept { return type_; }
void HirNode::setType(sema::Type* type) noexcept { type_ = type; }
sema::Type* HirNode::operationType() const noexcept { return operationType_; }
void HirNode::setOperationType(sema::Type* type) noexcept { operationType_ = type; }
sema::Symbol* HirNode::symbol() const noexcept { return symbol_; }
void HirNode::setSymbol(sema::Symbol* symbol) noexcept { symbol_ = symbol; }
lexer::TokenKind HirNode::operation() const noexcept { return operation_; }
void HirNode::setOperation(lexer::TokenKind operation) noexcept { operation_ = operation; }
bool HirNode::hasConstant() const noexcept { return hasConstant_; }
const sema::ConstantValue& HirNode::constant() const noexcept { return constant_; }

void HirNode::setConstant(sema::ConstantValue value)
{
    constant_ = std::move(value);
    hasConstant_ = true;
}

bool HirNode::hasConversion() const noexcept { return hasConversion_; }
sema::ConversionKind HirNode::conversionKind() const noexcept { return conversionKind_; }

void HirNode::setConversionKind(sema::ConversionKind kind) noexcept
{
    conversionKind_ = kind;
    hasConversion_ = true;
}

bool HirNode::hasReadConversion() const noexcept { return hasReadConversion_; }
sema::ConversionKind HirNode::readConversionKind() const noexcept { return readConversionKind_; }

void HirNode::setReadConversionKind(sema::ConversionKind kind) noexcept
{
    readConversionKind_ = kind;
    hasReadConversion_ = true;
}

bool HirNode::hasResultConversion() const noexcept { return hasResultConversion_; }
sema::ConversionKind HirNode::resultConversionKind() const noexcept { return resultConversionKind_; }

void HirNode::setResultConversionKind(sema::ConversionKind kind) noexcept
{
    resultConversionKind_ = kind;
    hasResultConversion_ = true;
}

bool HirNode::requiresBoundsCheck() const noexcept { return requiresBoundsCheck_; }
void HirNode::setRequiresBoundsCheck(bool value) noexcept { requiresBoundsCheck_ = value; }
bool HirNode::arrayForm() const noexcept { return arrayForm_; }
void HirNode::setArrayForm(bool value) noexcept { arrayForm_ = value; }
bool HirNode::postfix() const noexcept { return postfix_; }
void HirNode::setPostfix(bool value) noexcept { postfix_ = value; }
bool HirNode::hasOffset() const noexcept { return hasOffset_; }
std::uint64_t HirNode::offset() const noexcept { return offset_; }

void HirNode::setOffset(std::uint64_t value) noexcept
{
    offset_ = value;
    hasOffset_ = true;
}

HirNode* HirNode::first() const noexcept { return first_; }
HirNode* HirNode::second() const noexcept { return second_; }
HirNode* HirNode::third() const noexcept { return third_; }
void HirNode::setFirst(HirNode* node) noexcept { first_ = node; }
void HirNode::setSecond(HirNode* node) noexcept { second_ = node; }
void HirNode::setThird(HirNode* node) noexcept { third_ = node; }

void HirNode::addChild(HirNode* node)
{
    children_.push_back(node);
}

const std::vector<HirNode*>& HirNode::children() const noexcept
{
    return children_;
}
}
