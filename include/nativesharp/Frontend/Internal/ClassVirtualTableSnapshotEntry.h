#pragma once

#include "nativesharp/Frontend/VirtualMethodInfo.h"

#include <cstddef>
#include <vector>

namespace nativesharp::frontend
{
// EN: Plan item 3.9. The virtual table of one class of a restored API unit and the virtual methods
//     its own members registered, read from the `dispatch` section with every member reference
//     resolved to this compilation's ordinals.
// ES: Punto 3.9 del plan. La tabla virtual de una clase de una unidad de API restaurada y los
//     métodos virtuales que registraron sus propios miembros, leídos de la sección `dispatch` con
//     cada referencia de miembro resuelta a los ordinales de esta compilación.
struct ClassVirtualTableSnapshotEntry final
{
    std::vector<std::size_t> slotMemberOrdinals;
    std::vector<bool> slotAbstract;
    std::vector<VirtualMethodInfo> methods;
};
}
