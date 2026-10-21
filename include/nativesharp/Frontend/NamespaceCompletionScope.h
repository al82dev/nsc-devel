#pragma once

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
struct NamespaceCompletionScope final
{
    FileId fileId;
    SourceRange range;
    std::size_t projectOrdinal = 0;
    std::string lexicalNamespace;
    std::vector<std::string> importedNamespaces;
    std::unordered_map<std::string, std::string> aliases;
};
}
