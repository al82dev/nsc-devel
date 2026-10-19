#pragma once

#include <string>

namespace nativesharp::language
{
struct SyntaxFreezeManifest final
{
    std::string grammarRevision;
    std::string canonicalGrammarHash;
    std::string precedenceHash;
    std::string reservedWordsHash;
    std::string recoveryRulesHash;
    bool generatedFromCanonicalSource = false;
    bool parserCorpusPassed = false;
    bool ambiguityCheckPassed = false;
    bool formatterRoundTripPassed = false;
    bool specificationSnippetsPassed = false;
};
}
