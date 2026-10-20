#pragma once

#include "Stage37ConstantKind.h"

#include <cstdint>
#include <string>

namespace nativesharp::frontend::method_body_detail
{
struct Stage37ConstantValue final
{
    Stage37ConstantValue() = default;
    Stage37ConstantValue(
        Stage37ConstantKind valueKind,
        std::string valueTypeKey,
        std::uint64_t integerValue,
        double floatingValue,
        bool booleanValue,
        std::string stringValue = {});

    Stage37ConstantKind kind = Stage37ConstantKind::Integer;
    std::string typeKey;
    std::uint64_t integer = 0;
    double floating = 0.0;
    bool boolean = false;
    std::string string;
};
}
