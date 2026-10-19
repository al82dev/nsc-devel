#pragma once

#include "nativesharp/Frontend/ProjectConversionKind.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectIntegerConstant final
{
    std::uint64_t value = 0;
    bool isUnsigned = false;
    // ID 119. `value` es una MAGNITUD, no un patron de bits en complemento a dos: `constantFits`
    // solo compara contra cotas superiores. Un literal negativo se representa entonces con la
    // magnitud y esta bandera, no con `value` negado -- negarlo daria 0xFFFF...FD y ninguna cota
    // superior lo aceptaria.
    bool isNegative = false;
};
}
