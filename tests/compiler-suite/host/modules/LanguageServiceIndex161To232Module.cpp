#include "LanguageServiceTestSupport.h"

namespace nsc::suite
{
void registerLanguageServiceIndex161To232Module(TestRegistry& registry)
{
    using language_service_support::addInfrastructureContract;
    addInfrastructureContract(registry, 21U, "unit",
        "SemanticIndex ownership is private and has no authorized construction seam");
    addInfrastructureContract(registry, 22U, "unit",
        "languageLocation and identifierLocation are private and have no authorized unit seam");
    addInfrastructureContract(registry, 23U, "unit",
        "frontend-kind mapping helpers are private and have no authorized table seam");
    addInfrastructureContract(registry, 24U, "unit",
        "callable detail rendering is private and has no authorized model-construction seam");
    addInfrastructureContract(registry, 25U, "unit",
        "semantic symbol population is private and requires a controlled frontend-model seam");
    addInfrastructureContract(registry, 26U, "unit",
        "semantic reference indexing is private and requires controlled binding models");
    addInfrastructureContract(registry, 27U, "unit",
        "call and signature indexing is private and requires controlled method-body models");
    addInfrastructureContract(registry, 28U, "unit",
        "smallest-range selection helpers are private and have no authorized index seam");
    addInfrastructureContract(registry, 29U, "unit",
        "target grouping and overload expansion are private and have no authorized index seam");
}
}
