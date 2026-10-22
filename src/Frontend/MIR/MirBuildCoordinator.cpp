#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectCompilationIndexes.h"

#include "MirConstantEvaluator.h"
#include "../Cache/CachePayloadReader.h"
#include "MirVerifier.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Language/CoroutineSemantics.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <string_view>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
std::string sanitizeDispatchName(std::string_view value)
{
    return compilation::symbolFragmentForIdentityKey(value);
}

std::uint64_t stableTypeId(std::string_view value) noexcept
{
    // FNV-1a 64-bit. This is an ABI-visible deterministic identity seed, not reflection.
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : value)
    {
        const auto byte = static_cast<unsigned char>(character);
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string genericSpecializationKeyForType(
    const ProjectNameLookup& nameLookup,
    std::size_t typeOrdinal)
{
    if (typeOrdinal >= nameLookup.namedTypes().size()) return {};
    constexpr std::string_view kSpecializationPrefix = "stage44-specialization:";
    const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
    const auto keyForSyntax = [&](const ast::AstNode* syntax) -> std::string {
        if (syntax == nullptr || !syntax->auxiliaryText().starts_with(kSpecializationPrefix))
            return {};
        std::string key = syntax->auxiliaryText().substr(kSpecializationPrefix.size());
        if (const std::size_t metadata = key.find(";nsc-meta:"); metadata != std::string::npos)
            key.resize(metadata);
        return key;
    };
    if (std::string key = keyForSyntax(type.syntax); !key.empty()) return key;
    for (const NamedTypeDeclarationPart& part : type.parts)
    {
        if (std::string key = keyForSyntax(part.syntax); !key.empty()) return key;
    }
    // Name aggregation can select the open template as the declaration's primary syntax even
    // though this ordinal names the generated closed type.  The generated name is itself a
    // stable compiler contract: __g<arity>__<arguments>__h<16 hex>.  Treat it as reusable when
    // the AST marker is no longer reachable from the aggregate declaration.
    const std::string& name = type.identity.fullyQualifiedName();
    const std::size_t generic = name.rfind("__g");
    const std::size_t hash = name.rfind("__h");
    if (generic != std::string::npos && hash != std::string::npos && hash > generic + 3U &&
        hash + 3U + 16U == name.size())
    {
        bool validHash = true;
        for (const char character : std::string_view(name).substr(hash + 3U))
            validHash = validHash && std::isxdigit(static_cast<unsigned char>(character));
        if (validHash) return "type:" + type.identity.key();
    }
    return {};
}

}

ProjectOopMirModel::ProjectOopMirModel(ProjectCompilationIndexes& indexes) noexcept
    : indexes_(&indexes)
{
}

bool ProjectOopMirModel::initializeBuild(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectVirtualDispatchModel& virtualDispatchModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    std::string& error)
{
    error.clear();
    if (built_)
    {
        error = "ProjectOopMirModel instances are single-use";
        return false;
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !objectLayoutModel.isBuilt() ||
        !virtualDispatchModel.isBuilt() || !interfaceModel.isBuilt() ||
        !destructionModel.isBuilt() || !hirModel.isBuilt())
    {
        error = "Stage 34 OOP MIR requires built OOP HIR and Stage 29 object layouts";
        return false;
    }
    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    objectLayoutModel_ = &objectLayoutModel;
    virtualDispatchModel_ = &virtualDispatchModel;
    interfaceModel_ = &interfaceModel;
    destructionModel_ = &destructionModel;
    hirModel_ = &hirModel;
    if (!importLayouts(nameLookup, memberModel, objectLayoutModel, hirModel, error)) return false;
    functionByMember_.resize(memberModel.members().size());
    for (const ProjectOopHirFunction& function : hirModel.functions())
    {
        if (function.namespaceFunctionOrdinal.has_value() &&
            *function.namespaceFunctionOrdinal >= functionByNamespaceFunction_.size())
            functionByNamespaceFunction_.resize(*function.namespaceFunctionOrdinal + 1U);
    }
    dispatchTableByType_.resize(nameLookup.namedTypes().size());
    return true;
}

bool ProjectOopMirModel::appendDispatchSlots(
    const ClassVirtualTable& sourceTable,
    const ProjectOopHirModel& hirModel,
    ProjectOopMirDispatchTable& table,
    std::string& error)
{
    table.slotMemberOrdinals = sourceTable.slotMemberOrdinals;
    for (std::size_t slot = 0; slot < sourceTable.slotMemberOrdinals.size(); ++slot)
    {
        const std::size_t slotMember = sourceTable.slotMemberOrdinals[slot];
        const bool abstractSlot = slot < sourceTable.slotAbstract.size() && sourceTable.slotAbstract[slot];
        if (abstractSlot)
        {
            table.slotCalleeNames.emplace_back();
            continue;
        }
        const ProjectOopHirFunction* target = hirModel.functionForMember(slotMember);
        if (target == nullptr)
        {
            error = "Stage 35 concrete vtable slot refers to a method without OOP HIR body";
            return false;
        }
        table.slotCalleeNames.push_back(target->mangledName);
    }
    return true;
}

