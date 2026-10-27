#pragma once

#include "nativesharp/Frontend/ClassInterfaceTablePlan.h"
#include "nativesharp/Frontend/TraitOperatorWitness.h"

#include <cstddef>
#include <vector>

namespace nativesharp::frontend
{
// EN: Plan item 3.7. What the interface phase produced for one implementing class or struct of a
//     restored API unit, read from the `interfaces` section with every reference resolved to this
//     compilation's ordinals.
// ES: Punto 3.7 del plan. Lo que la fase de interfaces produjo para una clase o struct
//     implementadora de una unidad de API restaurada, leído de la sección `interfaces` con cada
//     referencia resuelta a los ordinales de esta compilación.
struct ClassInterfaceSnapshotEntry final
{
    std::vector<std::size_t> implementedInterfaceOrdinals;
    std::vector<TraitOperatorWitness> operatorWitnesses;
    std::vector<ClassInterfaceTablePlan> tablePlans;
};
}
