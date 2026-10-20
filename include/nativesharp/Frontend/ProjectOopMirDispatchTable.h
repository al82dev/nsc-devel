#pragma once

#include "nativesharp/Language/ErrorSemantics.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace nativesharp::frontend
{
struct ProjectOopMirDispatchTable final
{
    // Imported tables describe producer-owned runtime data. They participate in semantic slot
    // selection, but the consumer must neither define nor initialize their globals locally.
    bool isImported = false;
    std::string genericSpecializationKey;
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::string typeKey;
    std::string descriptorName;
    std::string descriptorAccessorName;
    std::string vtableName;
    std::uint64_t stableTypeId = 0;
    // C8 "type root": the type's own name, as source wrote it -- the string
    // NativeSharp.Core.Object.ToString() hands back. It goes into the class descriptor as a
    // pointer to a length-prefixed constant with exactly the layout of a string literal, so
    // returning it costs a load and allocates nothing.
    std::string typeDisplayName;
    std::string typeNameGlobalName;
    std::vector<std::size_t> slotMemberOrdinals;
    std::vector<std::string> slotCalleeNames;
    std::string destructionEntryName;
    std::string objectDestructionEntryName;
    std::vector<std::size_t> destructorMemberOrdinals;
    std::vector<std::string> destructorCalleeNames;
    // A local class may derive from a class whose destructor bodies and owned-field layout live
    // exclusively in a precompiled producer. After completing every local destruction step, the
    // local entry delegates the remaining base chain through that type's runtime descriptor.
    // The delegated entry also releases the allocation, so the local entry must not free it again.
    std::optional<std::size_t> delegatedBaseDestructionTypeOrdinal;
    // Bloque 1 item 1 (owning field storage) -- instance fields declared `owned` whose type really
    // owns a separate allocation, INCLUDING the ones inherited from base types, in the order they
    // must be destroyed. The destruction entry walks this list after running the declared
    // destructor bodies, so a class that owns something releases it without the author writing a
    // `delete` by hand. Empty for every type that declares no owned field, which is why nothing
    // changes for code that does not use the feature.
    //
    // The byte offset is resolved HERE rather than in the LLVM layer, matching how every other
    // field access in this pipeline works (the backend consumes offsets the frontend computed, and
    // never reaches into the object layout model itself).
    struct OwnedField;
    std::vector<OwnedField> ownedFields;

    // Bloque 1 (audit finding "orden de destruccion debe fijarse por contrato") -- the SINGLE,
    // ordered sequence the destruction entry executes. This is the language's destruction-order
    // contract, frozen here and verified by execution
    // (Stage50_11OwnershipExecutionTests: DestructionOrderContract*):
    //
    //     most-derived destructor body
    //     most-derived class's owned fields, in REVERSE declaration order
    //     base destructor body
    //     base class's owned fields, in REVERSE declaration order
    //     ... up the chain
    //
    // -- i.e. destruction is the exact reverse of construction, the same rule scoped owners
    // already follow (NestedScopesDestroyInReverseOrder) and the rule C++ fixes for member
    // subobjects. Two properties fall out of it and are load-bearing:
    //
    //   * a destructor body always runs while ITS OWN class's owned fields are still alive (it
    //     may read them; the null-guarded release right after it skips anything it discharged by
    //     hand), and
    //   * a derived class's fields are released before ANY base state goes away, so a derived
    //     field's destructor may still rely on base fields -- never the other way around.
    //
    // HISTORY: before this contract existed, the entry ran ALL destructor bodies (derived->base)
    // and THEN released ALL owned fields base-first in declaration order -- measured, not read:
    // ~Derived, ~Base, base fields, derived fields. That inverted both properties above (base
    // fields died before derived fields that might reference them, and every base body ran while
    // no field had been released yet). Nothing could have depended on the old order -- it was
    // undocumented and untested -- which is exactly why the audit required freezing one now.
    //
    // Exactly one of the two payloads is meaningful per step: isFieldRelease selects between a
    // body call (destructorCalleeName) and a null-guarded slot release (ownedField).
    struct DestructionStep;
    std::vector<DestructionStep> destructionSteps;
    struct InterfaceTable;
    std::string interfaceMapName;
    std::vector<InterfaceTable> interfaceTables;
};
}

#include "nativesharp/Frontend/ProjectOopMirDispatchTableOwnedField.h"
#include "nativesharp/Frontend/ProjectOopMirDispatchTableDestructionStep.h"
#include "nativesharp/Frontend/ProjectOopMirDispatchTableInterfaceTable.h"