bool ProjectOopMirModel::appendDispatchDestructors(
    const ProjectMemberModel& memberModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    ProjectOopMirDispatchTable& table,
    std::string& error)
{
    const ClassDestructionPlan* destruction = destructionModel.planForType(table.typeOrdinal);
    if (destruction == nullptr)
    {
        error = "Stage 35 dispatch table has no class destruction plan";
        return false;
    }
    table.destructionEntryName = destruction->destructionEntryName;
    table.objectDestructionEntryName = destruction->objectDestructionEntryName;
    table.destructorMemberOrdinals = destruction->destructorMemberOrdinals;
    if (table.isImported) return true;
    for (const std::size_t destructorMember : destruction->destructorMemberOrdinals)
    {
        if (destructorMember < memberModel.members().size() &&
            memberModel.members()[destructorMember].isPrecompiled)
            continue;
        const ProjectOopHirFunction* destructorBody = hirModel.functionForMember(destructorMember);
        if (destructorBody == nullptr || !destructorBody->destructor)
        {
            error = "Stage 35 destruction plan refers to a destructor without OOP HIR body";
            return false;
        }
        table.destructorCalleeNames.push_back(destructorBody->mangledName);
    }
    return true;
}

bool ProjectOopMirModel::appendDispatchInterfaces(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectOopHirModel& hirModel,
    std::string_view suffix,
    ProjectOopMirDispatchTable& table,
    std::string& error)
{
    for (const std::size_t interfaceOrdinal : interfaceModel.interfacesOfClass(table.typeOrdinal))
    {
        const ClassInterfaceTablePlan* plan = interfaceModel.tablePlan(table.typeOrdinal, interfaceOrdinal);
        if (plan == nullptr)
        {
            error = "Stage 36 class interface map has no semantic table plan";
            return false;
        }
        ProjectOopMirDispatchTable::InterfaceTable interfaceTable;
        interfaceTable.interfaceTypeOrdinal = interfaceOrdinal;
        interfaceTable.interfaceTypeKey = interfaceModel.typeKeyForOrdinal(interfaceOrdinal);
        interfaceTable.stableInterfaceTypeId = plan->stableInterfaceTypeId;
        interfaceTable.methodTableName = "__nsc_iftable_" + std::string(suffix) + "__" +
            sanitizeDispatchName(nameLookup.namedTypes()[interfaceOrdinal].identity.key());
        interfaceTable.interfaceMemberOrdinals = plan->interfaceMethodMemberOrdinals;
        for (const std::optional<std::size_t> implementation : plan->implementationMemberOrdinals)
        {
            if (!implementation.has_value())
            {
                interfaceTable.implementationMemberOrdinals.push_back(memberModel.members().size());
                interfaceTable.calleeNames.emplace_back();
                continue;
            }
            interfaceTable.implementationMemberOrdinals.push_back(*implementation);
            const ProjectOopHirFunction* implementationFunction =
                hirModel.functionForMember(*implementation);
            if (implementationFunction == nullptr)
            {
                error = "Stage 36 concrete interface implementation has no OOP HIR function";
                return false;
            }
            interfaceTable.calleeNames.push_back(implementationFunction->mangledName);
        }
        table.interfaceTables.push_back(std::move(interfaceTable));
    }
    return true;
}

bool ProjectOopMirModel::appendDispatchTable(
    const ClassVirtualTable& sourceTable,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    std::string& error)
{
    if (sourceTable.typeOrdinal >= nameLookup.namedTypes().size())
    {
        error = "Macro A virtual table refers to an invalid class ordinal";
        return false;
    }
    ProjectOopMirDispatchTable table;
    table.isImported = nameLookup.namedTypes()[sourceTable.typeOrdinal].isPrecompiled;
    table.genericSpecializationKey = genericSpecializationKeyForType(nameLookup, sourceTable.typeOrdinal);
    table.typeOrdinal = sourceTable.typeOrdinal;
    table.baseTypeOrdinal = sourceTable.baseTypeOrdinal;
    table.typeKey = "type:" + nameLookup.namedTypes()[sourceTable.typeOrdinal].identity.key();
    const std::string suffix = sanitizeDispatchName(
        nameLookup.namedTypes()[sourceTable.typeOrdinal].identity.key());
    table.descriptorName = "__nsc_classdesc_" + suffix;
    table.descriptorAccessorName = "__nsc_get_classdesc_" + suffix;
    table.vtableName = "__nsc_vtable_" + suffix;
    table.interfaceMapName = "__nsc_ifmap_" + suffix;
    table.stableTypeId = stableTypeId(table.typeKey);
    // C8: the SHORT name, not the fully-qualified key -- `Dictionary`, not
    // `Proj::NativeSharp.Core.Collections.Generic.Dictionary`. That is what a ToString()
    // default is useful for, and it matches what real C#'s GetType().Name gives.
    {
        const std::string& identity = nameLookup.namedTypes()[sourceTable.typeOrdinal].identity.key();
        const std::size_t lastDot = identity.find_last_of(".:");
        table.typeDisplayName = lastDot == std::string::npos
            ? identity : identity.substr(lastDot + 1);
    }
    table.typeNameGlobalName = "__nsc_typename_" + suffix;
    if (!appendDispatchSlots(sourceTable, hirModel, table, error) ||
        !appendDispatchDestructors(memberModel, destructionModel, hirModel, table, error) ||
        !appendDispatchInterfaces(
            nameLookup, memberModel, interfaceModel, hirModel, suffix, table, error))
        return false;
    dispatchTableByType_[table.typeOrdinal] = dispatchTables_.size();
    dispatchTables_.push_back(std::move(table));
    return true;
}

