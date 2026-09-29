#pragma once

#include <string_view>

namespace nativesharp
{
// La edicion de lenguaje predeterminada sigue siendo 2.0 para conservar compatibilidad con
// proyectos sin <Language>. La version de producto identifica esta rama de desarrollo como 3.0.
// La version visible agrega metadata SemVer unica de cada build; sus dos valores se definen en
// Version.cpp, el unico consumidor del header generado por CMake.
inline constexpr std::string_view kLanguageVersion = "2.0.0";
inline constexpr std::string_view kCompilerReleaseVersion = "3.0.0";
inline constexpr std::string_view kCompilerStage = "70";
extern const std::string_view kCompilerVersion;
extern const std::string_view kCompilerBuildId;
}
