#pragma once

#include <string_view>

namespace nativesharp
{
// La version de producto identifica esta rama como 3.0. La version visible agrega metadata SemVer
// unica de cada build; sus dos valores se definen en Version.cpp, el unico consumidor del header
// generado por CMake. La sintaxis reconocida es acumulativa y no se selecciona por proyecto.
inline constexpr std::string_view kLanguageVersion = "3.0.0";
inline constexpr std::string_view kCompilerReleaseVersion = "3.0.0";
inline constexpr std::string_view kPreviousCompilerReleaseVersion = "2.0.0";
inline constexpr std::string_view kCompilerStage = "70";
extern const std::string_view kCompilerVersion;
extern const std::string_view kCompilerBuildId;
}
