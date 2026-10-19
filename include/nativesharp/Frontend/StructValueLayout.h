#pragma once

#include "nativesharp/Frontend/ObjectFieldLayout.h"

#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess;

struct StructValueLayout final
{
    std::size_t typeOrdinal = 0;
    std::string typeKey;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    // C2.2 "union and explicit layout": what the declaration asked for, carried alongside the
    // computed result so consumers that must REPRODUCE the layout in another language -- the C
    // header generator above all -- can emit `union` / `#pragma pack(N)` / an alignment attribute
    // instead of guessing from offsets that happen to coincide.
    bool isUnion = false;
    std::uint64_t pack = 0;
    std::uint64_t minimumAlignment = 0;
    bool hasExplicitFieldOffsets = false;
    std::vector<ObjectFieldLayout> fields;
};
}
