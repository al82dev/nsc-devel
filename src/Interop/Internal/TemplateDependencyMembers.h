#pragma once

#include "nativesharp/Frontend/ProjectCompilation.h"

#include <cstddef>
#include <unordered_set>

namespace nativesharp::interop::metadata_detail
{
// EN: T08-D19: ordinals of the non-public members that a template published by the root project names from inside
//     their declaring type. NSMETA publishes them (keeping their accessibility) and a DLL exports their symbols.
// ES: T08-D19: ordinales de los miembros no publicos que una plantilla publicada por el proyecto raiz nombra desde
//     dentro de su tipo. NSMETA los publica (con su accesibilidad) y una DLL exporta sus simbolos.
std::unordered_set<std::size_t> templateDependencyMemberOrdinals(
    const frontend::ProjectCompilation& compilation,
    std::size_t rootProjectOrdinal);
}
