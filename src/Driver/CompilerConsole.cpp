#include "nativesharp/Driver/CompilerConsole.h"

#include "nativesharp/Basic/Version.h"

#include <ostream>
#include <utility>

namespace nativesharp::driver
{
namespace
{
thread_local CompilerLanguage currentLanguage = CompilerLanguage::English;

bool parseLanguage(std::string_view value, CompilerLanguage& language) noexcept
{
    if (value == "en")
    {
        language = CompilerLanguage::English;
        return true;
    }
    if (value == "es")
    {
        language = CompilerLanguage::Spanish;
        return true;
    }
    return false;
}
}

CompilerConsoleArguments parseCompilerConsoleArguments(
    std::span<const std::string_view> arguments, CompilerLanguage fallback)
{
    CompilerConsoleArguments result;
    result.language = fallback;
    result.arguments.reserve(arguments.size());

    bool languageSeen = false;
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        std::string_view value;
        bool languageArgument = false;
        if (argument == "--language")
        {
            languageArgument = true;
            if (index + 1U >= arguments.size())
            {
                result.error = result.language == CompilerLanguage::Spanish
                    ? "la opción --language requiere en o es"
                    : "--language requires en or es";
                return result;
            }
            value = arguments[++index];
        }
        else if (argument.starts_with("--language="))
        {
            languageArgument = true;
            value = argument.substr(std::string_view("--language=").size());
        }

        if (!languageArgument)
        {
            result.arguments.push_back(argument);
            continue;
        }
        if (languageSeen)
        {
            result.error = result.language == CompilerLanguage::Spanish
                ? "la opción --language no puede repetirse"
                : "--language may not be specified more than once";
            return result;
        }
        languageSeen = true;
        CompilerLanguage parsed = result.language;
        if (!parseLanguage(value, parsed))
        {
            result.error = result.language == CompilerLanguage::Spanish
                ? "idioma no compatible '" + std::string(value) + "'; se esperaba en o es"
                : "unsupported language '" + std::string(value) + "'; expected en or es";
            return result;
        }
        result.language = parsed;
    }
    return result;
}

CompilerLanguage activeCompilerLanguage() noexcept
{
    return currentLanguage;
}

CompilerLanguageScope::CompilerLanguageScope(CompilerLanguage language) noexcept
    : previous_(currentLanguage)
{
    currentLanguage = language;
}

CompilerLanguageScope::~CompilerLanguageScope()
{
    currentLanguage = previous_;
}

std::string_view compilerText(std::string_view english, std::string_view spanish) noexcept
{
    return currentLanguage == CompilerLanguage::Spanish ? spanish : english;
}

std::string localizeCompilerMessage(std::string_view english)
{
    if (currentLanguage != CompilerLanguage::Spanish) return std::string(english);

    static constexpr std::pair<std::string_view, std::string_view> exact[] = {
        {"project name must be a valid NativeSharp identifier",
         "el nombre del proyecto debe ser un identificador NativeSharp válido"},
        {"template must be console, classlib, or dll",
         "la plantilla debe ser console, classlib o dll"},
        {"cannot resolve output directory", "no se puede resolver el directorio de salida"},
        {"project already exists; pass --force to replace its configuration",
         "el proyecto ya existe; use --force para reemplazar su configuración"},
        {"configuration must be Debug or Release", "la configuración debe ser Debug o Release"},
        {"optimization must be O0, O1, O2, or O3", "la optimización debe ser O0, O1, O2 u O3"},
        {"linker must be NSLink or lld-link", "el enlazador debe ser NSLink o lld-link"},
        {"library-reference root requires an absolute directory",
         "la raíz de referencias de bibliotecas requiere un directorio absoluto"},
        {"compiler failed", "el compilador falló"},
        {"nsc run requires an executable project", "nsc run requiere un proyecto ejecutable"},
        {"nsc run is currently supported on Windows only",
         "nsc run actualmente sólo es compatible con Windows"},
        {"clean safety validation failed", "falló la validación de seguridad de clean"},
        {"package id is required", "se requiere el identificador del paquete"},
        {"only one project may be specified", "sólo se puede especificar un proyecto"},
        {"unsafe package id or version", "el identificador o la versión del paquete no son seguros"},
        {"restore accepts only one project", "restore acepta un solo proyecto"},
        {"build accepts only one project", "build acepta un solo proyecto"},
        {"--force cannot be combined with --locked-mode or --verify",
         "--force no puede combinarse con --locked-mode ni --verify"},
        {"duplicate compilation job count", "el número de trabajos de compilación está repetido"},
        {"-j requires a job count from 1 through 1024",
         "-j requiere un número de trabajos entre 1 y 1024"},
        {"compilation job count must be between 1 and 1024",
         "el número de trabajos de compilación debe estar entre 1 y 1024"},
        {"duplicate library-reference root", "la raíz de referencias de bibliotecas está repetida"},
        {"--library-reference-root requires an absolute directory",
         "--library-reference-root requiere un directorio absoluto"},
    };
    for (const auto& [source, translated] : exact)
        if (english == source) return std::string(translated);

    static constexpr std::pair<std::string_view, std::string_view> prefixes[] = {
        {"usage: ", "uso: "},
        {"unknown new option: ", "opción de new desconocida: "},
        {"unknown restore option: ", "opción de restore desconocida: "},
        {"unknown build option: ", "opción de build desconocida: "},
        {"unknown package option: ", "opción de paquete desconocida: "},
        {"cannot create project directory: ", "no se puede crear el directorio del proyecto: "},
        {"cannot create build output directory: ", "no se puede crear el directorio de salida: "},
        {"cannot clean generated project files: ", "no se pueden limpiar los archivos generados: "},
        {"cannot clean '", "no se puede limpiar '"},
        {"project does not exist: ", "el proyecto no existe: "},
        {"project must have the .nsproj extension", "el proyecto debe tener la extensión .nsproj"},
        {"no .nsproj was found in the current directory", "no se encontró un .nsproj en el directorio actual"},
        {"more than one .nsproj exists; specify the project explicitly",
         "existe más de un .nsproj; especifique el proyecto explícitamente"},
        {"generated build project is invalid: ", "el proyecto de construcción generado no es válido: "},
        {"program exited with code ", "el programa terminó con el código "},
        {"package is not present: ", "el paquete no está presente: "},
        {"Created NativeSharp project: ", "Proyecto NativeSharp creado: "},
        {"Restore verified: ", "Restauración verificada: "},
        {"Restored ", "Restaurados "},
        {"Build succeeded: ", "Construcción correcta: "},
        {"Program exited with code 0", "El programa terminó con el código 0"},
        {"Package request saved: ", "Solicitud de paquete guardada: "},
        {"Package request removed: ", "Solicitud de paquete eliminada: "},
    };
    for (const auto& [source, translated] : prefixes)
    {
        if (english.starts_with(source))
            return std::string(translated) + std::string(english.substr(source.size()));
    }
    return std::string(english);
}

void printCompilerBanner(std::ostream& output)
{
    if (currentLanguage == CompilerLanguage::Spanish)
    {
        output << "Compilador NativeSharp " << kCompilerReleaseVersion << " (nsc)\n"
               << "Copyright (c) 2026 Alberto Huerta Luna. Todos los derechos reservados.\n"
               << "Hash de compilación: " << kCompilerBuildId << '\n';
        return;
    }
    output << "NativeSharp Compiler " << kCompilerReleaseVersion << " (nsc)\n"
           << "Copyright (c) 2026 Alberto Huerta Luna. All rights reserved.\n"
           << "Build hash: " << kCompilerBuildId << '\n';
}
}
