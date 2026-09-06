#pragma once

#include <string_view>

namespace nativesharp
{
// La version del lenguaje y la compatibilidad del compilador son estables. La version visible
// agrega metadata SemVer unica de cada build; sus dos valores se definen en Version.cpp, el unico
// consumidor del header generado por CMake.
inline constexpr std::string_view kLanguageVersion = "0.1";
inline constexpr std::string_view kCompilerReleaseVersion = "0.8.0-alpha";
inline constexpr std::string_view kCompilerStage = "57";
extern const std::string_view kCompilerVersion;
extern const std::string_view kCompilerBuildId;
}
