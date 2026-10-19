#pragma once

#include "nativesharp/LanguageService/LanguageTextEdit.h"

#include <string>
#include <vector>

namespace nativesharp::language_service
{
struct RenameResult final
{
    bool allowed = false;
    std::string error;
    std::vector<LanguageTextEdit> edits;
};
}
