#pragma once

#include <string>

namespace nativesharp::frontend
{
// A candidate only: Fields independently parses and verifies this source
// against the transported AST before trusting any of its declarations.
struct PrecompiledTemplateFieldCertificate final
{
    std::string templateKey;
    std::string sourceText;
};
}
