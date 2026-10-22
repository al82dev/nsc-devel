#pragma once

#include "nativesharp/Linker/LldLinker.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::linker
{
// Interop Foundation: one external DLL's imported C symbols, as collected from the project's
// `extern C from "<dll>" { ... }` declarations (or handed in by tooling). Symbols merge into the
// runtime's own import set when the DLL matches (case-insensitively) one the runtime already
// links, and get an import library of their own otherwise.
struct ExternalImportModule final
{
    std::string dllName;
    std::vector<std::string> symbols;
};

bool createCanonicalWindowsImportLibraries(
    const LldLinker& linker,
    const std::filesystem::path& directory,
    std::vector<std::filesystem::path>& libraries,
    std::string& error);

// THE single source of truth for the Windows runtime's import surface, and the single place that
// materializes import libraries for a link.
//
// HISTORY -- before this existed, the four runtime symbol tables (KERNEL32's 62 entries,
// KERNELBASE, SHELL32, ucrtbase) were DUPLICATED verbatim between WindowsExecutableBuilder.cpp
// and WindowsLibraryBuilder.cpp, and the project carried a standing rule that any new runtime
// Win32 import had to be synchronized across five locations by hand. That rule existed because
// nothing enforced agreement; this component is the enforcement. There is exactly one table now,
// both builders consume it, and external imports flow through the same door instead of scaling
// the hardcoded arrays API by API -- the audit's "no debe escalarse API por API" requirement.
//
// HISTORICAL: `combinedIrText` drove conditional ucrtbase entries because LLVM passes could
// materialize @llvm.memcpy/memset/memmove or reference __C_specific_handler inside either module.
// CURRENT: the import closure is canonical and external imports are explicit. The parameter is
// retained for source and ABI compatibility, but implementations and callers do not serialize IR.
bool createWindowsImportLibraries(
    const LldLinker& linker,
    const std::filesystem::path& directory,
    std::string_view combinedIrText,
    std::span<const ExternalImportModule> externalImports,
    std::vector<std::filesystem::path>& libraries,
    std::string& error);
}
