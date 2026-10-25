#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
// EN: Freeze 3.0 C-05: project files that exist only in memory. A lone `.ns` given to the driver
//     becomes such a project (its source stays on disk, beside the virtual project file), and the
//     loader reads the registered text instead of a file. The registry is process-wide because
//     every ProjectLoader instance (the driver's, the project graph's, the compilation's) must
//     resolve the same project.
// ES: Congelamiento 3.0 C-05: archivos de proyecto que solo existen en memoria. Un `.ns` suelto
//     que recibe el driver se vuelve un proyecto asi (su fuente sigue en disco, junto al archivo
//     de proyecto virtual), y el loader lee el texto registrado en lugar de un archivo. El registro
//     es de todo el proceso porque cada instancia de ProjectLoader (la del driver, la del grafo de
//     proyectos, la de la compilacion) debe resolver el mismo proyecto.
class ImplicitProjectRegistry final
{
public:
    static void add(const std::filesystem::path& projectFile, std::string text);
    static std::optional<std::string> find(const std::filesystem::path& projectFile);
};
}