bool ProjectOopMirModel::buildDispatchTables(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectVirtualDispatchModel& virtualDispatchModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    std::string& error)
{
    for (const ClassVirtualTable& sourceTable : virtualDispatchModel.classTables())
    {
        if (!appendDispatchTable(
                sourceTable, nameLookup, memberModel, interfaceModel,
                destructionModel, hirModel, error))
            return false;
    }
    return true;
}

bool ProjectOopMirModel::appendOwnedFieldDestructionSteps(
    ProjectOopMirDispatchTable& table,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    const std::unordered_set<std::string>& destroyableClassTypeKeys,
    std::string& error)
{
    // The producer already emitted this complete plan behind destructionEntryName.
    // Rebuilding it would require importing private destructor bodies and field layout.
    if (table.isImported) return true;
    // The chain is walked MOST-DERIVED FIRST, and within each class the owned fields are
    // taken in REVERSE declaration order -- destruction is the exact reverse of
    // construction. This is the frozen order contract; the full statement and its
    // rationale live on ProjectOopMirDispatchTable::destructionSteps.
    std::vector<std::size_t> chain;
    for (std::optional<std::size_t> current = table.typeOrdinal; current.has_value();)
    {
        chain.push_back(*current);
        const std::optional<std::size_t> tableIndex = *current < dispatchTableByType_.size()
            ? dispatchTableByType_[*current] : std::nullopt;
        current = tableIndex.has_value()
            ? dispatchTables_[*tableIndex].baseTypeOrdinal : std::nullopt;
    }
    for (const std::size_t chainTypeOrdinal : chain)
    {
        // The precompiled producer owns the complete remainder of this base chain,
        // including private destructor bodies and private owned-field layout. Calling its
        // descriptor's destruction entry is the only complete operation available to a
        // consumer, and it is terminal because that entry also releases the allocation.
        if (chainTypeOrdinal < nameLookup.namedTypes().size() &&
            nameLookup.namedTypes()[chainTypeOrdinal].isPrecompiled)
        {
            table.delegatedBaseDestructionTypeOrdinal = chainTypeOrdinal;
            break;
        }
        // This class's destructor body first: it runs while every one of its own owned
        // fields is still alive, and the null-guarded releases right after it skip
        // anything it already discharged by hand.
        const ClassDestructionPlan* chainPlan = destructionModel.planForType(chainTypeOrdinal);
        if (chainPlan != nullptr && chainPlan->explicitDestructorMemberOrdinal.has_value())
        {
            const ProjectOopHirFunction* body =
                hirModel.functionForMember(*chainPlan->explicitDestructorMemberOrdinal);
            if (body == nullptr || !body->destructor)
            {
                error = "Bloque 1 destruction order refers to a destructor without OOP HIR body";
                return false;
            }
            ProjectOopMirDispatchTable::DestructionStep bodyStep;
            bodyStep.destructorCalleeName = body->mangledName;
            table.destructionSteps.push_back(std::move(bodyStep));
        }
        const std::span<const std::size_t> classMembers =
            memberModel.membersOfType(chainTypeOrdinal);
        for (auto memberIt = classMembers.rbegin(); memberIt != classMembers.rend(); ++memberIt)
        {
            const std::size_t memberOrdinal = *memberIt;
            const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
            if (member.kind != ClassMemberKind::Field || member.isStatic) continue;
            if (member.declaredOwnership != sema::OwnershipKind::Owned) continue;
            if (member.declaredTypeKey != "builtin:string" &&
                !destroyableClassTypeKeys.contains(member.declaredTypeKey))
                continue;
            const ObjectFieldLayout* layout = objectLayoutModel.fieldLayoutForMember(memberOrdinal);
            if (layout == nullptr)
            {
                error = "Bloque 1 owned field has no object layout";
                return false;
            }
            const ProjectOopMirDispatchTable::OwnedField ownedField{
                memberOrdinal, layout->offset, member.declaredTypeKey == "builtin:string"};
            table.ownedFields.push_back(ownedField);
            ProjectOopMirDispatchTable::DestructionStep fieldStep;
            fieldStep.ownedField = ownedField;
            fieldStep.isFieldRelease = true;
            table.destructionSteps.push_back(std::move(fieldStep));
        }
    }
    return true;
}

