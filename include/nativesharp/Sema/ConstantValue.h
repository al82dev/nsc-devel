#pragma once

#include "nativesharp/Sema/ConstantKind.h"

#include <cstdint>
#include <string>

namespace nativesharp::sema
{
class ConstantValue final
{
public:
    static ConstantValue signedInteger(std::int64_t value) noexcept;
    static ConstantValue unsignedInteger(std::uint64_t value) noexcept;
    static ConstantValue floating(double value) noexcept;
    static ConstantValue boolean(bool value) noexcept;
    static ConstantValue nullValue() noexcept;
    static ConstantValue stringValue(std::string value);

    ConstantKind kind() const noexcept;
    std::int64_t signedInteger() const noexcept;
    std::uint64_t unsignedInteger() const noexcept;
    double floating() const noexcept;
    bool boolean() const noexcept;
    const std::string& string() const noexcept;

private:
    ConstantKind kind_ = ConstantKind::None;
    std::int64_t signedInteger_ = 0;
    std::uint64_t unsignedInteger_ = 0;
    double floating_ = 0.0;
    bool boolean_ = false;
    std::string string_;
};
}
