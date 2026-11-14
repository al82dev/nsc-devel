#pragma once

#include "nativesharp/Frontend/MethodLocalSymbol.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"

#include <cstdint>
#include <span>

namespace nativesharp::frontend::internal
{
inline void foldCppObjectSizeIndices(ProjectOopHirNode& node, std::span<const std::uint8_t> modes,
                                     std::span<const MethodLocalSymbol> locals)
{
    for (std::size_t argument = 0; argument < node.children.size(); ++argument)
    {
        const auto parameter = node.argumentParameterOrdinals.empty() ? argument :
            argument < node.argumentParameterOrdinals.size() ? node.argumentParameterOrdinals[argument] : modes.size();
        if (parameter >= modes.size()) continue;
        const auto mode = modes[parameter];
        if (!mode || ((mode - 1) % 4 != 1 && (mode - 1) % 4 != 3)) continue;
        auto* address = node.children[argument].get();
        if (!address || address->kind != ProjectOopHirNodeKind::AddressValue ||
            address->children.size() != 1 || !address->children[0]) continue;
        auto* element = address->children[0].get();
        if (element->kind != ProjectOopHirNodeKind::RawElementAddress ||
            element->children.size() != 2 || !element->children[1]) continue;
        auto* index = element->children[1].get();
        if (index->kind != ProjectOopHirNodeKind::LocalValue || !index->localOrdinal ||
            *index->localOrdinal >= locals.size()) continue;
        const auto& local = locals[*index->localOrdinal];
        if (!local.isConst || local.isVolatile || local.isReference || local.isStatic ||
            !local.constantIntegerValue || *local.constantIntegerValue < 0) continue;
        index->kind = ProjectOopHirNodeKind::IntegerConstant;
        index->integerValue = static_cast<std::uint64_t>(*local.constantIntegerValue);
        index->localOrdinal.reset();
    }
}
}
