#pragma once

#include "nativesharp/Frontend/GenericConstraintViolationKind.h"

#include <cstddef>
#include <string>

namespace nativesharp::frontend
{
struct GenericConstraintViolation final
{
    GenericConstraintViolationKind kind = GenericConstraintViolationKind::RequiresReferenceType;
    // Ordinal de la ESPECIALIZACION que incumple, o de la PLANTILLA cuando la violacion es de
    // declaracion --`isDeclarationViolation`--, que no tiene sitio de instanciacion al que
    // apuntar porque el error esta en el `where`, no en quien lo usa.
    std::size_t specializationOrdinal = 0;
    std::string parameterName;
    std::string argumentTypeKey;
    std::string constraintTypeKey;
    // Cierto para lo que esta mal ESCRITO en la clausula --un constraint que no nombra clase ni
    // interfaz, o el mismo tipo dos veces bajo dos nombres--. Se comprueba sobre la plantilla y
    // por tanto se reporta aunque nadie la instancie.
    bool isDeclarationViolation = false;
};
}
