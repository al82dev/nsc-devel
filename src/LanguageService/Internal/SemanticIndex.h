#pragma once

#include "IndexedCall.h"
#include "IndexedReference.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LanguageService/LanguageSymbol.h"

#include <memory>
#include <vector>

namespace nativesharp::language_service
{
struct SemanticIndex final
{
    std::shared_ptr<frontend::ProjectCompilation> compilation;
    std::vector<LanguageSymbol> symbols;
    std::vector<IndexedReference> references;
    std::vector<IndexedCall> calls;
};
}
