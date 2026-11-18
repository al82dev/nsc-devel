#pragma once

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace nativesharp::interop
{
enum class CppLanguageMode { C, Cpp };
enum class CppIncludeKind { User, System };
enum class CppMacroAction { Define, Undefine };
enum class CppVolatileMode { Default, Microsoft, Iso };

struct CppIncludePath
{
    std::filesystem::path path;
    CppIncludeKind kind = CppIncludeKind::User;
};

struct CppMacroOption
{
    CppMacroAction action = CppMacroAction::Define;
    // A definition may include its value, e.g. NAME=VALUE.
    std::string text;
};

struct CppFileInput
{
    std::filesystem::path path;
};

struct CppBufferInput
{
    std::filesystem::path virtualPath;
    std::string contents;
};

// All strings, paths and ordered options are owned by the receiving session.
struct CppSessionOptions
{
    CppLanguageMode language = CppLanguageMode::Cpp;
    std::string languageStandard;
    std::string targetTriple;
    // EN: T19-036. Clang's -march for the compilation's CPU level; empty for the base target.
    // ES: T19-036. El -march de Clang del nivel de CPU de la compilacion; vacio para el destino base.
    std::string targetCpu{};
    std::filesystem::path driverReferencePath;
    std::filesystem::path resourceDirectory;
    std::filesystem::path workingDirectory;
    std::vector<CppIncludePath> includePaths;
    std::vector<CppMacroOption> macros;
    std::variant<CppFileInput, CppBufferInput> input;
    CppVolatileMode volatileMode = CppVolatileMode::Default;
    // Optional because ordinary compilation does not need the extra token tracing.
    bool capturePreprocessingFingerprint = false;
    // EN: T19-003. A session over the preprocessed source a precompiled producer carries, used only to expand
    //     the C function-like macros its published templates call: it reports only those macros and their
    //     wrappers; every other declaration comes from the producer's catalogue.
    // ES: T19-003. Una sesion sobre la fuente preprocesada que lleva un productor precompilado, usada solo para
    //     expandir las macros de funcion C que llaman sus plantillas publicadas: informa solo esas macros y sus
    //     envoltorios; cualquier otra declaracion viene del catalogo del productor.
    bool macroExpansionOnly = false;
};

// Stable identity of these option values. It is not a complete cache key: file
// contents, toolchain binaries and transitive include state are not captured.
std::string cppSessionConfigurationIdentity(const CppSessionOptions& options);
}