bool ProjectOopMirModel::buildOwnedFieldDestructionPlans(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    std::string& error)
{
    // Bloque 1 item 1 (owning field storage) -- collect, per class, the instance fields it OWNS, so
    // the destruction entry can release them without the author writing a `delete` by hand. Done as
    // a pass of its own AFTER every dispatch table exists, because deciding whether a field's type
    // is destroyable means asking whether that type has a class descriptor at all -- and that is
    // exactly the set of types that got a table above.
    //
    // Only fields whose type genuinely owns a separate allocation are listed. A value-typed `owned`
    // field owns no heap block for the traversal to release, which is the same rule already applied
    // to `owned` PARAMETERS (requiresOwnedDestruction) and to `owned` LOCALS
    // (ProjectMethodBodyModel) -- the three spellings agree on purpose.
    std::unordered_set<std::string> destroyableClassTypeKeys;
    for (const ProjectOopMirDispatchTable& built : dispatchTables_)
        destroyableClassTypeKeys.insert(built.typeKey);
    for (ProjectOopMirDispatchTable& built : dispatchTables_)
    {
        if (!appendOwnedFieldDestructionSteps(
                built, nameLookup, memberModel, objectLayoutModel, destructionModel,
                hirModel, destroyableClassTypeKeys, error))
            return false;
    }
    return true;
}

bool ProjectOopMirModel::lowerFieldInitializers(
    const ProjectOopHirModel& hirModel,
    std::string& error)
{
    for (const ProjectOopHirFieldInitializer& source : hirModel.fieldInitializers())
    {
        auto value = lowerNode(source.value.get(), error);
        if (value == nullptr)
        {
            // Same reason as the function body above: a field initializer that fails to lower
            // otherwise reports nothing at all about which field it was.
            error += "\n  while lowering the field initializer of member #" +
                std::to_string(source.memberOrdinal);
            return false;
        }
        fieldInitializers_.push_back(ProjectOopMirFieldInitializer{
            source.memberOrdinal,
            source.declaringTypeOrdinal,
            source.isStatic,
            source.isConst,
            std::move(value)});
    }
    return true;
}

bool ProjectOopMirModel::materializeStaticFieldConstant(
    std::size_t memberOrdinal,
    const ClassMemberSymbol& field,
    const ProjectOopHirModel& hirModel,
    ProjectOopMirStaticField& staticField,
    std::string& error)
{
    for (const ProjectOopHirFieldInitializer& initializer : hirModel.fieldInitializers())
    {
        if (initializer.memberOrdinal != memberOrdinal || initializer.value == nullptr) continue;
        // Stage 51.x: an enum member's declaredTypeKey is its own enum type (see the
        // comment in ProjectMemberModel::collectEnumMembers), so isIntegral() alone
        // would miss it here -- it is still an int-sized compile-time constant.
        if (ProjectTypeConversion::isIntegral(field.declaredTypeKey) || field.isEnumConstant)
        {
            const std::optional<std::uint64_t> value =
                MirConstantEvaluator::integerValue(initializer.value.get(), staticFields_);
            if (value.has_value())
            {
                staticField.hasConstantInitializer = true;
                staticField.constantIntegerValue = *value;
            }
        }
        else if (field.declaredTypeKey == "builtin:bool")
        {
            const std::optional<bool> value =
                MirConstantEvaluator::booleanValue(initializer.value.get(), staticFields_);
            if (value.has_value())
            {
                staticField.hasConstantInitializer = true;
                staticField.constantBooleanValue = *value;
            }
        }
        else if (field.declaredTypeKey == "builtin:float" ||
                 field.declaredTypeKey == "builtin:double" ||
                 field.declaredTypeKey == "builtin:decimal")
        {
            const std::optional<double> value =
                MirConstantEvaluator::floatingValue(initializer.value.get(), staticFields_);
            if (value.has_value())
            {
                staticField.hasConstantInitializer = true;
                staticField.constantFloatingValue = *value;
            }
        }
        else if (field.declaredTypeKey == "builtin:string")
        {
            const std::optional<std::string> value =
                MirConstantEvaluator::stringValue(initializer.value.get(), staticFields_);
            if (value.has_value())
            {
                staticField.hasConstantInitializer = true;
                staticField.constantStringValue = *value;
            }
        }
        break;
    }
    if (!staticField.hasConstantInitializer)
    {
        error =
            "Stage 37 const initializer validated semantically but could not be "
            "materialized as a native constant";
        return false;
    }
    return true;
}

