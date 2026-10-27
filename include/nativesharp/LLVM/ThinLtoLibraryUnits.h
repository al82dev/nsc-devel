#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::llvm_backend
{
/// EN: The summary bitcode units of one static library whose sidecar the driver accepted, handed to
///     the object generator so its ThinLTO backends import from them (optimization part 2, item 48).
/// ES: Las unidades de bitcode con resumen de una biblioteca estatica cuyo sidecar acepto el driver,
///     entregadas al generador de objetos para que sus backends ThinLTO importen de ellas (parte 2,
///     punto 48).
struct ThinLtoLibraryUnits final
{
    std::filesystem::path libraryPath;
    std::vector<std::string> units;
};
}
