#pragma once

#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/SymbolKind.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::sema
{
class Type;

class Symbol final
{
public:
    Symbol(SymbolKind kind, std::string name, Type* type, const ast::AstNode* declaration);

    SymbolKind kind() const noexcept;
    const std::string& name() const noexcept;
    Type* type() const noexcept;
    const ast::AstNode* declaration() const noexcept;

    bool isConst() const noexcept;
    void setConst(bool value) noexcept;
    bool isDefined() const noexcept;
    void setDefined(bool value) noexcept;
    bool isExternC() const noexcept;
    void setExternC(bool value) noexcept;

    std::int64_t enumValue() const noexcept;
    void setEnumValue(std::int64_t value) noexcept;

    bool hasFieldOffset() const noexcept;
    std::uint64_t fieldOffset() const noexcept;
    void setFieldOffset(std::uint64_t offset) noexcept;

    bool hasConstantValue() const noexcept;
    const ConstantValue& constantValue() const noexcept;
    void setConstantValue(ConstantValue value);

    void addMember(Symbol* symbol);
    const std::vector<Symbol*>& members() const noexcept;
    Symbol* findMember(std::string_view name) const noexcept;

private:
    SymbolKind kind_;
    std::string name_;
    Type* type_;
    const ast::AstNode* declaration_;
    bool isConst_ = false;
    bool isDefined_ = false;
    bool isExternC_ = false;
    std::int64_t enumValue_ = 0;
    std::optional<std::uint64_t> fieldOffset_;
    bool hasConstantValue_ = false;
    ConstantValue constantValue_;
    std::vector<Symbol*> members_;
};
}
