#pragma once

#include "nativesharp/Compilation/DeclaredAccessibility.h"
#include "nativesharp/Compilation/MergedTypeDescriptor.h"
#include "nativesharp/Compilation/TypeDeclarationKind.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Compilation/TypePartDescriptor.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
class PartialTypeAggregator final
{
public:
    bool addPart(TypePartDescriptor part, std::string& error);
    bool finalize(std::string& error);

    std::span<const MergedTypeDescriptor> types() const noexcept;
    bool isFinalized() const noexcept;

private:
    static bool mergeGroup(MergedTypeDescriptor& merged, std::string& error);

    std::vector<TypePartDescriptor> parts_;
    std::vector<MergedTypeDescriptor> types_;
    bool finalized_ = false;
};
}