bool ProjectOopMirModel::appendStaticField(
    std::size_t memberOrdinal,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectOopHirModel& hirModel,
    std::string& error)
{
    const ClassMemberSymbol& field = memberModel.members()[memberOrdinal];
    ProjectOopMirStaticField staticField;
    staticField.memberOrdinal = memberOrdinal;
    staticField.declaringTypeOrdinal = field.declaringTypeOrdinal;
    staticField.name = field.name;
    staticField.typeKey = field.declaredTypeKey;
    staticField.isReadonly = field.isReadonly;
    staticField.isVolatile = field.isVolatile;
    staticField.isConst = field.isConst;
    staticField.isImported = field.isPrecompiled;
    staticField.genericSpecializationKey =
        genericSpecializationKeyForType(nameLookup, field.declaringTypeOrdinal);
    if (field.declaringTypeOrdinal >= nameLookup.namedTypes().size())
    {
        error = "Stage 37 static field has an invalid declaring type ordinal";
        return false;
    }
    const std::string typeFragment = compilation::symbolFragmentForIdentityKey(
        nameLookup.namedTypes()[field.declaringTypeOrdinal].identity.key());
    const std::string fieldFragment = compilation::symbolFragmentForIdentityKey(field.name);
    staticField.storageSymbol = "__nsc_static_" + typeFragment + "__" + fieldFragment;
    staticField.accessorName = "__nsc_get_static_" + typeFragment + "__" + fieldFragment;
    staticField.sourceRange = field.range;
    staticField.debugDisplayName =
        nameLookup.namedTypes()[field.declaringTypeOrdinal].identity.fullyQualifiedName() +
        "." + field.name;
    staticField.isEnumConstant = field.isEnumConstant;
    if (field.isConst &&
        !materializeStaticFieldConstant(
            memberOrdinal, field, hirModel, staticField, error))
        return false;
    staticFields_.push_back(std::move(staticField));
    return true;
}

bool ProjectOopMirModel::buildStaticFields(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectOopHirModel& hirModel,
    std::string& error)
{
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& field = memberModel.members()[memberOrdinal];
        if (field.kind == ClassMemberKind::Field && field.isStatic &&
            !appendStaticField(memberOrdinal, nameLookup, memberModel, hirModel, error))
            return false;
    }
    return true;
}

void ProjectOopMirModel::buildNamespaceStateFields(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel)
{
    const std::size_t namespaceStateMemberBase = memberModel.members().size();
    for (std::size_t stateOrdinal = 0;
         stateOrdinal < namespaceFunctionModel.states().size(); ++stateOrdinal)
    {
        const NamespaceStateSymbol& state = namespaceFunctionModel.states()[stateOrdinal];
        ProjectOopMirStaticField staticField;
        staticField.memberOrdinal = namespaceStateMemberBase + stateOrdinal;
        staticField.declaringTypeOrdinal = nameLookup.namedTypes().size();
        staticField.name = state.name;
        staticField.typeKey = state.declaredTypeKey;
        staticField.isReadonly = state.isConst;
        staticField.isConst = state.isConst;
        staticField.isImported = state.isPrecompiled;
        staticField.storageSymbol = state.storageName;
        staticField.accessorName = "__nsc_get_" + state.storageName;
        staticField.namespaceStateOrdinal = stateOrdinal;
        staticField.sourceRange = state.range;
        staticField.debugDisplayName = state.namespaceName.empty()
            ? state.name
            : state.namespaceName + "." + state.name;
        switch (state.initializerKind)
        {
            case NamespaceStateInitializerKind::Integer:
                staticField.hasConstantInitializer = true;
                staticField.constantIntegerValue = state.integerValue;
                break;
            case NamespaceStateInitializerKind::Floating:
                staticField.hasConstantInitializer = true;
                staticField.constantFloatingValue = state.floatingValue;
                break;
            case NamespaceStateInitializerKind::Boolean:
                staticField.hasConstantInitializer = true;
                staticField.constantBooleanValue = state.booleanValue;
                break;
            case NamespaceStateInitializerKind::String:
                staticField.hasConstantInitializer = true;
                staticField.constantStringValue = state.stringValue;
                break;
            case NamespaceStateInitializerKind::Null:
                staticField.hasConstantInitializer = true;
                break;
            case NamespaceStateInitializerKind::Zero:
                if (state.isConst) staticField.hasConstantInitializer = true;
                break;
        }
        staticFields_.push_back(std::move(staticField));
    }
}

