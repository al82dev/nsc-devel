#pragma once

#include <cstddef>
#include <optional>

namespace nativesharp::frontend
{
// NSC-LIMIT-010. Una transferencia de propiedad elegida por la resolucion de constructores: que
// parametro la recibe y de donde sale. Es la misma informacion que una llamada ordinaria ya
// registraba; el constructor no la tenia porque su resolucion no miraba propiedad en absoluto.
struct ConstructorOwnershipTransfer final
{
    std::size_t parameterOrdinal = 0;
    std::optional<std::size_t> sourceLocalOrdinal;
    std::optional<std::size_t> sourceParameterOrdinal;
};
}
