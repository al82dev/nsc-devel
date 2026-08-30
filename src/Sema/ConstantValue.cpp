#include "nativesharp/Sema/ConstantValue.h"

#include <utility>

namespace nativesharp::sema
{
ConstantValue ConstantValue::signedInteger(std::int64_t value) noexcept
{
    ConstantValue result;
    result.kind_ = ConstantKind::SignedInteger;
    result.signedInteger_ = value;
    return result;
}

ConstantValue ConstantValue::unsignedInteger(std::uint64_t value) noexcept
{
    ConstantValue result;
    result.kind_ = ConstantKind::UnsignedInteger;
    result.unsignedInteger_ = value;
    return result;
}

ConstantValue ConstantValue::floating(double value) noexcept
{
    ConstantValue result;
    result.kind_ = ConstantKind::Floating;
    result.floating_ = value;
    return result;
}

ConstantValue ConstantValue::boolean(bool value) noexcept
{
    ConstantValue result;
    result.kind_ = ConstantKind::Boolean;
    result.boolean_ = value;
    return result;
}

ConstantValue ConstantValue::nullValue() noexcept
{
    ConstantValue result;
    result.kind_ = ConstantKind::Null;
    return result;
}

ConstantValue ConstantValue::stringValue(std::string value)
{
    ConstantValue result;
    result.kind_ = ConstantKind::String;
    result.string_ = std::move(value);
    return result;
}

ConstantKind ConstantValue::kind() const noexcept { return kind_; }
std::int64_t ConstantValue::signedInteger() const noexcept { return signedInteger_; }
std::uint64_t ConstantValue::unsignedInteger() const noexcept { return unsignedInteger_; }
double ConstantValue::floating() const noexcept { return floating_; }
bool ConstantValue::boolean() const noexcept { return boolean_; }
const std::string& ConstantValue::string() const noexcept { return string_; }
}