bool ProjectOopMirModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectVirtualDispatchModel& virtualDispatchModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    std::string& error,
    std::size_t workerCount)
{
    driver::CompilationWorkerPool workerPool(workerCount);
    return build(
        nameLookup, memberModel, objectLayoutModel, virtualDispatchModel,
        interfaceModel, destructionModel, hirModel, namespaceFunctionModel,
        error, workerCount, &workerPool);
}

bool ProjectOopMirModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectVirtualDispatchModel& virtualDispatchModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    std::string& error,
    std::size_t workerCount,
    driver::CompilationWorkerPool* workerPool)
{
    if (!initializeBuild(
            nameLookup,
            memberModel,
            objectLayoutModel,
            virtualDispatchModel,
            interfaceModel,
            destructionModel,
            hirModel,
            error))
        return false;
    if (!buildDispatchTables(
            nameLookup,
            memberModel,
            virtualDispatchModel,
            interfaceModel,
            destructionModel,
            hirModel,
            error))
        return false;
    if (!buildOwnedFieldDestructionPlans(
            nameLookup,
            memberModel,
            objectLayoutModel,
            destructionModel,
            hirModel,
            error))
        return false;
    calculateMirCacheKeys(hirModel);
    lookupMirCachePayloads();
    if (!lowerFunctions(
            nameLookup,
            memberModel,
            objectLayoutModel,
            virtualDispatchModel,
            interfaceModel,
            destructionModel,
            hirModel,
            error,
            workerCount,
            workerPool))
    {
        return false;
    }

    if (!lowerFieldInitializers(hirModel, error)) return false;
    if (!buildStaticFields(nameLookup, memberModel, hirModel, error)) return false;
    buildNamespaceStateFields(nameLookup, memberModel, namespaceFunctionModel);
    MirVerifier verifier(*this);
    if (!verifier.finalize(error)) return false;

    if (indexes_ != nullptr)
    {
        indexes_->rebuildMirFunctions(
            functions_, memberModel.members().size(), functionByNamespaceFunction_.size());
        indexes_->rebuildMirLayouts(
            structLayouts_, classLayouts_, nameLookup.namedTypes().size());
        indexes_->rebuildMirInterfaces(interfaceBindings_);
    }
    built_ = true;
    // NSC305-C04. Persist the built MIR functions (no-op when the cache is disabled).
    storeMirFunctions();
    return true;
}

void ProjectOopMirModel::setPanicProfile(language::PanicProfile profile) noexcept
{
    panicProfile_ = profile;
}

language::PanicProfile ProjectOopMirModel::panicProfile() const noexcept
{
    return panicProfile_;
}

void ProjectOopMirModel::setPanicHandler(std::string handler)
{
    panicHandler_ = std::move(handler);
}

std::string_view ProjectOopMirModel::panicHandler() const noexcept
{
    return panicHandler_;
}

std::string_view ProjectOopMirModel::panicHandlerMangledName() const noexcept
{
    return panicHandlerMangledName_;
}

std::span<const ProjectOopMirFunction> ProjectOopMirModel::functions() const noexcept
{
    return functions_;
}

std::span<const ProjectOopMirFieldInitializer> ProjectOopMirModel::fieldInitializers() const noexcept
{
    return fieldInitializers_;
}

std::span<const ProjectOopMirStaticField> ProjectOopMirModel::staticFields() const noexcept
{
    return staticFields_;
}

std::span<const ProjectOopMirStructLayout> ProjectOopMirModel::structLayouts() const noexcept
{
    return structLayouts_;
}

std::span<const ProjectOopMirClassLayout> ProjectOopMirModel::classLayouts() const noexcept
{
    return classLayouts_;
}

const ProjectOopMirStructLayout* ProjectOopMirModel::structLayoutForType(
    std::size_t typeOrdinal) const noexcept
{
    if (built_ && indexes_ != nullptr) return indexes_->mirStructLayoutForType(typeOrdinal);
    for (const ProjectOopMirStructLayout& layout : structLayouts_)
        if (layout.typeOrdinal == typeOrdinal) return &layout;
    return nullptr;
}

const ProjectOopMirStructLayout* ProjectOopMirModel::structLayoutForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (built_ && indexes_ != nullptr) return indexes_->mirStructLayoutForTypeKey(typeKey);
    for (const ProjectOopMirStructLayout& layout : structLayouts_)
        if (layout.typeKey == typeKey) return &layout;
    return nullptr;
}

const ProjectOopMirClassLayout* ProjectOopMirModel::classLayoutForType(
    std::size_t typeOrdinal) const noexcept
{
    if (built_ && indexes_ != nullptr) return indexes_->mirClassLayoutForType(typeOrdinal);
    for (const ProjectOopMirClassLayout& layout : classLayouts_)
        if (layout.typeOrdinal == typeOrdinal) return &layout;
    return nullptr;
}

