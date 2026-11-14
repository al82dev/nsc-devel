#pragma once

#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

namespace nativesharp::frontend
{
// Experimental frontend evidence only: it does not validate LLVM, artifact
// identity, dependency provenance or producer trust. Never consume as NoEscape.
struct ImplicitLifecycleEvidence final
{
    std::string typeIdentity;
    std::string constructorSymbol;
    std::string objectDestructionSymbol;
};

[[nodiscard]] inline std::optional<ImplicitLifecycleEvidence> inspectImplicitLifecycleEvidence(
    const ProjectCompilation& project, std::size_t typeOrdinal, std::size_t producingProjectOrdinal)
{
    const auto* names = project.nameLookup();
    const auto* members = project.memberModel();
    const auto* inheritance = project.inheritanceModel();
    const auto* construction = project.constructionModel();
    const auto* destruction = project.destructionModel();
    const auto* hir = project.oopHirModel();
    const auto* mir = project.oopMirModel();
    if (names == nullptr || members == nullptr || inheritance == nullptr || construction == nullptr ||
        destruction == nullptr || hir == nullptr || mir == nullptr || !names->isBuilt() ||
        !members->isBuilt() || !inheritance->isBuilt() || !construction->isBuilt() ||
        !destruction->isBuilt() || !mir->isBuilt() || typeOrdinal >= names->namedTypes().size())
        return std::nullopt;
    const auto& type = names->namedTypes()[typeOrdinal];
    if (type.kind != NamedTypeDeclarationKind::Class || type.isPrecompiled || type.isStatic ||
        type.isAbstract || type.isPartial || type.projectOrdinal != producingProjectOrdinal ||
        type.syntax == nullptr || !type.identity.isValid() || type.identity.arity() != 0 ||
        type.parts.empty() || inheritance->baseTypeOrdinal(typeOrdinal).has_value())
        return std::nullopt;
    for (const auto& part : type.parts)
        if (part.isPrecompiled || part.projectOrdinal != producingProjectOrdinal || part.syntax == nullptr)
            return std::nullopt;
    for (const std::size_t ordinal : members->membersOfType(typeOrdinal))
    {
        if (ordinal >= members->members().size()) return std::nullopt;
        const auto& member = members->members()[ordinal];
        if (member.kind == ClassMemberKind::Constructor || member.kind == ClassMemberKind::Destructor ||
            (!member.isStatic && (member.kind == ClassMemberKind::Field || member.kind == ClassMemberKind::Event)))
            return std::nullopt;
    }
    // LLVM inserts a type-initialization call into constructors even when the
    // HIR/MIR constructor body is empty. Only certify a root for which the
    // backend has no non-constant static initializer or static constructor.
    for (const auto& initializer : mir->fieldInitializers())
        if (initializer.declaringTypeOrdinal == typeOrdinal && initializer.isStatic && !initializer.isConst)
            return std::nullopt;
    for (const auto& function : mir->functions())
        if (function.declaringTypeOrdinal == typeOrdinal && function.constructor && function.isStatic)
            return std::nullopt;
    const auto* initial = construction->implicitDefaultConstructorBindingForType(typeOrdinal);
    const auto* drop = destruction->planForType(typeOrdinal);
    if (initial == nullptr || initial->typeOrdinal != typeOrdinal ||
        initial->resolution != ImplicitDefaultConstructorResolutionKind::NoBaseRequired ||
        initial->baseTypeOrdinal.has_value() || initial->directBaseConstructorMemberOrdinal.has_value() ||
        initial->entryConstructorMemberOrdinal.has_value() || drop == nullptr || drop->typeOrdinal != typeOrdinal ||
        drop->explicitDestructorMemberOrdinal.has_value() || !drop->destructorMemberOrdinals.empty() ||
        drop->objectDestructionEntryName.empty()) return std::nullopt;
    const ProjectOopHirFunction* hirCtor = nullptr;
    for (const auto& function : hir->functions())
        if (function.declaringTypeOrdinal == typeOrdinal && function.isSyntheticImplicitConstructor)
        {
            if (hirCtor != nullptr) return std::nullopt;
            hirCtor = &function;
        }
    if (hirCtor == nullptr || !hirCtor->constructor || hirCtor->body == nullptr ||
        hirCtor->body->kind != ProjectOopHirNodeKind::Block ||
        !hirCtor->body->children.empty() || hirCtor->mangledName.empty()) return std::nullopt;
    const ProjectOopMirFunction* mirCtor = nullptr;
    for (const auto& function : mir->functions())
        if (function.declaringTypeOrdinal == typeOrdinal && function.isSyntheticImplicitConstructor)
        {
            if (mirCtor != nullptr) return std::nullopt;
            mirCtor = &function;
        }
    if (mirCtor == nullptr || !mirCtor->constructor || mirCtor->body == nullptr ||
        mirCtor->body->kind != ProjectOopMirNodeKind::Block ||
        !mirCtor->body->children.empty() || !mirCtor->cleanupRegions.empty() ||
        mirCtor->mangledName != hirCtor->mangledName) return std::nullopt;
    const ProjectOopMirDispatchTable* table = nullptr;
    for (const auto& candidate : mir->dispatchTables())
        if (candidate.typeOrdinal == typeOrdinal && candidate.genericSpecializationKey.empty())
        {
            if (table != nullptr) return std::nullopt;
            table = &candidate;
        }
    if (table == nullptr || table->isImported || table->baseTypeOrdinal.has_value() ||
        table->delegatedBaseDestructionTypeOrdinal.has_value() || !table->destructionSteps.empty() ||
        !table->ownedFields.empty() || !table->destructorMemberOrdinals.empty() ||
        table->typeKey != "type:" + type.identity.key() ||
        table->objectDestructionEntryName != drop->objectDestructionEntryName) return std::nullopt;
    return ImplicitLifecycleEvidence{type.identity.key(), mirCtor->mangledName,
        table->objectDestructionEntryName};
}
}
