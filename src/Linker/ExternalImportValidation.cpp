#include "nativesharp/Linker/ExternalImportValidation.h"

#include "nativesharp/Linker/PeExecutableVerifier.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <Windows.h>

namespace nativesharp::linker
{
namespace
{
std::string lowerAscii(std::string_view text)
{
    std::string lowered(text);
    for (char& character : lowered)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return lowered;
}

// EN: API-set names are resolved by the loader, not by a file of that name.
// ES: Los nombres de API set los resuelve el loader, no un archivo con ese nombre.
bool isApiSetName(std::string_view dllName)
{
    const std::string lowered = lowerAscii(dllName);
    return lowered.starts_with("api-ms-win-") || lowered.starts_with("ext-ms-");
}

std::vector<std::filesystem::path> systemSearchDirectories()
{
    std::vector<std::filesystem::path> directories;
    wchar_t buffer[MAX_PATH] = {};
    const UINT systemLength = GetSystemDirectoryW(buffer, MAX_PATH);
    if (systemLength > 0 && systemLength < MAX_PATH) directories.emplace_back(buffer);
    const UINT windowsLength = GetWindowsDirectoryW(buffer, MAX_PATH);
    if (windowsLength > 0 && windowsLength < MAX_PATH) directories.emplace_back(buffer);
    const DWORD pathLength = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    if (pathLength == 0) return directories;
    std::wstring path(pathLength, L'\0');
    const DWORD written = GetEnvironmentVariableW(L"PATH", path.data(), pathLength);
    if (written == 0 || written >= pathLength) return directories;
    path.resize(written);
    std::wstring_view remaining(path);
    while (!remaining.empty())
    {
        const std::size_t separator = remaining.find(L';');
        const std::wstring_view entry = remaining.substr(0, separator);
        if (!entry.empty()) directories.emplace_back(std::wstring(entry));
        if (separator == std::wstring_view::npos) break;
        remaining.remove_prefix(separator + 1);
    }
    return directories;
}

std::optional<std::filesystem::path> locateDll(
    std::string_view dllName,
    const std::vector<std::filesystem::path>& directories)
{
    const std::filesystem::path named{std::string(dllName)};
    std::error_code fileError;
    if (named.has_parent_path())
    {
        if (std::filesystem::is_regular_file(named, fileError)) return named;
        return std::nullopt;
    }
    for (const std::filesystem::path& directory : directories)
    {
        const std::filesystem::path candidate = directory / named;
        if (std::filesystem::is_regular_file(candidate, fileError)) return candidate;
    }
    return std::nullopt;
}
}

bool validateExternalImportExports(
    std::span<const ExternalImportModule> externalImports,
    std::string& error)
{
    if (externalImports.empty()) return true;
    const std::vector<std::filesystem::path> directories = systemSearchDirectories();
    std::string missing;
    for (const ExternalImportModule& module : externalImports)
    {
        if (module.dllName.empty() || module.symbols.empty() || isApiSetName(module.dllName)) continue;
        const std::optional<std::filesystem::path> dll = locateDll(module.dllName, directories);
        if (!dll.has_value()) continue;
        // EN: A file that is not a 64-bit DLL (a 32-bit copy on PATH, say) cannot be judged.
        // ES: Un archivo que no es una DLL de 64 bits (una copia de 32 bits en PATH) no se juzga.
        PeExecutableVerifier verifier;
        if (!verifier.verifyDynamicLibrary(*dll)) continue;
        const std::vector<std::string>& exported = verifier.info().exportedSymbols;
        const std::unordered_set<std::string> exports(exported.begin(), exported.end());
        for (const std::string& symbol : module.symbols)
        {
            if (exports.contains(symbol)) continue;
            if (!missing.empty()) missing += "; ";
            missing += "'" + symbol + "' is not exported by '" + module.dllName + "' (" + dll->string() + ")";
        }
    }
    if (missing.empty()) return true;
    error = "extern C import cannot be resolved, so the program would fail to load: " + missing;
    return false;
}
}
