#pragma once

#include <string>

namespace nativesharp::compilation
{
struct PrecompiledProductSidecar final
{
    std::string publicApiSource;
    std::string publicTemplateSource;
    std::string publicApiSyntax;
    std::string publicTemplateSyntax;
    // EN: Semantic snapshot of the public API unit (sidecar schema v2). Empty when the sidecar
    //     carries none: every v1 sidecar, and a producer that could not build one.
    // ES: Snapshot semántico de la unidad de API pública (esquema v2 del sidecar). Vacío cuando el
    //     sidecar no lo trae: todo sidecar v1 y un productor que no pudo construirlo.
    std::string publicApiSnapshot;
};
}