const ProjectOopMirClassLayout* ProjectOopMirModel::classLayoutForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (built_ && indexes_ != nullptr) return indexes_->mirClassLayoutForTypeKey(typeKey);
    for (const ProjectOopMirClassLayout& layout : classLayouts_)
        if (layout.typeKey == typeKey) return &layout;
    return nullptr;
}

std::span<const ProjectOopMirDispatchTable> ProjectOopMirModel::dispatchTables() const noexcept
{
    return dispatchTables_;
}

std::span<const ProjectOopMirGenericSpecialization>
ProjectOopMirModel::genericSpecializations() const noexcept
{
    return genericSpecializations_;
}

std::span<const ProjectOopMirInterfaceBinding> ProjectOopMirModel::interfaceBindings() const noexcept
{
    return interfaceBindings_;
}

const ProjectOopMirDispatchTable* ProjectOopMirModel::dispatchTableForType(std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= dispatchTableByType_.size() || !dispatchTableByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *dispatchTableByType_[typeOrdinal];
    return index < dispatchTables_.size() ? &dispatchTables_[index] : nullptr;
}

const ProjectOopMirInterfaceBinding* ProjectOopMirModel::interfaceBindingForTypes(
    std::size_t implementingTypeOrdinal,
    std::size_t interfaceTypeOrdinal) const noexcept
{
    if (built_ && indexes_ != nullptr)
    {
        return indexes_->mirInterfaceBindingForTypes(
            implementingTypeOrdinal, interfaceTypeOrdinal);
    }
    const auto found = std::find_if(interfaceBindings_.begin(), interfaceBindings_.end(),
        [implementingTypeOrdinal, interfaceTypeOrdinal](const ProjectOopMirInterfaceBinding& binding) {
            return binding.implementingTypeOrdinal == implementingTypeOrdinal &&
                binding.interfaceTypeOrdinal == interfaceTypeOrdinal;
        });
    return found == interfaceBindings_.end() ? nullptr : &*found;
}

bool ProjectOopMirModel::interfaceSlotForMember(
    std::size_t interfaceMemberOrdinal,
    std::uint64_t& stableInterfaceTypeId,
    std::size_t& slot) const noexcept
{
    if (built_ && indexes_ != nullptr)
    {
        return indexes_->mirInterfaceSlotForMember(
            interfaceMemberOrdinal, stableInterfaceTypeId, slot);
    }
    for (const ProjectOopMirInterfaceBinding& binding : interfaceBindings_)
    {
        const auto found = std::find(binding.interfaceMemberOrdinals.begin(),
            binding.interfaceMemberOrdinals.end(), interfaceMemberOrdinal);
        if (found == binding.interfaceMemberOrdinals.end()) continue;
        stableInterfaceTypeId = binding.stableInterfaceTypeId;
        slot = static_cast<std::size_t>(found - binding.interfaceMemberOrdinals.begin());
        return true;
    }
    return false;
}

