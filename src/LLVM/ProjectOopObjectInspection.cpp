#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"

#include "Internal/ThinLtoBackend.h"

// EN: Static object and archive inspection of ProjectOopObjectGenerator. It only delegates to ThinLtoBackend, so the
//     driver and the compiler suite use it without compiling the code generator.
// ES: Inspeccion estatica de objetos y archivos de ProjectOopObjectGenerator. Solo delega en ThinLtoBackend, asi que
//     el driver y el suite la usan sin compilar el generador de codigo.
namespace nativesharp::llvm_backend
{
bool ProjectOopObjectGenerator::collectUndefinedSymbols(
    const std::filesystem::path& file,
    std::vector<std::string>& symbols,
    std::string& error)
{
    return ThinLtoBackend::collectUndefinedSymbols(file, symbols, error);
}

bool ProjectOopObjectGenerator::validateThinLtoNativeObject(std::string_view bytes, std::string& error)
{
    return ThinLtoBackend::validateNativeObject(bytes, error);
}

bool ProjectOopObjectGenerator::requiresLldForStaticInput(
    const std::filesystem::path& file, bool& found, std::string& error)
{
    return ThinLtoBackend::requiresLldForStaticInput(file, found, error);
}

bool ProjectOopObjectGenerator::findLinkedCrtInitializers(std::span<const std::filesystem::path> objects,
                                                          std::span<const std::filesystem::path> libraries,
                                                          std::string& input, std::string& member, std::string& error)
{
    return ThinLtoBackend::findLinkedCrtInitializers(objects, libraries, input, member, error);
}

bool ProjectOopObjectGenerator::findMissingLinkSymbols(
    std::span<const std::filesystem::path> objects, std::span<const std::filesystem::path> libraries,
    std::span<const std::string> symbols,
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>>& missing,
    std::string& error)
{
    return ThinLtoBackend::findMissingLinkSymbols(objects, libraries, symbols, missing, error);
}

std::string ProjectOopObjectGenerator::demangleSymbol(std::string_view symbol)
{
    return ThinLtoBackend::demangleSymbol(symbol);
}

bool ProjectOopObjectGenerator::thinLtoRuns(LinkTimeOptimizationMode mode, LlvmOptimizationLevel level) noexcept
{
    return mode == LinkTimeOptimizationMode::Thin && level != LlvmOptimizationLevel::O0 &&
        ThinLtoBackend::available();
}
} // namespace nativesharp::llvm_backend
