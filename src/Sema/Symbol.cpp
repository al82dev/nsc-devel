#include "nativesharp/Sema/Symbol.h"

#include <utility>

namespace nativesharp::sema
{
Symbol::Symbol(SymbolKind kind, std::string name, Type* type, const ast::AstNode* declaration)
    : kind_(kind),
      name_(std::move(name)),
      type_(type),
      declaration_(declaration)
{
}

SymbolKind Symbol::kind() const noexcept { return kind_; }
const std::string& Symbol::name() const noexcept { return name_; }
Type* Symbol::type() const noexcept { return type_; }
const ast::AstNode* Symbol::declaration() const noexcept { return declaration_; }
bool Symbol::isConst() const noexcept { return isConst_; }
void Symbol::setConst(bool value) noexcept { isConst_ = value; }
bool Symbol::isDefined() const noexcept { return isDefined_; }
void Symbol::setDefined(bool value) noexcept { isDefined_ = value; }
bool Symbol::isExternC() const noexcept { return isExternC_; }
void Symbol::setExternC(bool value) noexcept { isExternC_ = value; }
std::int64_t Symbol::enumValue() const noexcept { return enumValue_; }
void Symbol::setEnumValue(std::int64_t value) noexcept { enumValue_ = value; }
bool Symbol::hasFieldOffset() const noexcept { return fieldOffset_.has_value(); }
std::uint64_t Symbol::fieldOffset() const noexcept { return fieldOffset_.value_or(0); }
void Symbol::setFieldOffset(std::uint64_t offset) noexcept { fieldOffset_ = offset; }
bool Symbol::hasConstantValue() const noexcept { return hasConstantValue_; }
const ConstantValue& Symbol::constantValue() const noexcept { return constantValue_; }
void Symbol::setConstantValue(ConstantValue value)
{
    constantValue_ = std::move(value);
    hasConstantValue_ = true;
}
void Symbol::addMember(Symbol* symbol) { members_.push_back(symbol); }
const std::vector<Symbol*>& Symbol::members() const noexcept { return members_; }

Symbol* Symbol::findMember(std::string_view name) const noexcept
{
    for (Symbol* member : members_)
    {
        if (member->name() == name)
        {
            return member;
        }
    }
    return nullptr;
}
}
