#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/HIR/HirNodeKind.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ConversionKind.h"

#include <cstdint>
#include <vector>

namespace nativesharp::sema
{
class Symbol;
class Type;
}

namespace nativesharp::hir
{
class HirNode final
{
public:
    HirNode(HirNodeKind kind, SourceRange range);

    HirNodeKind kind() const noexcept;
    SourceRange range() const noexcept;

    sema::Type* type() const noexcept;
    void setType(sema::Type* type) noexcept;

    sema::Type* operationType() const noexcept;
    void setOperationType(sema::Type* type) noexcept;

    sema::Symbol* symbol() const noexcept;
    void setSymbol(sema::Symbol* symbol) noexcept;

    lexer::TokenKind operation() const noexcept;
    void setOperation(lexer::TokenKind operation) noexcept;

    bool hasConstant() const noexcept;
    const sema::ConstantValue& constant() const noexcept;
    void setConstant(sema::ConstantValue value);

    bool hasConversion() const noexcept;
    sema::ConversionKind conversionKind() const noexcept;
    void setConversionKind(sema::ConversionKind kind) noexcept;

    bool hasReadConversion() const noexcept;
    sema::ConversionKind readConversionKind() const noexcept;
    void setReadConversionKind(sema::ConversionKind kind) noexcept;

    bool hasResultConversion() const noexcept;
    sema::ConversionKind resultConversionKind() const noexcept;
    void setResultConversionKind(sema::ConversionKind kind) noexcept;

    bool requiresBoundsCheck() const noexcept;
    void setRequiresBoundsCheck(bool value) noexcept;

    bool arrayForm() const noexcept;
    void setArrayForm(bool value) noexcept;

    bool postfix() const noexcept;
    void setPostfix(bool value) noexcept;

    bool hasOffset() const noexcept;
    std::uint64_t offset() const noexcept;
    void setOffset(std::uint64_t value) noexcept;

    HirNode* first() const noexcept;
    HirNode* second() const noexcept;
    HirNode* third() const noexcept;
    void setFirst(HirNode* node) noexcept;
    void setSecond(HirNode* node) noexcept;
    void setThird(HirNode* node) noexcept;

    void addChild(HirNode* node);
    const std::vector<HirNode*>& children() const noexcept;

private:
    HirNodeKind kind_;
    SourceRange range_;
    sema::Type* type_ = nullptr;
    sema::Type* operationType_ = nullptr;
    sema::Symbol* symbol_ = nullptr;
    lexer::TokenKind operation_ = lexer::TokenKind::Invalid;
    bool hasConstant_ = false;
    sema::ConstantValue constant_;
    bool hasConversion_ = false;
    sema::ConversionKind conversionKind_ = sema::ConversionKind::Identity;
    bool hasReadConversion_ = false;
    sema::ConversionKind readConversionKind_ = sema::ConversionKind::Identity;
    bool hasResultConversion_ = false;
    sema::ConversionKind resultConversionKind_ = sema::ConversionKind::Identity;
    bool requiresBoundsCheck_ = false;
    bool arrayForm_ = false;
    bool postfix_ = false;
    bool hasOffset_ = false;
    std::uint64_t offset_ = 0;
    HirNode* first_ = nullptr;
    HirNode* second_ = nullptr;
    HirNode* third_ = nullptr;
    std::vector<HirNode*> children_;
};
}
