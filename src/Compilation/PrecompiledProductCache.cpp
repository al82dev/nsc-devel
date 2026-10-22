#include "nativesharp/Compilation/PrecompiledProductCache.h"

#include "../Frontend/Cache/CachePayloadReader.h"
#include "../Frontend/Cache/CachePayloadWriter.h"

#include <utility>

namespace nativesharp::compilation
{
namespace
{
constexpr std::string_view productSchema = "nsc-precompiled-source-products-v1";
}

void PrecompiledProductCache::setRoot(std::filesystem::path root)
{
    storage_.setRoot(std::move(root) / "sources");
}

bool PrecompiledProductCache::restore(
    std::string_view key,
    std::string& publicApi,
    std::string& publicTemplates,
    bool& hit,
    std::string& error) const
{
    hit = false;
    std::string payload;
    if (!storage_.restore(key, payload, hit, error) || !hit) return false;
    frontend::internal::CachePayloadReader reader(payload);
    std::string schema;
    std::string restoredApi;
    std::string restoredTemplates;
    if (!reader.string(schema) || schema != productSchema ||
        !reader.string(restoredApi) || !reader.string(restoredTemplates) ||
        !reader.remaining().empty())
    {
        storage_.quarantine(key);
        hit = false;
        return true;
    }
    publicApi = std::move(restoredApi);
    publicTemplates = std::move(restoredTemplates);
    return true;
}

bool PrecompiledProductCache::store(
    std::string_view key,
    std::string_view publicApi,
    std::string_view publicTemplates,
    std::string& error) const
{
    std::string payload;
    frontend::internal::CachePayloadWriter::string(payload, productSchema);
    frontend::internal::CachePayloadWriter::string(payload, publicApi);
    frontend::internal::CachePayloadWriter::string(payload, publicTemplates);
    return storage_.store(key, payload, error);
}
} // namespace nativesharp::compilation
