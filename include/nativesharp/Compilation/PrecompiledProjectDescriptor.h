#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
struct PrecompiledProjectDescriptor final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    std::string moduleName;
    // EN: Source projects of the graph that provide modules this metadata names (external types). The synthetic
    //     project sees them, as it sees a peer precompiled module: its published templates may import their
    //     namespaces.
    // ES: Proyectos fuente del grafo que aportan modulos nombrados por estos metadatos (tipos externos). El
    //     proyecto sintetico los ve, como ve un modulo precompilado par: sus plantillas publicadas pueden importar
    //     sus namespaces.
    std::vector<std::filesystem::path> sourceDependencies{};
};
}
