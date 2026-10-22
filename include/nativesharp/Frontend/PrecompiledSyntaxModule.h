#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct PrecompiledSyntaxModule final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    std::string moduleName;
    std::string productCacheKey;
    std::string sourceText;
    std::string templateSourceText;
    std::string publicApiSyntaxProduct;
    std::string publicTemplateSyntaxProduct;
    std::vector<PrecompiledMemberIdentity> members;
    std::vector<PrecompiledNamespaceFunctionIdentity> namespaceFunctions;
    std::vector<PrecompiledGenericSpecialization> genericSpecializations;
    std::filesystem::path productSidecarPath;
};
}