const ProjectOopMirFunction* ProjectOopMirModel::functionForMember(std::size_t memberOrdinal) const noexcept
{
    if (built_ && indexes_ != nullptr) return indexes_->mirFunctionForMember(memberOrdinal);
    if (memberOrdinal >= functionByMember_.size() || !functionByMember_[memberOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByMember_[memberOrdinal]];
}

const ProjectOopMirFunction* ProjectOopMirModel::functionForNamespaceFunction(
    std::size_t functionOrdinal) const noexcept
{
    if (built_ && indexes_ != nullptr)
        return indexes_->mirFunctionForNamespaceFunction(functionOrdinal);
    if (functionOrdinal >= functionByNamespaceFunction_.size() ||
        !functionByNamespaceFunction_[functionOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByNamespaceFunction_[functionOrdinal]];
}

const ProjectOopMirFunction* ProjectOopMirModel::functionForMangledName(
    std::string_view mangledName) const noexcept
{
    if (built_ && indexes_ != nullptr)
        return indexes_->mirFunctionForMangledName(mangledName);
    for (const ProjectOopMirFunction& function : functions_)
        if (function.mangledName == mangledName) return &function;
    return nullptr;
}

const ProjectOopMirFunction* ProjectOopMirModel::functionForLambda(
    std::size_t lambdaOrdinal) const noexcept
{
    if (built_ && indexes_ != nullptr)
        return indexes_->mirFunctionForLambda(lambdaOrdinal);
    for (const ProjectOopMirFunction& function : functions_)
        if (function.isLambda && function.lambdaOrdinal == lambdaOrdinal) return &function;
    return nullptr;
}

const std::tuple<std::size_t, std::uint64_t, std::string>* ProjectOopMirModel::asyncFrameLocal(
    const ProjectOopMirFunction& function,
    std::size_t localOrdinal) const noexcept
{
    if (built_ && indexes_ != nullptr)
        return indexes_->mirAsyncLocalForOrdinal(function, localOrdinal);
    for (const auto& slot : function.asyncFrameLocals)
        if (std::get<0>(slot) == localOrdinal) return &slot;
    return nullptr;
}

const std::tuple<std::size_t, std::uint64_t, std::string>* ProjectOopMirModel::asyncFrameParameter(
    const ProjectOopMirFunction& function,
    std::size_t parameterOrdinal,
    std::string_view typeKey) const noexcept
{
    if (built_ && indexes_ != nullptr)
        return indexes_->mirAsyncParameterForOrdinal(function, parameterOrdinal, typeKey);
    for (const auto& slot : function.asyncFrameParameters)
        if (std::get<0>(slot) == parameterOrdinal && std::get<2>(slot) == typeKey) return &slot;
    return nullptr;
}
const ProjectOopMirFunction* ProjectOopMirModel::executableEntryPoint(
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    bool& ambiguous) const noexcept
{
    ambiguous = false;
    const ProjectOopMirFunction* result = nullptr;
    for (const ProjectOopMirFunction& function : functions_)
    {
        bool candidate = false;
        if (function.namespaceFunctionOrdinal.has_value())
        {
            if (*function.namespaceFunctionOrdinal >= namespaceFunctionModel.functions().size())
                continue;
            const NamespaceFunctionSymbol& symbol =
                namespaceFunctionModel.functions()[*function.namespaceFunctionOrdinal];
            candidate = symbol.name == "Main" && symbol.parameters.empty() &&
                symbol.declaredTypeKey == "builtin:int" && !symbol.isExternC;
        }
        else if (!function.isLambda && !function.constructor && !function.destructor &&
                 memberModel_ != nullptr && function.memberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& member = memberModel_->members()[function.memberOrdinal];
            candidate = member.kind == ClassMemberKind::Method && member.name == "Main" &&
                member.isStatic && member.parameters.empty() &&
                member.declaredTypeKey == "builtin:int";
        }
        if (!candidate) continue;
        if (result != nullptr)
        {
            ambiguous = true;
            return nullptr;
        }
        result = &function;
    }
    return result;
}
bool ProjectOopMirModel::isBuilt() const noexcept
{
    return built_;
}

const ObjectFieldLayout* ProjectOopMirModel::fieldLayout(std::size_t memberOrdinal) const noexcept
{
    return objectLayoutModel_ != nullptr
        ? objectLayoutModel_->fieldLayoutForMember(memberOrdinal)
        : nullptr;
}

const ObjectAllocationPlan* ProjectOopMirModel::allocationPlan(std::size_t constructionOrdinal) const noexcept
{
    if (objectLayoutModel_ == nullptr)
    {
        return nullptr;
    }
    if (indexes_ != nullptr)
        return indexes_->allocationPlanForConstruction(constructionOrdinal);
    // The plan vector is NOT indexed by construction ordinal, and treating it as if it were was
    // the bug. buildAllocationPlans() walks every construction but APPENDS a plan only for the
    // ones that actually allocate: it skips an unresolved construction, and -- since C6 -- every
    // STRUCT construction, which has no class layout and needs no plan at all because there is
    // nothing to allocate.
    //
    // So `allocationPlans()[ordinal]` was only correct while every construction produced a plan.
    // One skipped construction shifted every later plan down by one, and from there the lookup
    // either ran off the end -- the reported "Stage 34 cannot find Stage 29 allocation plan", an
    // internal error with no file and no line, which cost nine bisection steps to isolate when it
    // showed up in a 300-line file -- or, in the cases that stayed in range, silently returned
    // SOMEBODY ELSE'S plan and would have allocated a class with the wrong size and alignment.
    // The loud half is what got reported; the silent half is the reason this is worth fixing at
    // the root instead of widening the bounds check.
    //
    // Each plan already records the ordinal it belongs to, so match on it.
    for (const ObjectAllocationPlan& plan : objectLayoutModel_->allocationPlans())
    {
        if (plan.constructionOrdinal == constructionOrdinal)
        {
            return &plan;
        }
    }
    return nullptr;
}

// C2.1 "fixed arrays": the size and alignment of one element, for every element type this
// compiler can put in an array -- managed (heap, length-prefixed) or raw (inline storage, a raw
// pointer). Extracted from an inline lambda in the managed-array case so the raw case asks the
// SAME question the same way; two answers that could disagree about a struct's size would produce
// a GEP stride that does not match the layout the field was given.
std::optional<std::pair<std::uint64_t, std::uint64_t>> ProjectOopMirModel::elementLayout(
    std::string_view typeKey) const
{
    if (nameLookup_ == nullptr || objectLayoutModel_ == nullptr) return std::nullopt;
    // One rule for "how big is one of these", shared with the OOP HIR model's aggregate-element
    // offsets (C7). See ProjectObjectLayoutModel::valueLayoutForTypeKey.
    return objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, typeKey);
}

}
