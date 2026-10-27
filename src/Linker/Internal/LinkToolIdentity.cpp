#include "LinkToolIdentity.h"

#include "nativesharp/Basic/Sha256.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <system_error>
#include <utility>

namespace nativesharp::linker
{
namespace
{
/// EN: SHA-256 hex digest and size of `tool`, computed once per (path, size, write time) and
///     process. Returns false with `error` when the file cannot be read or hashed.
/// ES: Digest SHA-256 hexadecimal y tamano de `tool`, calculados una vez por (ruta, tamano, fecha
///     de escritura) y proceso. Devuelve false con `error` si el archivo no se puede leer o hashear.
bool toolDigest(
    const std::filesystem::path& tool,
    std::uintmax_t& size,
    std::string& digest,
    std::string& error)
{
    static std::mutex mutex;
    static std::map<std::string, std::pair<std::uintmax_t, std::string>> digests;
    std::error_code sizeError;
    const std::uintmax_t fileSize = std::filesystem::file_size(tool, sizeError);
    std::error_code timeError;
    const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(tool, timeError);
    const bool memoizable = !sizeError && !timeError;
    const std::string memoKey = tool.generic_string() + "|" + std::to_string(fileSize) + "|" +
        std::to_string(writeTime.time_since_epoch().count());
    const std::lock_guard<std::mutex> lock(mutex);
    if (memoizable)
    {
        const auto found = digests.find(memoKey);
        if (found != digests.end())
        {
            size = found->second.first;
            digest = found->second.second;
            return true;
        }
    }
    basic::Sha256Digest value{};
    if (!basic::sha256File(tool, value, size, error)) return false;
    digest = basic::sha256Hex(value);
    if (memoizable) digests.insert_or_assign(memoKey, std::make_pair(size, digest));
    return true;
}
}

bool appendLinkToolIdentity(
    std::string_view role,
    const std::filesystem::path& tool,
    std::string& material,
    std::string& error)
{
    std::error_code filesystemError;
    if (tool.empty() || !std::filesystem::is_regular_file(tool, filesystemError) || filesystemError)
    {
        material += "link-tool " + std::string(role) + " unavailable\n";
        return true;
    }
    std::uintmax_t size = 0;
    std::string digest;
    if (!toolDigest(tool, size, digest, error)) return false;
    material += "link-tool " + std::string(role) + " " + tool.filename().generic_string() + ":" +
        std::to_string(size) + ":" + digest + "\n";
    return true;
}
}
