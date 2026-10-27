#pragma once

#include "nativesharp/Frontend/StructValueLayout.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
// EN: Plan item 3.12. The value layout of one struct of a restored API unit and every layout its
//     computation asked for, in order (is class, type ordinal), read from the `layout` section.
// ES: Punto 3.12 del plan. El layout de valor de una struct de una unidad de API restaurada y cada
//     layout que pidió su cálculo, en orden (es clase, ordinal de tipo), leídos de la sección
//     `layout`.
struct StructLayoutSnapshotEntry final
{
    StructValueLayout layout;
    std::vector<std::pair<bool, std::size_t>> layoutNeeds;
};
}
