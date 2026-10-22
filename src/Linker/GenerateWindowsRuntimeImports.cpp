#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: nsc-windows-runtime-imports <output-directory>\n";
        return 2;
    }

    const std::filesystem::path outputDirectory(argv[1]);
    std::error_code directoryError;
    std::filesystem::create_directories(outputDirectory, directoryError);
    if (directoryError)
    {
        std::cerr << "cannot create runtime import-library directory: "
                  << directoryError.message() << '\n';
        return 1;
    }

    std::string error;
    std::unique_ptr<nativesharp::linker::LldLinker> linker =
        nativesharp::linker::LldLinker::discover(error);
    if (linker == nullptr)
    {
        std::cerr << error << '\n';
        return 1;
    }

    std::vector<std::filesystem::path> libraries;
    if (!nativesharp::linker::createCanonicalWindowsImportLibraries(
            *linker, outputDirectory, libraries, error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    if (libraries.size() != 4U)
    {
        std::cerr << "canonical Windows runtime import closure did not produce four libraries\n";
        return 1;
    }
    return 0;
}
