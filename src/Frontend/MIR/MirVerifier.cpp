#include "MirVerifier.h"

#include <algorithm>
#include <vector>

namespace nativesharp::frontend
{
ProjectOopMirModel::MirVerifier::MirVerifier(ProjectOopMirModel& model) noexcept
    : model_(model)
{
}

std::size_t ProjectOopMirModel::MirVerifier::recordGenericSpecialization(std::string_view key)
{
    const auto found = std::find_if(
        model_.genericSpecializations_.begin(),
        model_.genericSpecializations_.end(),
        [key](const ProjectOopMirGenericSpecialization& specialization) {
            return specialization.key == key;
        });
    if (found != model_.genericSpecializations_.end())
        return static_cast<std::size_t>(found - model_.genericSpecializations_.begin());
    model_.genericSpecializations_.push_back(
        ProjectOopMirGenericSpecialization{std::string(key), {}, {}, {}});
    return model_.genericSpecializations_.size() - 1;
}

void ProjectOopMirModel::MirVerifier::recordFunctionSpecializations()
{
    for (std::size_t index = 0; index < model_.functions_.size(); ++index)
    {
        const std::string& key = model_.functions_[index].genericSpecializationKey;
        if (key.empty()) continue;
        model_.genericSpecializations_[recordGenericSpecialization(key)].functionIndices.push_back(index);
    }
}

void ProjectOopMirModel::MirVerifier::recordStaticFieldSpecializations()
{
    for (std::size_t index = 0; index < model_.staticFields_.size(); ++index)
    {
        const std::string& key = model_.staticFields_[index].genericSpecializationKey;
        if (key.empty()) continue;
        model_.genericSpecializations_[recordGenericSpecialization(key)].staticFieldIndices.push_back(index);
    }
}

bool ProjectOopMirModel::MirVerifier::appendInterfaceBinding(
    const ProjectOopMirDispatchTable& table,
    const ProjectOopMirDispatchTable::InterfaceTable& source,
    std::string& error)
{
    if (source.interfaceMemberOrdinals.size() != source.implementationMemberOrdinals.size() ||
        source.interfaceMemberOrdinals.size() != source.calleeNames.size())
    {
        error = "Item 398 MIR interface binding has inconsistent slot vectors";
        return false;
    }
    if (source.interfaceTypeKey.empty() || source.stableInterfaceTypeId == 0 ||
        source.methodTableName.empty())
    {
        error = "Item 398 MIR interface binding is missing stable ABI identity";
        return false;
    }
    if (model_.interfaceBindingForTypes(table.typeOrdinal, source.interfaceTypeOrdinal) != nullptr)
    {
        error = "Item 398 MIR contains a duplicate class/interface binding";
        return false;
    }
    model_.interfaceBindings_.push_back(ProjectOopMirInterfaceBinding{
        table.typeOrdinal,
        source.interfaceTypeOrdinal,
        source.interfaceTypeKey,
        source.stableInterfaceTypeId,
        source.methodTableName,
        source.interfaceMemberOrdinals,
        source.implementationMemberOrdinals,
        source.calleeNames});
    const std::size_t bindingIndex = model_.interfaceBindings_.size() - 1U;
    if (table.typeOrdinal >= model_.interfaceBindingIndexByImplementingType_.size())
        model_.interfaceBindingIndexByImplementingType_.resize(table.typeOrdinal + 1U);
    model_.interfaceBindingIndexByImplementingType_[table.typeOrdinal].try_emplace(
        source.interfaceTypeOrdinal, bindingIndex);
    for (std::size_t slot = 0; slot < source.interfaceMemberOrdinals.size(); ++slot)
    {
        model_.interfaceSlotIndexByMember_.try_emplace(
            source.interfaceMemberOrdinals[slot],
            std::pair{source.stableInterfaceTypeId, slot});
    }
    return true;
}

bool ProjectOopMirModel::MirVerifier::recordDispatchSpecializationsAndBindings(std::string& error)
{
    for (std::size_t index = 0; index < model_.dispatchTables_.size(); ++index)
    {
        const ProjectOopMirDispatchTable& table = model_.dispatchTables_[index];
        if (!table.genericSpecializationKey.empty())
        {
            model_.genericSpecializations_[recordGenericSpecialization(table.genericSpecializationKey)]
                .dispatchTableIndices.push_back(index);
        }
        for (const ProjectOopMirDispatchTable::InterfaceTable& source : table.interfaceTables)
        {
            if (!appendInterfaceBinding(table, source, error)) return false;
        }
    }
    return true;
}

bool ProjectOopMirModel::MirVerifier::validatePanicHandler(std::string& error)
{
    model_.panicHandlerMangledName_.clear();
    if (model_.panicHandler_.empty()) return true;

    const auto found = std::find_if(
        model_.functions_.begin(),
        model_.functions_.end(),
        [&](const ProjectOopMirFunction& function) {
            return function.debugDisplayName == model_.panicHandler_;
        });
    if (found == model_.functions_.end())
    {
        error = "PanicHandler '" + model_.panicHandler_ +
            "' does not name a project namespace function";
        return false;
    }
    static const std::vector<std::string> expectedParameters = {
        "builtin:string", "builtin:string", "builtin:int", "builtin:int"};
    if (!found->isStatic || found->referenceReturn || found->isVariadic ||
        found->returnTypeKey != "builtin:void" || !found->errorTypeKey.empty() ||
        found->parameterTypeKeys != expectedParameters)
    {
        error = "PanicHandler '" + model_.panicHandler_ +
            "' must be a non-throwing free function with signature "
            "void(string message, string file, int line, int column)";
        return false;
    }
    model_.panicHandlerMangledName_ = found->mangledName;
    return true;
}

bool ProjectOopMirModel::MirVerifier::finalize(std::string& error)
{
    recordFunctionSpecializations();
    recordStaticFieldSpecializations();
    if (!recordDispatchSpecializationsAndBindings(error)) return false;
    return validatePanicHandler(error);
}
}
