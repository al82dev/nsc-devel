#pragma once

#include <string>

namespace nativesharp::compilation
{
// EN: One precompiled dependency a semantic snapshot was resolved against: its module name and the
//     SHA-256 (hex) of the exact NSMETA bytes the producer loaded.
// ES: Una dependencia precompilada contra la que se resolvió un snapshot semántico: su nombre de
//     módulo y el SHA-256 (hex) de los bytes exactos de la NSMETA que cargó el productor.
struct PrecompiledSnapshotDependency final
{
    std::string moduleName;
    std::string metadataDigest;
};
}
