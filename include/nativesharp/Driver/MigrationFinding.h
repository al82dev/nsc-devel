#pragma once

#include "nativesharp/Driver/MigrationTextEdit.h"
#include "nativesharp/Language/CompatibilitySemantics.h"

#include <cstdint>
#include <optional>
#include <string>

namespace nativesharp::driver
{
struct MigrationFinding final
{
    language::MigrationIssue issue = language::MigrationIssue::NewlyContextualWord;
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    std::string message;
    std::string alternative;
    std::optional<MigrationTextEdit> safeFix;
};
} // namespace nativesharp::driver
