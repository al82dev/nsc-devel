#pragma once

#include "nativesharp/Frontend/TemplateParseCache.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::compilation
{
class PrecompiledProductCache final
{
public:
    void setRoot(std::filesystem::path root);

    bool restore(
        std::string_view key,
        std::string& publicApi,
        std::string& publicTemplates,
        bool& hit,
        std::string& error) const;

    bool store(
        std::string_view key,
        std::string_view publicApi,
        std::string_view publicTemplates,
        std::string& error) const;

private:
    frontend::TemplateParseCache storage_;
};
} // namespace nativesharp::compilation
