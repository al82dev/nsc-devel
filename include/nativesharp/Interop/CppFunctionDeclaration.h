#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppType.h"
#include "nativesharp/Interop/CppAbiPlan.h"

#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace nativesharp::interop
{
enum class CppLanguageLinkage { None, C, Cpp };

// Textual spellings support identification and diagnostics; they are not an ABI classification.
struct CppFunctionParameter
{
    std::string name;
    std::string typeSpelling;
    std::string canonicalTypeSpelling;
    CppType type;
    bool hasDefaultArgument = false;
};

struct CppFunctionDeclaration
{
    std::string name;
    std::string qualifiedName;
    std::string canonicalFunctionType;
    std::string callingConvention;
    CppLanguageLinkage languageLinkage = CppLanguageLinkage::None;
    std::string returnTypeSpelling;
    std::string canonicalReturnTypeSpelling;
    CppType returnType;
    std::vector<CppFunctionParameter> parameters;
    bool variadic = false;
    bool externalLinkage = false;
    // True only if Clang resolved a non-throwing exception specification.
    bool isNonThrowing = false;
    bool dllImport = false;
    // An external spelling does not guarantee that a particular binary defines the symbol.
    std::optional<std::string> externalSymbol;
    std::optional<CppSourcePoint> location;
    // Opaque Clang USR; absence means Clang could not produce one.
    std::optional<std::string> clangUsr;
    // A C declaration f() has no prototype; f(void) does.
    bool hasPrototype = true;
    // Definition owned by this C translation unit; not an external ABI symbol.
    bool isCStaticInlineDefinition = false;
    // Optional experimental ABI observation; absence never implies a Win64 fallback.
    std::shared_ptr<const abi_plan::Plan> abiPlan;
};
}
