#pragma once

#include <string_view>

namespace nativesharp
{
// La version de producto y la version publicada del lenguaje identifican esta rama como 3.0.
// Esto no cambia la edicion predeterminada de los proyectos sin <Language>: esa decision vive en
// LanguageEdition y permanece en V2 por compatibilidad. La version visible agrega metadata SemVer
// unica de cada build; sus dos valores se definen en Version.cpp, el unico consumidor del header
// generado por CMake.
inline constexpr std::string_view kLanguageVersion = "3.0.0";
inline constexpr std::string_view kCompilerReleaseVersion = "3.0.0";
inline constexpr std::string_view kCompilerStage = "70";
extern const std::string_view kCompilerVersion;
extern const std::string_view kCompilerBuildId;
}
