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
    std::vector<std::string> externalTypeKeys;
    std::filesystem::path productSidecarPath;
    // EN: SHA-256 (hex) of the module's NSMETA bytes; snapshots of other libraries name it as a
    //     dependency digest.
    // ES: SHA-256 (hex) de los bytes de la NSMETA del módulo; los snapshots de otras bibliotecas lo
    //     nombran como digest de dependencia.
    std::string metadataDigest;
    // EN: Semantic snapshot bytes of the public API unit from the published `.nsproducts`; empty
    //     when the sidecar has none or the published products were not used.
    // ES: Bytes del snapshot semántico de la unidad de API pública del `.nsproducts` publicado;
    //     vacío si el sidecar no lo trae o si no se usaron los productos publicados.
    std::string publicApiSnapshot;
    // EN: True when the consumer's demand pruned the module surface (plan A1.4): its API unit is
    //     generated from the pruned metadata, so no published snapshot applies to it.
    // ES: True cuando la demanda del consumidor podó la superficie del módulo (plan A1.4): su
    //     unidad de API se genera desde la metadata podada, así que ningún snapshot publicado le
    //     aplica.
    bool surfacePruned = false;
};
}
