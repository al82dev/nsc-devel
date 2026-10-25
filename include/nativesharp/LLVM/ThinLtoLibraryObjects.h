#pragma once

#include <filesystem>
#include <vector>

namespace nativesharp::llvm_backend
{
/// EN: The native objects the ThinLTO backends wrote for the units of one consumed static library,
///     in unit order; the driver links them in place of `libraryPath` (optimization part 2, item 48).
/// ES: Los objetos nativos que los backends ThinLTO escribieron para las unidades de una biblioteca
///     estatica consumida, en orden de unidad; el driver los enlaza en lugar de `libraryPath` (parte 2,
///     punto 48).
struct ThinLtoLibraryObjects final
{
    std::filesystem::path libraryPath;
    std::vector<std::filesystem::path> objectFiles;
};
}
