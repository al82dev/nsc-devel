#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct BilingualClause final
{
    std::string identity;
    std::string spanishRequirement;
    std::string englishRequirement;
    std::size_t spanishExamples = 0;
    std::size_t englishExamples = 0;
    std::size_t spanishCompatibilityNotes = 0;
    std::size_t englishCompatibilityNotes = 0;
    bool glossaryConsistent = true;
    bool linksValid = true;
    bool codeSamplesCompile = true;
    bool presentInFinalIndexes = true;
};
}
