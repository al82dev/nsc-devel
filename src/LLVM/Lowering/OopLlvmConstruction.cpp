#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

namespace
{
bool simpleValueFieldConstructor(const frontend::ProjectOopMirFunction* function,
    const std::vector<frontend::ObjectFieldLayout>& fields, const frontend::ProjectMemberModel& members,
    const frontend::ProjectOopMirModel& model)
{
    using Kind = frontend::ProjectOopMirNodeKind;
    if (function == nullptr || function->body == nullptr || function->body->kind != Kind::Block ||
        function->body->children.empty() || fields.empty()) return false;
    const auto& statements = function->body->children;
    const auto* last = statements.back().get();
    if (last == nullptr) return false;
    const auto simpleThrow = [](const frontend::ProjectOopMirNode* node)
    {
        return node != nullptr && node->kind == Kind::Throw && node->children.size() == 1 &&
            node->children[0] && node->children[0]->kind == Kind::AllocateAndConstruct &&
            node->children[0]->children.empty();
    };
    bool terminal = last->kind == Kind::Throw;
    if (terminal && !simpleThrow(last)) return false;
    if (last->kind == Kind::If)
    {
        if (last->children.size() != 2 || !last->children[0] || !last->children[1]) return false;
        const auto* condition = last->children[0].get();
        const auto* branch = last->children[1].get();
        const bool literal = condition->kind == Kind::BooleanConstant;
        const bool staticBoolean = condition->kind == Kind::StaticFieldLoad &&
            condition->typeKey == "builtin:bool" && condition->memberOrdinal.has_value() &&
            *condition->memberOrdinal < members.members().size() &&
            members.members()[*condition->memberOrdinal].isStatic &&
            !members.members()[*condition->memberOrdinal].isVolatile;
        if ((!literal && !staticBoolean) || branch->kind != Kind::Block ||
            branch->children.size() != 1 || !simpleThrow(branch->children[0].get())) return false;
        terminal = true;
    }
    if (terminal && statements.size() < 2) return false;
    const std::size_t end = statements.size() - (terminal ? 1 : 0);
    const bool hasBase = statements.front() && statements.front()->kind == Kind::DirectInstanceCall;
    if (hasBase)
    {
        const auto* base = statements[0].get();
        if (base == nullptr || base->kind != Kind::DirectInstanceCall || !base->baseReceiver ||
            !base->memberOrdinal.has_value() || *base->memberOrdinal >= members.members().size() ||
            base->children.size() != 1 || !base->children[0] || base->children[0]->kind != Kind::ThisValue)
            return false;
        const auto& member = members.members()[*base->memberOrdinal];
        if (member.kind != frontend::ClassMemberKind::Constructor ||
            member.declaringTypeOrdinal == function->declaringTypeOrdinal) return false;
    }
    const std::size_t begin = hasBase ? 1 : 0;
    if (end < begin || end - begin < fields.size()) return false;
    std::unordered_set<std::size_t> initialized;
    std::unordered_map<std::size_t, std::string> liveLocals;
    std::unordered_set<std::size_t> seenLocals;
    const auto simpleConstruction = [&](const frontend::ProjectOopMirNode* value, std::string_view typeKey)
    {
        if (value == nullptr || value->kind != Kind::ValueConstruct || value->typeKey != typeKey ||
            value->placementConstructed || !value->memberOrdinal.has_value() ||
            *value->memberOrdinal >= members.members().size()) return false;
        const auto& constructor = members.members()[*value->memberOrdinal];
        const auto* body = model.functionForMangledName(value->calleeName);
        if (constructor.kind != frontend::ClassMemberKind::Constructor || !constructor.declaredErrorTypeKey.empty() ||
            body == nullptr || !body->constructor || body->memberOrdinal != *value->memberOrdinal ||
            body->declaringTypeOrdinal != constructor.declaringTypeOrdinal || body->body == nullptr ||
            body->isImportedC || body->isImportedNative) return false;
        for (const auto& argument : value->children)
            if (!argument || (argument->kind != Kind::IntegerConstant && argument->kind != Kind::FloatingConstant &&
                              argument->kind != Kind::BooleanConstant)) return false;
        return true;
    };
    for (std::size_t index = begin; index < end; ++index)
    {
        const auto* store = statements[index].get();
        if (store != nullptr && store->kind == Kind::LocalDeclaration)
        {
            if (!store->localOrdinal.has_value() || !store->cleanupRegionOrdinal.has_value() ||
                store->children.size() != 1 || !members.typeRequiresValueDrop(store->typeKey) ||
                !seenLocals.insert(*store->localOrdinal).second ||
                !simpleConstruction(store->children[0].get(), store->typeKey)) return false;
            liveLocals.emplace(*store->localOrdinal, store->typeKey);
            continue;
        }
        if (store != nullptr && store->kind == Kind::ExpressionStatement && store->children.size() == 1)
            store = store->children[0].get();
        if (store == nullptr || store->kind != Kind::Store || store->children.size() != 2 ||
            !store->children[0] || !store->children[1]) return false;
        const auto* address = store->children[0].get();
        const auto* value = store->children[1].get();
        if (!address->memberOrdinal.has_value()) return false;
        const auto found = std::find_if(fields.begin(), fields.end(), [&](const auto& candidate)
            { return candidate.memberOrdinal == *address->memberOrdinal; });
        if (found == fields.end() || found->memberOrdinal >= members.members().size()) return false;
        initialized.insert(found->memberOrdinal);
        const auto& field = *found;
        if (address->kind != Kind::FieldAddress ||
            address->children.size() != 1 || !address->children[0] || address->children[0]->kind != Kind::ThisValue ||
            members.members()[field.memberOrdinal].declaringTypeOrdinal != function->declaringTypeOrdinal ||
            members.members()[field.memberOrdinal].isVolatile || value->typeKey != field.typeKey) return false;
        if (value->kind == Kind::MoveValue)
        {
            if (!value->localOrdinal.has_value() || value->parameterOrdinal.has_value() ||
                !value->children.empty()) return false;
            const auto local = liveLocals.find(*value->localOrdinal);
            if (local == liveLocals.end() || local->second != field.typeKey) return false;
            liveLocals.erase(local);
        }
        else if (!simpleConstruction(value, field.typeKey)) return false;
    }
    return initialized.size() == fields.size() && liveLocals.empty();
}

struct ReceiverPublicationScan
{
    bool publishes = false;
    bool returnsSource = false;
    std::vector<std::string> returnDependencies;
    std::vector<std::pair<std::string, std::optional<std::size_t>>> directReceiverCallees;
    std::vector<std::size_t> virtualReceiverSlots;
    std::vector<std::pair<std::size_t, std::size_t>> interfaceReceiverSlots;
    std::vector<std::pair<std::string, std::size_t>> directArgumentCallees;
    std::vector<std::tuple<std::string, std::size_t, std::size_t>> virtualArgumentCallees;
    std::vector<std::tuple<std::size_t, std::size_t, std::size_t>> interfaceArgumentCallees;
};

// A body consisting solely of returning a parameter cannot publish its input.
// Missing bodies, cleanup, side effects and non-direct calls remain unknown.
const frontend::ProjectOopMirNode* pureIdentityArgument(
    const frontend::ProjectOopMirNode* call, const frontend::ProjectOopMirModel& mirModel)
{
    using Kind = frontend::ProjectOopMirNodeKind;
    if (call == nullptr || call->kind != Kind::DirectStaticCall) return nullptr;
    const auto* function = mirModel.functionForMangledName(call->calleeName);
    if (function == nullptr || !function->isStatic || !function->cleanupRegions.empty() ||
        function->body == nullptr || function->body->kind != Kind::Block ||
        function->body->children.size() != 1U || function->body->children.front() == nullptr) return nullptr;
    const auto* result = function->body->children.front().get();
    if (result->kind != Kind::Return || result->children.size() != 1U || result->children.front() == nullptr ||
        result->children.front()->kind != Kind::ParameterValue ||
        !result->children.front()->parameterOrdinal.has_value()) return nullptr;
    const std::size_t parameter = *result->children.front()->parameterOrdinal;
    for (std::size_t index = 0; index < call->children.size(); ++index)
    {
        if (!call->argumentParameterOrdinals.empty() && index >= call->argumentParameterOrdinals.size())
            return nullptr;
        const std::size_t ordinal = call->argumentParameterOrdinals.empty()
            ? index : call->argumentParameterOrdinals[index];
        if (ordinal == parameter) return call->children[index].get();
    }
    return nullptr;
}

ReceiverPublicationScan scanReceiverPublication(
    const frontend::ProjectOopMirNode* body, std::optional<std::size_t> sourceParameter,
    const std::unordered_map<std::string, std::unordered_set<std::size_t>>& returnedSources,
    const frontend::ProjectOopMirModel& mirModel)
{
    using Kind = frontend::ProjectOopMirNodeKind;
    std::unordered_map<const frontend::ProjectOopMirNode*, std::vector<std::string>> dynamicCallees;
    const auto dynamicTargets = [&](const frontend::ProjectOopMirNode* call) -> const std::vector<std::string>& {
        auto [entry, inserted] = dynamicCallees.try_emplace(call);
        if (!inserted || call->children.empty()) return entry->second;
        if (call->kind == Kind::InterfaceInstanceCall)
        {
            if (!call->interfaceSlot.has_value() || !call->targetTypeOrdinal.has_value()) return entry->second;
            for (const auto& candidate : mirModel.dispatchTables())
                for (const auto& binding : candidate.interfaceTables)
                    if (binding.interfaceTypeOrdinal == *call->targetTypeOrdinal &&
                        *call->interfaceSlot < binding.calleeNames.size())
                        entry->second.push_back(binding.calleeNames[*call->interfaceSlot]);
            return entry->second;
        }
        if (!call->virtualSlot.has_value()) return entry->second;
        const auto& receiverType = call->children.front()->typeKey;
        for (const auto& candidate : mirModel.dispatchTables())
        {
            const auto* ancestor = &candidate;
            while (ancestor != nullptr && ancestor->typeKey != receiverType)
                ancestor = ancestor->baseTypeOrdinal.has_value()
                    ? mirModel.dispatchTableForType(*ancestor->baseTypeOrdinal) : nullptr;
            if (ancestor != nullptr && *call->virtualSlot < candidate.slotCalleeNames.size())
                entry->second.push_back(candidate.slotCalleeNames[*call->virtualSlot]);
        }
        return entry->second;
    };
    enum class OriginKind { Value, LocalRead, ThisFieldRead };
    const auto origins = [&](const frontend::ProjectOopMirNode* value) {
        std::vector<std::pair<const frontend::ProjectOopMirNode*, OriginKind>> leaves;
        std::vector<const frontend::ProjectOopMirNode*> work{value};
        while (!work.empty())
        {
            value = work.back();
            work.pop_back();
            if (value == nullptr) continue;
            // A conditional value may originate from either result branch. Its condition
            // does not contribute to the resulting reference.
            if (value->kind == Kind::Conditional && value->children.size() == 3U)
            {
                work.push_back(value->children[1].get());
                work.push_back(value->children[2].get());
                continue;
            }
            // Either operand of a null-coalescing expression can be its result.
            if (value->kind == Kind::NullCoalescing && value->children.size() == 2U)
            {
                work.push_back(value->children[0].get());
                work.push_back(value->children[1].get());
                continue;
            }
            if (value->kind == Kind::NullCoalescingStore && value->children.size() == 2U)
            {
                const auto* address = value->children[0].get();
                if (address != nullptr && address->kind == Kind::LocalAddress)
                    leaves.emplace_back(address, OriginKind::LocalRead);
                else if (address != nullptr && address->kind == Kind::FieldAddress &&
                         address->memberOrdinal.has_value() && address->children.size() == 1U &&
                         address->children.front() != nullptr &&
                         address->children.front()->kind == Kind::ThisValue)
                    leaves.emplace_back(address, OriginKind::ThisFieldRead);
                work.push_back(value->children[1].get());
                continue;
            }
            if ((value->kind == Kind::Convert || value->kind == Kind::BorrowValue) &&
                value->children.size() == 1U)
            {
                work.push_back(value->children.front().get());
                continue;
            }
            if (value->kind == Kind::DirectInstanceCall || value->kind == Kind::DirectStaticCall ||
                value->kind == Kind::VirtualInstanceCall || value->kind == Kind::InterfaceInstanceCall)
            {
                std::vector<std::string> direct;
                const std::vector<std::string>* targets = nullptr;
                if (value->kind == Kind::VirtualInstanceCall || value->kind == Kind::InterfaceInstanceCall)
                    targets = &dynamicTargets(value);
                else { direct.push_back(value->calleeName); targets = &direct; }
                bool hasSummary = false;
                for (const auto& target : *targets)
                {
                    const auto found = returnedSources.find(target);
                    if (found == returnedSources.end()) continue;
                    hasSummary = true;
                    const std::size_t first = value->kind == Kind::DirectStaticCall ? 0U : 1U;
                    if (first != 0U && !value->children.empty() &&
                        found->second.contains(static_cast<std::size_t>(-1)))
                        work.push_back(value->children.front().get());
                    for (std::size_t child = first; child < value->children.size(); ++child)
                    {
                        const std::size_t source = child - first;
                        const std::size_t parameter = value->argumentParameterOrdinals.empty()
                            ? source : value->argumentParameterOrdinals.at(source);
                        if (found->second.contains(parameter)) work.push_back(value->children[child].get());
                    }
                }
                if (hasSummary) continue;
            }
            leaves.emplace_back(value, OriginKind::Value);
        }
        return leaves;
    };
    std::unordered_map<std::size_t, std::vector<std::size_t>> localCopies;
    std::unordered_map<std::size_t, std::vector<std::size_t>> fieldToLocal;
    std::unordered_map<std::size_t, std::vector<std::size_t>> localToField;
    std::unordered_map<std::size_t, std::vector<std::size_t>> fieldToField;
    std::vector<std::pair<std::size_t, const frontend::ProjectOopMirNode*>> receiverFieldWrites;
    std::vector<std::size_t> receiverFields;
    std::unordered_map<std::size_t, std::vector<std::size_t>> globalCopies;
    std::vector<std::size_t> receiverLocals;
    std::vector<std::size_t> globalLocals;
    std::vector<const frontend::ProjectOopMirNode*> globalValues;
    std::vector<std::pair<const frontend::ProjectOopMirNode*, const frontend::ProjectOopMirNode*>> fieldStores;
    std::vector<const frontend::ProjectOopMirNode*> returnValues;
    std::vector<const frontend::ProjectOopMirNode*> directCalls;
    const auto isOrigin = [&](const frontend::ProjectOopMirNode* value) {
        return value != nullptr && ((!sourceParameter.has_value() && value->kind == Kind::ThisValue) ||
            (sourceParameter.has_value() && value->kind == Kind::ParameterValue &&
             value->parameterOrdinal == sourceParameter));
    };
    const auto containerRoot = [&](const frontend::ProjectOopMirNode* value) {
        while (value != nullptr)
        {
            if (!value->children.empty() &&
                ((value->children.size() == 1U &&
                  (value->kind == Kind::FieldLoad || value->kind == Kind::Convert || value->kind == Kind::BorrowValue)) ||
                 value->kind == Kind::ArrayElementLoad || value->kind == Kind::RangeArrayElementLoad))
                value = value->children.front().get();
            else if (const auto* argument = pureIdentityArgument(value, mirModel); argument != nullptr)
                value = argument;
            else break;
        }
        return value;
    };
    const auto mayBeGlobalContainer = [&](const frontend::ProjectOopMirNode* root) {
        return root != nullptr && (root->kind == Kind::StaticFieldLoad || root->kind == Kind::NamespaceStateLoad ||
            root->kind == Kind::DirectStaticCall || root->kind == Kind::DirectInstanceCall ||
            root->kind == Kind::VirtualInstanceCall || root->kind == Kind::InterfaceInstanceCall);
    };
    const auto recordCopy = [&](std::size_t destination, const frontend::ProjectOopMirNode* value) {
        for (const auto& [origin, originKind] : origins(value))
        {
            if (isOrigin(origin)) receiverLocals.push_back(destination);
            else if ((origin->kind == Kind::LocalValue || originKind == OriginKind::LocalRead) && origin->localOrdinal.has_value())
                localCopies[*origin->localOrdinal].push_back(destination);
            else if (originKind == OriginKind::ThisFieldRead && origin->memberOrdinal.has_value())
                fieldToLocal[*origin->memberOrdinal].push_back(destination);
        }
        const auto* root = containerRoot(value);
        if (mayBeGlobalContainer(root))
            globalLocals.push_back(destination);
        else if (root != nullptr && root->kind == Kind::LocalValue && root->localOrdinal.has_value())
        {
            globalCopies[*root->localOrdinal].push_back(destination);
            globalCopies[destination].push_back(*root->localOrdinal);
        }
    };
    std::vector<const frontend::ProjectOopMirNode*> pending{body};
    while (!pending.empty())
    {
        const auto* node = pending.back();
        pending.pop_back();
        if (node == nullptr) continue;
        if (node->kind == Kind::LocalDeclaration && node->localOrdinal.has_value() && !node->children.empty())
            recordCopy(*node->localOrdinal, node->children.front().get());
        if ((node->kind == Kind::DirectInstanceCall || node->kind == Kind::VirtualInstanceCall ||
             node->kind == Kind::InterfaceInstanceCall ||
             node->kind == Kind::DirectStaticCall) &&
            !node->children.empty()) directCalls.push_back(node);
        if (node->kind == Kind::Return && !node->children.empty())
            returnValues.push_back(node->children.front().get());
        if ((node->kind == Kind::Store || node->kind == Kind::NullCoalescingStore) &&
            node->children.size() >= 2U && node->children[0] != nullptr)
        {
            const auto* target = node->children[0].get();
            if (target->kind == Kind::FieldAddress && target->memberOrdinal.has_value() &&
                target->children.size() == 1U && target->children.front() != nullptr &&
                target->children.front()->kind == Kind::ThisValue)
                receiverFieldWrites.emplace_back(*target->memberOrdinal, node->children[1].get());
            if (target->kind == Kind::LocalAddress && target->localOrdinal.has_value())
                recordCopy(*target->localOrdinal, node->children[1].get());
            else if (target->kind == Kind::StaticFieldAddress || target->kind == Kind::NamespaceStateAddress)
                globalValues.push_back(node->children[1].get());
            else if ((target->kind == Kind::FieldAddress || target->kind == Kind::ArrayElementAddress ||
                target->kind == Kind::RangeArrayElementAddress) && !target->children.empty() &&
                target->children.front() != nullptr)
                fieldStores.emplace_back(target->children.front().get(), node->children[1].get());
        }
        for (const auto& child : node->children) pending.push_back(child.get());
    }
    for (const auto& [field, value] : receiverFieldWrites)
        for (const auto& [origin, originKind] : origins(value))
        {
            if (isOrigin(origin)) receiverFields.push_back(field);
            else if ((origin->kind == Kind::LocalValue || originKind == OriginKind::LocalRead) && origin->localOrdinal.has_value())
                localToField[*origin->localOrdinal].push_back(field);
            else if (originKind == OriginKind::ThisFieldRead && origin->memberOrdinal.has_value())
                fieldToField[*origin->memberOrdinal].push_back(field);
        }
    std::unordered_set<std::size_t> reached;
    std::unordered_set<std::size_t> reachedFields;
    while (!receiverLocals.empty() || !receiverFields.empty())
    {
        if (!receiverFields.empty())
        {
            const std::size_t field = receiverFields.back();
            receiverFields.pop_back();
            if (!reachedFields.insert(field).second) continue;
            if (const auto copies = fieldToLocal.find(field); copies != fieldToLocal.end())
                receiverLocals.insert(receiverLocals.end(), copies->second.begin(), copies->second.end());
            if (const auto copies = fieldToField.find(field); copies != fieldToField.end())
                receiverFields.insert(receiverFields.end(), copies->second.begin(), copies->second.end());
            continue;
        }
        const std::size_t local = receiverLocals.back();
        receiverLocals.pop_back();
        if (!reached.insert(local).second) continue;
        const auto copies = localCopies.find(local);
        if (copies != localCopies.end())
            receiverLocals.insert(receiverLocals.end(), copies->second.begin(), copies->second.end());
        if (const auto fields = localToField.find(local); fields != localToField.end())
            receiverFields.insert(receiverFields.end(), fields->second.begin(), fields->second.end());
    }
    // A local container published into global state is also globally reachable.
    for (const auto* value : globalValues)
    {
        const auto* root = containerRoot(value);
        if (root != nullptr && root->kind == Kind::LocalValue && root->localOrdinal.has_value())
            globalLocals.push_back(*root->localOrdinal);
    }
    // Calls without the proven identity body may publish a container argument or receiver.
    for (const auto* call : directCalls)
    {
        if (pureIdentityArgument(call, mirModel) != nullptr) continue;
        for (const auto& argument : call->children)
        {
            const auto* root = containerRoot(argument.get());
            if (root != nullptr && root->kind == Kind::LocalValue && root->localOrdinal.has_value())
                globalLocals.push_back(*root->localOrdinal);
        }
    }
    std::unordered_set<std::size_t> globalReached;
    while (!globalLocals.empty())
    {
        const std::size_t local = globalLocals.back();
        globalLocals.pop_back();
        if (!globalReached.insert(local).second) continue;
        const auto copies = globalCopies.find(local);
        if (copies != globalCopies.end())
            globalLocals.insert(globalLocals.end(), copies->second.begin(), copies->second.end());
    }
    const auto isReceiver = [&](const frontend::ProjectOopMirNode* value) {
        for (const auto& [origin, originKind] : origins(value))
            if (isOrigin(origin) || ((origin->kind == Kind::LocalValue || originKind == OriginKind::LocalRead) && origin->localOrdinal.has_value() &&
                reached.contains(*origin->localOrdinal)) ||
                (originKind == OriginKind::ThisFieldRead && origin->memberOrdinal.has_value() &&
                 reachedFields.contains(*origin->memberOrdinal))) return true;
        return false;
    };
    ReceiverPublicationScan result;
    for (const auto* value : returnValues)
        if (isReceiver(value)) { result.returnsSource = true; break; }
    for (const auto* value : globalValues)
        if (isReceiver(value)) { result.publishes = true; return result; }
    for (const auto& [object, value] : fieldStores)
    {
        const auto* root = containerRoot(object);
        if (root != nullptr && (mayBeGlobalContainer(root) ||
            (root->kind == Kind::LocalValue && root->localOrdinal.has_value() &&
             globalReached.contains(*root->localOrdinal))) && isReceiver(value))
        { result.publishes = true; return result; }
    }
    for (const auto* call : directCalls)
    {
        if ((call->kind == Kind::DirectInstanceCall || call->kind == Kind::DirectStaticCall) &&
            !call->calleeName.empty())
            result.returnDependencies.push_back(call->calleeName);
        if (call->kind == Kind::VirtualInstanceCall || call->kind == Kind::InterfaceInstanceCall)
            for (const auto& target : dynamicTargets(call))
                if (!target.empty()) result.returnDependencies.push_back(target);
        if (call->kind != Kind::DirectStaticCall && isReceiver(call->children.front().get()))
        {
            if (call->kind == Kind::VirtualInstanceCall && call->virtualSlot.has_value())
                result.virtualReceiverSlots.push_back(*call->virtualSlot);
            else if (call->kind == Kind::InterfaceInstanceCall && call->interfaceSlot.has_value() &&
                     call->targetTypeOrdinal.has_value())
                result.interfaceReceiverSlots.emplace_back(*call->targetTypeOrdinal, *call->interfaceSlot);
            else if (call->kind == Kind::DirectInstanceCall && !call->calleeName.empty())
                result.directReceiverCallees.emplace_back(call->calleeName, call->memberOrdinal);
        }
        if ((call->kind == Kind::DirectStaticCall || call->kind == Kind::DirectInstanceCall) &&
            !call->calleeName.empty())
        {
            const std::size_t firstArgument = call->kind == Kind::DirectStaticCall ? 0U : 1U;
            for (std::size_t child = firstArgument; child < call->children.size(); ++child)
                if (isReceiver(call->children[child].get()))
                {
                    const std::size_t source = child - firstArgument;
                    const std::size_t parameter = call->argumentParameterOrdinals.empty()
                        ? source : call->argumentParameterOrdinals.at(source);
                    result.directArgumentCallees.emplace_back(call->calleeName, parameter);
                }
        }
        if (call->kind == Kind::VirtualInstanceCall && call->virtualSlot.has_value())
            for (std::size_t child = 1; child < call->children.size(); ++child)
                if (isReceiver(call->children[child].get()))
                {
                    const std::size_t source = child - 1U;
                    const std::size_t parameter = call->argumentParameterOrdinals.empty()
                        ? source : call->argumentParameterOrdinals.at(source);
                    result.virtualArgumentCallees.emplace_back(
                        call->children.front()->typeKey, *call->virtualSlot, parameter);
                }
        if (call->kind == Kind::InterfaceInstanceCall && call->interfaceSlot.has_value() &&
            call->targetTypeOrdinal.has_value())
            for (std::size_t child = 1; child < call->children.size(); ++child)
                if (isReceiver(call->children[child].get()))
                {
                    const std::size_t source = child - 1U;
                    const std::size_t parameter = call->argumentParameterOrdinals.empty()
                        ? source : call->argumentParameterOrdinals.at(source);
                    result.interfaceArgumentCallees.emplace_back(
                        *call->targetTypeOrdinal, *call->interfaceSlot, parameter);
                }
    }
    return result;
}
}

LLVMValueRef OopLlvmImplementation::lowerValueConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (node->immediate == 0 || node->alignment == 0)
        {
            return failValue(error, "C6 struct construction has no value layout");
        }
        if (node->argumentCount > node->children.size())
        {
            return failValue(error, "C6 struct construction has an invalid source-argument count");
        }
        std::vector<LLVMValueRef> evaluatedArguments;
        evaluatedArguments.reserve(node->argumentCount);
        for (std::size_t index = 0; index < node->argumentCount; ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index]
                : lowerExpression(node->children[index].get(), error);
            if (value == nullptr) return nullptr;
            // Win64 aggregate ABI: a struct whose size is not 1/2/4/8 travels as a POINTER to a
            // caller-owned per-call copy, never by value. The constructor's SIGNATURE already
            // carries that mapping -- declareFunctions builds every parameter through
            // abiParameterType -- so the call site has to agree, exactly like the four ordinary
            // call lowerers already do.
            //
            // Missing this was the bug: the call site loaded `[16 x i8]` and passed it by value
            // while the constructor declared `ptr`, and the LLVM verifier rejected the whole
            // module with "Call parameter type does not match function signature!". Measured
            // scope was every aggregate that does not fit in a register -- 12, 16, 20 and 64
            // bytes fail, 8 passes -- and it was SPECIFIC to constructors, because a constructor
            // is the one call shape that did not go through the mapping. It deformed the public
            // API of every type naturally built from a Matrix or a Vector4.
            value = wrapIndirectArgument(value, node->children[index]->typeKey, error);
            if (value == nullptr) return nullptr;
            evaluatedArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, evaluatedArguments.size(), parameterToSource, error)) return nullptr;
        std::vector<LLVMValueRef> orderedArguments;
        orderedArguments.reserve(evaluatedArguments.size());
        for (const std::size_t source : parameterToSource)
            orderedArguments.push_back(evaluatedArguments[source]);

        LLVMTypeRef carrier = llvmType(node->typeKey, error);
        if (carrier == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(
            carrier, static_cast<unsigned>(node->alignment), "struct.new");
        if (slot == nullptr) return failValue(error, "C6 struct construction could not reserve storage");

        if (node->zeroInitialize)
        {
            LLVMValueRef args[] = {
                slot,
                LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
                LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
                LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, args, 4, "");
        }

        if (!node->calleeName.empty())
        {
            const auto target = functions_.find(node->calleeName);
            if (target == functions_.end())
                return failValue(error, "C6 struct constructor target missing: " + node->calleeName);
            std::vector<LLVMValueRef> ctorArgs;
            ctorArgs.reserve(1 + orderedArguments.size());
            ctorArgs.push_back(slot);
            ctorArgs.insert(ctorArgs.end(), orderedArguments.begin(), orderedArguments.end());
            releaseTransferredArguments(node);
            if (buildPotentiallyThrowingCall(
                    target->second.functionType, target->second.function, ctorArgs.data(),
                    static_cast<unsigned>(ctorArgs.size()), "", error) == nullptr && !error.empty())
            {
                return nullptr;
            }
        }

        // Field initializers synthesized for an implicit default constructor run against the slot,
        // exactly as they run against the freshly allocated object on the class path.
        LLVMValueRef previousOverride = constructionThisOverride_;
        constructionThisOverride_ = slot;
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            if (lowerExpression(node->children[index].get(), error) == nullptr)
            {
                constructionThisOverride_ = previousOverride;
                return nullptr;
            }
        }
        constructionThisOverride_ = previousOverride;

        // BUG FOUND BY C7, FIXED HERE. This used to short-circuit -- `if
        // (isIndirectStructTypeKey(...)) return slot;` -- handing back the ADDRESS of the freshly
        // constructed struct for any aggregate that travels indirectly (anything not 1/2/4/8
        // bytes). But an expression of aggregate type hands over the LOADED carrier at this
        // level, never a pointer: a call returning one writes into an sret slot and then loads it
        // (`abi.sret.load`), and the TemporaryAddress case above already carries the same lesson
        // in writing. The mismatch was silent rather than fatal wherever the consumer stored the
        // result: `Big g = new Big(1.0f);` on a twenty-byte struct emitted
        // `store ptr %struct.new, ptr %local0.addr` -- the local ended up holding a POINTER in
        // its first eight bytes, and every field read afterwards read pointer bits as floats.
        // Wrong values, no diagnostic, no verifier complaint. One shape, one path.
        LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "struct.new.value");
        if (node->alignment > 1U) LLVMSetAlignment(loaded, static_cast<unsigned>(node->alignment));
        return loaded;
    }

LLVMValueRef OopLlvmImplementation::lowerParameterPack(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (node == nullptr || node->immediate == 0 || node->alignment == 0 ||
            node->secondaryImmediate == 0)
        {
            return failValue(error, "T01-F02 parameter pack has incomplete layout data");
        }
        LLVMTypeRef spanType = llvmType(node->typeKey, error);
        LLVMTypeRef elementType = llvmType(node->elementTypeKey, error);
        if (spanType == nullptr || elementType == nullptr) return nullptr;

        const unsigned spanAlignment = structAlignmentFor(node->typeKey);
        LLVMValueRef spanSlot = entryBlockAlloca(spanType, spanAlignment, "params.span");
        if (spanSlot == nullptr)
            return failValue(error, "T01-F02 could not reserve parameter-pack span storage");

        LLVMValueRef memsetArgs[] = {
            spanSlot,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            LLVMConstInt(LLVMInt64TypeInContext(context_), node->secondaryImmediate, 0),
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, memsetArgs, 4, "");

        LLVMTypeRef ptrType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef elementStorage = LLVMConstNull(ptrType);
        if (!node->children.empty())
        {
            LLVMTypeRef arrayType = LLVMArrayType2(elementType, node->children.size());
            elementStorage = entryBlockAlloca(
                arrayType, static_cast<unsigned>(node->alignment), "params.elements");
            if (elementStorage == nullptr)
                return failValue(error, "T01-F02 could not reserve parameter-pack elements");

            for (std::size_t index = 0; index < node->children.size(); ++index)
            {
                LLVMValueRef value = argumentOverrides != nullptr
                    ? (*argumentOverrides)[index]
                    : lowerExpression(node->children[index].get(), error);
                if (value == nullptr) return nullptr;
                LLVMValueRef indices[] = {
                    LLVMConstInt(LLVMInt32TypeInContext(context_), 0, 0),
                    LLVMConstInt(LLVMInt64TypeInContext(context_), index, 0)};
                LLVMValueRef address = LLVMBuildGEP2(
                    builder_, arrayType, elementStorage, indices, 2, "params.element.addr");
                LLVMValueRef store = LLVMBuildStore(builder_, value, address);
                if (node->alignment > 1U)
                    LLVMSetAlignment(store, static_cast<unsigned>(node->alignment));
            }
        }

        LLVMValueRef pointerAddress = byteOffset(
            spanSlot, node->integerValue, "params.span.pointer.addr");
        LLVMValueRef pointerStore = LLVMBuildStore(builder_, elementStorage, pointerAddress);
        LLVMSetAlignment(pointerStore, static_cast<unsigned>(sizeof(void*)));
        LLVMValueRef countAddress = byteOffset(
            spanSlot, node->bitStorageSize, "params.span.count.addr");
        LLVMValueRef countStore = LLVMBuildStore(
            builder_,
            LLVMConstInt(LLVMInt32TypeInContext(context_), node->children.size(), 0),
            countAddress);
        LLVMSetAlignment(countStore, 4U);

        LLVMValueRef result = LLVMBuildLoad2(builder_, spanType, spanSlot, "params.span.value");
        if (spanAlignment > 1U) LLVMSetAlignment(result, spanAlignment);
        return result;
    }

bool OopLlvmImplementation::initializeAggregateSpread(
        const frontend::ProjectOopMirNode* child,
        LLVMValueRef slot,
        const std::vector<LLVMValueRef>* fieldValues,
        std::size_t& fieldIndex,
        std::string& error)
{
                if (child->children.empty())
                    return fail(error, "T01-F05 aggregate spread has no source value");
                LLVMValueRef sourceSlot = nullptr;
                if (child->booleanValue)
                {
                    if (fieldValues != nullptr)
                    {
                        if (fieldIndex < fieldValues->size()) sourceSlot = (*fieldValues)[fieldIndex++];
                    }
                    else sourceSlot = lowerExpression(child->children.front().get(), error);
                    if (sourceSlot == nullptr) return false;
                }
                else
                {
                    LLVMValueRef sourceValue = nullptr;
                    if (fieldValues != nullptr)
                    {
                        if (fieldIndex < fieldValues->size()) sourceValue = (*fieldValues)[fieldIndex++];
                    }
                    else sourceValue = lowerExpression(child->children.front().get(), error);
                    if (sourceValue == nullptr) return false;
                    LLVMTypeRef sourceType = llvmType(child->typeKey, error);
                    if (sourceType == nullptr) return false;
                    sourceSlot = entryBlockAlloca(
                        sourceType, structAlignmentFor(child->typeKey), "aggregate.spread.source");
                    LLVMBuildStore(builder_, sourceValue, sourceSlot);
                }
                for (std::size_t index = 1; index < child->children.size(); ++index)
                {
                    const frontend::ProjectOopMirNode* descriptor = child->children[index].get();
                    if (descriptor == nullptr ||
                        descriptor->kind != frontend::ProjectOopMirNodeKind::AggregateElementStore ||
                        !descriptor->children.empty() || descriptor->bitStorageSize == 0)
                    {
                        return fail(error, "T01-F05 aggregate spread has a malformed field descriptor");
                    }
                    LLVMValueRef destination = byteOffset(
                        slot, descriptor->immediate, "aggregate.spread.destination");
                    LLVMValueRef source = byteOffset(
                        sourceSlot, descriptor->secondaryImmediate, "aggregate.spread.source.field");
                    LLVMValueRef copyArgs[] = {
                        destination,
                        source,
                        LLVMConstInt(
                            LLVMInt64TypeInContext(context_), descriptor->bitStorageSize, 0),
                        LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
                    buildNoUnwindCall(memcpyFunctionType_, memcpyFunction_, copyArgs, 4, "");
                    if (child->booleanValue)
                    {
                        // T02-F06 move-spread transfers this field's bits into the destination.
                        // Empty the original slot immediately so the source aggregate's ordinary
                        // drop glue skips the moved field while continuing to destroy untouched
                        // siblings. Zero is the canonical empty state for every drop-bearing
                        // carrier in the 3.0 value model.
                        LLVMValueRef clearArgs[] = {
                            source,
                            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
                            LLVMConstInt(
                                LLVMInt64TypeInContext(context_), descriptor->bitStorageSize, 0),
                            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
                        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, clearArgs, 4, "");
                    }
                }
        return true;
    }

bool OopLlvmImplementation::initializeAggregateField(
        const frontend::ProjectOopMirNode* child,
        LLVMValueRef slot,
        const std::vector<LLVMValueRef>* fieldValues,
        std::size_t& fieldIndex,
        std::string& error)
{
            if (child == nullptr ||
                child->kind != frontend::ProjectOopMirNodeKind::AggregateElementStore ||
                child->children.size() != 1)
            {
                return fail(error, "C7 aggregate initializer has a malformed element");
            }
            LLVMValueRef value = nullptr;
            if (fieldValues != nullptr)
            {
                if (fieldIndex < fieldValues->size()) value = (*fieldValues)[fieldIndex++];
            }
            else value = lowerExpression(child->children.front().get(), error);
            if (value == nullptr) return false;
            LLVMValueRef address = byteOffset(slot, child->immediate, "aggregate.element.addr");
            if (child->bitWidth.has_value())
            {
                LLVMTypeRef storageType = bitFieldStorageType(child, error);
                if (storageType == nullptr) return false;
                LLVMValueRef previous = LLVMBuildLoad2(
                    builder_, storageType, address, "aggregate.bitfield.storage.load");
                LLVMSetAlignment(previous, static_cast<unsigned>(child->alignment));
                if (child->volatileAccess) LLVMSetVolatile(previous, 1);
                LLVMValueRef merged = insertBitField(child, previous, value, error);
                if (merged == nullptr) return false;
                LLVMValueRef store = LLVMBuildStore(builder_, merged, address);
                LLVMSetAlignment(store, static_cast<unsigned>(child->alignment));
                if (child->volatileAccess) LLVMSetVolatile(store, 1);
                return true;
            }
            LLVMValueRef store = LLVMBuildStore(builder_, value, address);
            // An element is written through its own typed address, so it needs its own natural
            // alignment -- the carrier's `[N x i8]` shape would otherwise imply align 1.
            const unsigned elementAlignment = structAlignmentFor(child->typeKey);
            if (elementAlignment > 1U) LLVMSetAlignment(store, elementAlignment);
            else if (LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMVectorTypeKind)
                LLVMSetAlignment(store, 1U);
        return true;
    }

bool OopLlvmImplementation::initializeAggregateIntoSlot(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef slot,
        std::string& error,
        const std::vector<LLVMValueRef>* fieldValues )
{
        if (node == nullptr ||
            node->kind != frontend::ProjectOopMirNodeKind::AggregateInitialize ||
            slot == nullptr || node->immediate == 0 || node->alignment == 0)
        {
            return fail(error, "C7 aggregate initializer has no destination or value layout");
        }
        std::size_t fieldIndex = 0;
        LLVMValueRef memsetArgs[] = {
            slot,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, memsetArgs, 4, "");
        const bool trackTupleElements = fieldValues == nullptr && memberModel_ != nullptr &&
            frontend::ProjectTypeConversion::tupleElementTypeKeys(node->typeKey).has_value() &&
            memberModel_->typeTransitivelyHasCustomDestructor(node->typeKey) &&
            std::ranges::all_of(node->children, [](const auto& child)
            {
                return child != nullptr &&
                    child->kind == frontend::ProjectOopMirNodeKind::AggregateElementStore;
            });
        std::vector<std::pair<std::size_t, std::unique_ptr<frontend::ProjectOopMirNode>>> partial;
        const auto releasePartial = [&]()
        {
            for (auto it = partial.rbegin(); it != partial.rend(); ++it)
            {
                activeCleanupRegions_.pop_back();
                cleanupRegions_.erase(it->first);
                localStorage_.erase(it->first);
                cleanupChainCache_.erase(it->first);
            }
        };
        for (const auto& child : node->children)
        {
            if (child != nullptr &&
                child->kind == frontend::ProjectOopMirNodeKind::AggregateSpreadStore)
            {
                if (!initializeAggregateSpread(child.get(), slot, fieldValues, fieldIndex, error))
                {
                    releasePartial();
                    return false;
                }
                continue;
            }
            if (!initializeAggregateField(child.get(), slot, fieldValues, fieldIndex, error))
            {
                releasePartial();
                return false;
            }
            if (trackTupleElements && memberModel_->typeRequiresValueDrop(child->typeKey))
            {
                std::size_t ordinal = ~std::size_t{0};
                while (cleanupRegions_.contains(ordinal) || localStorage_.contains(ordinal))
                    --ordinal;
                auto cleanup = std::make_unique<frontend::ProjectOopMirNode>();
                cleanup->kind = frontend::ProjectOopMirNodeKind::LocalDeclaration;
                cleanup->typeKey = child->typeKey;
                cleanup->stringValue = "tuple.partial.element.cleanup";
                cleanup->localOrdinal = ordinal;
                cleanup->cleanupRegionOrdinal = ordinal;
                cleanupChainCache_.erase(ordinal);
                cleanupRegions_.emplace(ordinal, cleanup.get());
                localStorage_.emplace(ordinal, byteOffset(slot, child->immediate, "tuple.partial.addr"));
                activeCleanupRegions_.push_back(ordinal);
                partial.emplace_back(ordinal, std::move(cleanup));
            }
        }
        releasePartial();
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerAggregateInitializer(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* fieldValues )
{
        if (node->immediate == 0 || node->alignment == 0)
        {
            return failValue(error, "C7 aggregate initializer has no value layout");
        }
        LLVMTypeRef carrier = llvmType(node->typeKey, error);
        if (carrier == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(
            carrier, static_cast<unsigned>(node->alignment), "aggregate.init");
        if (slot == nullptr)
            return failValue(error, "C7 aggregate initializer could not reserve storage");
        if (!initializeAggregateIntoSlot(node, slot, error, fieldValues)) return nullptr;

        // Always the loaded carrier, never the slot pointer -- see the note in
        // lowerValueConstruction for the bug that short-circuit causes.
        LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "aggregate.init.value");
        if (node->alignment > 1U) LLVMSetAlignment(loaded, static_cast<unsigned>(node->alignment));
        return loaded;
    }

bool OopLlvmImplementation::collectConstructionArguments(
        const frontend::ProjectOopMirNode* node,
        std::size_t childOffset,
        const std::vector<LLVMValueRef>* argumentOverrides,
        std::vector<LLVMValueRef>& orderedArguments,
        std::string& error)
{
        // Source arguments are evaluated left-to-right before object allocation. Their SSA values
        // therefore dominate both the allocation-success branch and the constructor call. This
        // prevents argument side effects from disappearing merely because allocation returns null.
        std::vector<LLVMValueRef> evaluatedArguments;
        evaluatedArguments.reserve(node->argumentCount);
        for (std::size_t index = 0; index < node->argumentCount; ++index)
        {
            LLVMValueRef value = argumentOverrides != nullptr
                ? (*argumentOverrides)[index]
                : lowerExpression(node->children[index + childOffset].get(), error);
            if (value == nullptr) return false;
            // Win64 aggregate ABI: a struct whose size is not 1/2/4/8 travels as a POINTER to a
            // caller-owned per-call copy, never by value. The constructor's SIGNATURE already
            // carries that mapping -- declareFunctions builds every parameter through
            // abiParameterType -- so the call site has to agree, exactly like the four ordinary
            // call lowerers already do.
            //
            // Missing this was the bug: the call site loaded `[16 x i8]` and passed it by value
            // while the constructor declared `ptr`, and the LLVM verifier rejected the whole
            // module with "Call parameter type does not match function signature!". Measured
            // scope was every aggregate that does not fit in a register -- 12, 16, 20 and 64
            // bytes fail, 8 passes -- and it was SPECIFIC to constructors, because a constructor
            // is the one call shape that did not go through the mapping. It deformed the public
            // API of every type naturally built from a Matrix or a Vector4.
            value = wrapIndirectArgument(value, node->children[index + childOffset]->typeKey, error);
            if (value == nullptr) return false;
            evaluatedArguments.push_back(value);
        }
        std::vector<std::size_t> parameterToSource;
        if (!callArgumentOrder(node, evaluatedArguments.size(), parameterToSource, error)) return false;
        orderedArguments.reserve(evaluatedArguments.size());
        for (const std::size_t source : parameterToSource)
            orderedArguments.push_back(evaluatedArguments[source]);
        return true;
    }

LLVMValueRef OopLlvmImplementation::allocateConstructionStorage(
        const frontend::ProjectOopMirNode* node,
        LLVMBasicBlockRef& endBlock,
        std::string& error)
{
        LLVMValueRef size = LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0);
        LLVMValueRef object = nullptr;
        endBlock = nullptr;
        if (node->stackPromotedObject)
        {
            if (!node->localOrdinal.has_value())
                return failValue(error, "item 323 stack construction has no local identity");
            const auto storage = stackObjectStorage_.find(*node->localOrdinal);
            if (storage == stackObjectStorage_.end() || storage->second == nullptr)
                return failValue(error, "item 323 stack construction cannot find its frame storage");
            object = storage->second;
        }
        else
        {
            LLVMValueRef allocArgs[] = {size};
            object = buildNoUnwindCall(
                allocationFunctionType_, allocationFunction_, allocArgs, 1, "object");
            LLVMValueRef nullValue = LLVMConstNull(LLVMPointerTypeInContext(context_, 0));
            LLVMValueRef isNull = LLVMBuildICmp(
                builder_, LLVMIntEQ, object, nullValue, "alloc.failed");
            LLVMBasicBlockRef initBlock = appendBlock("object.init");
            endBlock = appendBlock("object.end");
            LLVMBuildCondBr(builder_, isNull, endBlock, initBlock);
            enterBlock(initBlock);
        }
        if (node->zeroInitialize)
        {
            LLVMValueRef zero = LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0);
            LLVMValueRef isVolatile = LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
            LLVMValueRef args[] = {object, zero, size, isVolatile};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, args, 4, "");
        }
        if (!node->targetTypeOrdinal.has_value())
        {
            return failValue(error, "Macro A allocation is missing its target class ordinal");
        }
        LLVMValueRef descriptorValue = nullptr;
        const auto descriptorAccessor = descriptorAccessorFunctions_.find(*node->targetTypeOrdinal);
        if (descriptorAccessor != descriptorAccessorFunctions_.end() &&
            runtimeDescriptorAccessorTypes_.contains(*node->targetTypeOrdinal))
        {
            descriptorValue = buildNoUnwindCall(
                descriptorAccessor->second.functionType,
                descriptorAccessor->second.function,
                nullptr,
                0,
                "classdesc");
        }
        else
        {
            const auto descriptor = descriptorGlobals_.find(*node->targetTypeOrdinal);
            if (descriptor != descriptorGlobals_.end()) descriptorValue = descriptor->second;
        }
        if (descriptorValue == nullptr)
        {
            return failValue(error, "Macro A allocation cannot resolve its class descriptor");
        }
        LLVMBuildStore(builder_, descriptorValue, object);
        return object;
    }

bool OopLlvmImplementation::invokeConstructionInitializer(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef object,
        const std::vector<LLVMValueRef>& orderedArguments,
        std::string& error)
{
        if (!node->calleeName.empty())
        {
            const auto target = functions_.find(node->calleeName);
            if (target == functions_.end())
                return fail(error, "Stage 34 constructor target missing: " + node->calleeName);
            std::vector<LLVMValueRef> ctorArgs;
            ctorArgs.reserve(1 + orderedArguments.size());
            ctorArgs.push_back(object);
            ctorArgs.insert(ctorArgs.end(), orderedArguments.begin(), orderedArguments.end());
            releaseTransferredArguments(node);
            bool provenNoUnwind = coroutine_.active && node->targetTypeOrdinal.has_value() &&
                node->memberOrdinal.has_value() && memberModel_ != nullptr &&
                *node->memberOrdinal < memberModel_->members().size() &&
                !staticInitFunctions_.contains(*node->targetTypeOrdinal);
            if (provenNoUnwind)
            {
                const auto& symbol = memberModel_->members()[*node->memberOrdinal];
                provenNoUnwind = symbol.kind == frontend::ClassMemberKind::Constructor &&
                    !symbol.isPrecompiled && !symbol.isImportedC && !symbol.isExternC &&
                    symbol.declaredErrorTypeKey.empty();
            }
            bool suspendedFinally = false;
            if (provenNoUnwind)
                for (const auto ordinal : activeCleanupRegions_)
                {
                    const auto region = cleanupRegions_.find(ordinal);
                    if (region != cleanupRegions_.end() && region->second != nullptr &&
                        region->second->kind == frontend::ProjectOopMirNodeKind::FinallyRegion &&
                        coroutine_.subtreeIds.contains(region->second)) suspendedFinally = true;
                }
            const auto* constructor = provenNoUnwind ? mirModel_.functionForMangledName(node->calleeName) : nullptr;
            provenNoUnwind = provenNoUnwind && suspendedFinally && constructor != nullptr &&
                constructor->constructor && constructor->body != nullptr && constructor->errorTypeKey.empty() &&
                !constructor->isImportedC && !constructor->isImportedNative && constructor->cleanupRegions.empty() &&
                constructor->declaringTypeOrdinal == *node->targetTypeOrdinal &&
                constructor->memberOrdinal == *node->memberOrdinal &&
                constructor->body->kind == frontend::ProjectOopMirNodeKind::Block;
            if (provenNoUnwind)
            {
                using Kind = frontend::ProjectOopMirNodeKind;
                for (const auto& statement : constructor->body->children)
                {
                    if (statement == nullptr || statement->kind != Kind::ExpressionStatement ||
                        statement->children.size() != 1U || statement->children[0] == nullptr)
                    { provenNoUnwind = false; break; }
                    const auto* store = statement->children[0].get();
                    if (store->kind != Kind::Store || store->typeKey != "builtin:int" ||
                        store->operation != lexer::TokenKind::Equal || store->children.size() != 2U ||
                        store->children[0] == nullptr || store->children[1] == nullptr)
                    { provenNoUnwind = false; break; }
                    const auto* field = store->children[0].get();
                    const auto* value = store->children[1].get();
                    if (field->kind != Kind::FieldAddress || field->typeKey != "builtin:int" ||
                        field->children.size() != 1U ||
                        field->children[0] == nullptr || field->children[0]->kind != Kind::ThisValue ||
                        !field->memberOrdinal.has_value() || *field->memberOrdinal >= memberModel_->members().size() ||
                        memberModel_->members()[*field->memberOrdinal].kind != frontend::ClassMemberKind::Field ||
                        memberModel_->members()[*field->memberOrdinal].declaringTypeOrdinal != *node->targetTypeOrdinal ||
                        memberModel_->members()[*field->memberOrdinal].isStatic ||
                        memberModel_->members()[*field->memberOrdinal].isVolatile ||
                        memberModel_->members()[*field->memberOrdinal].declaredTypeKey != "builtin:int" ||
                        value->typeKey != "builtin:int" ||
                        (value->kind != Kind::ParameterValue && value->kind != Kind::IntegerConstant) ||
                        !value->children.empty())
                    { provenNoUnwind = false; break; }
                }
            }
            LLVMValueRef call = provenNoUnwind
                ? buildNoUnwindCall(target->second.functionType, target->second.function,
                                    ctorArgs.data(), static_cast<unsigned>(ctorArgs.size()), "")
                : buildPotentiallyThrowingCall(
                    target->second.functionType, target->second.function,
                    ctorArgs.data(), static_cast<unsigned>(ctorArgs.size()), "", error);
            if (call == nullptr && !error.empty())
            {
                return false;
            }
        }

        // A construction node may carry synthesized instance-initializer work after its source
        // arguments.  These expressions conceptually execute against the just-allocated object;
        // they are not source arguments and therefore must not be evaluated before allocation.
        LLVMValueRef previousOverride = constructionThisOverride_;
        constructionThisOverride_ = object;
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            LLVMValueRef ignored = lowerExpression(node->children[index].get(), error);
            if (ignored == nullptr)
            {
                constructionThisOverride_ = previousOverride;
                return false;
            }
        }
        constructionThisOverride_ = previousOverride;
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (!node->stackPromotedObject && !node->allocationFailureReturnsNull)
        {
            return failValue(
                error,
                "Stage 34 LLVM lowering requires the frozen Stage 29 allocation-failure -> null contract");
        }
        if (node->argumentCount > node->children.size())
            return failValue(error, "Stage 37 construction MIR has an invalid source-argument count");
        std::vector<LLVMValueRef> orderedArguments;
        if (!collectConstructionArguments(
                node, 0, argumentOverrides, orderedArguments, error)) return nullptr;
        LLVMBasicBlockRef endBlock = nullptr;
        LLVMValueRef object = allocateConstructionStorage(node, endBlock, error);
        if (object == nullptr) return nullptr;
        if (!invokeConstructionInitializer(node, object, orderedArguments, error)) return nullptr;
        if (!node->stackPromotedObject)
        {
            if (!terminated()) LLVMBuildBr(builder_, endBlock);
            enterBlock(endBlock);
        }
        return object;
    }

bool OopLlvmImplementation::initializePlacementStorage(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        const frontend::NamedTypeDeclaration& targetType,
        std::string& error)
{
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        if (node->zeroInitialize)
        {
            LLVMValueRef args[] = {
                storage,
                LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
                LLVMConstInt(LLVMInt64TypeInContext(context_), node->immediate, 0),
                LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
            buildNoUnwindCall(memsetFunctionType_, memsetFunction_, args, 4, "");
        }

        if (targetType.kind == frontend::NamedTypeDeclarationKind::Class)
        {
            LLVMValueRef descriptorValue = nullptr;
            const auto descriptorAccessor =
                descriptorAccessorFunctions_.find(*node->targetTypeOrdinal);
            if (descriptorAccessor != descriptorAccessorFunctions_.end() &&
                runtimeDescriptorAccessorTypes_.contains(*node->targetTypeOrdinal))
            {
                descriptorValue = buildNoUnwindCall(
                    descriptorAccessor->second.functionType,
                    descriptorAccessor->second.function,
                    nullptr,
                    0,
                    "placement.classdesc");
            }
            else
            {
                const auto descriptor = descriptorGlobals_.find(*node->targetTypeOrdinal);
                if (descriptor != descriptorGlobals_.end()) descriptorValue = descriptor->second;
            }
            if (descriptorValue == nullptr)
                return fail(error, "item 318 placement construction cannot resolve its class descriptor");
            const bool hasCustomAllocator =
                node->allocatorContextLocalOrdinal.has_value() &&
                node->allocatorDeallocateMemberOrdinal.has_value() &&
                node->allocatorTypeOrdinal.has_value();
            if (hasCustomAllocator)
            {
                const auto found = localStorage_.find(*node->allocatorContextLocalOrdinal);
                if (found == localStorage_.end() || found->second == nullptr)
                    return fail(error,
                        "NS304-T13-F07 allocator provenance has no context storage");
                LLVMValueRef context = LLVMBuildLoad2(
                    builder_, pointerType, found->second, "allocator.context.load");
                // EN: the object was allocated with Allocate(sizeof(T), alignof(T)), which is the
                //     layout size and alignment this node carries (C-08).
                // ES: el objeto se pidio con Allocate(sizeof(T), alignof(T)), que son el tamano y
                //     la alineacion del layout que lleva este nodo (C-08).
                LLVMValueRef thunk = allocatorDeallocateThunk(
                    *node->allocatorTypeOrdinal,
                    node->allocatorDeallocateCalleeName,
                    node->immediate,
                    node->alignment,
                    error);
                if (thunk == nullptr) return false;
                if (!installAllocatorDescriptorSidecar(
                        storage, descriptorValue, context, thunk, error))
                    return false;
            }
            else
            {
                // Raw placement borrows storage. Its ordinary descriptor keeps field 7 available
                // for object-only destruction; cleanup chooses that entry from storageOwnership.
                LLVMValueRef descriptorStore = LLVMBuildStore(builder_, descriptorValue, storage);
                if (node->alignment > 1U)
                    LLVMSetAlignment(descriptorStore, static_cast<unsigned>(node->alignment));
            }
        }
        return true;
    }

bool OopLlvmImplementation::invokePlacementConstructor(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        const frontend::NamedTypeDeclaration& targetType,
        const std::vector<LLVMValueRef>& orderedArguments,
        std::string& error)
{
        if (!node->calleeName.empty())
        {
            const auto constructor = functions_.find(node->calleeName);
            if (constructor == functions_.end())
                return fail(error, "item 318 placement constructor target missing: " + node->calleeName);
            std::vector<LLVMValueRef> constructorArguments;
            constructorArguments.reserve(1 + orderedArguments.size());
            constructorArguments.push_back(storage);
            constructorArguments.insert(
                constructorArguments.end(), orderedArguments.begin(), orderedArguments.end());
            releaseTransferredArguments(node);
            LLVMBasicBlockRef allocatorFailureCleanup = nullptr;
            // Raw shared placement owns the object lifetime but borrows the storage. For the
            // restricted shape below, every owned reference field starts null and may be cleaned
            // on constructor unwind without invoking the incomplete object's own destructor.
            // Nontrivial value fields and base subobjects require separate liveness tracking.
            bool constructorThrows = false;
            for (const auto& function : mirModel_.functions())
                if (function.mangledName == node->calleeName)
                {
                    constructorThrows = !function.errorTypeKey.empty();
                    break;
                }
            bool sharedThrowingConstruction = false;
            bool sharedNonthrowingConstruction = false;
            if (currentMirFunction_ != nullptr && memberModel_ != nullptr &&
                currentMirFunction_->memberOrdinal < memberModel_->members().size() &&
                currentMirFunction_->declaringTypeOrdinal < nameLookup_.namedTypes().size())
            {
                const auto& currentMember = memberModel_->members()[currentMirFunction_->memberOrdinal];
                const auto& currentType = nameLookup_.namedTypes()[currentMirFunction_->declaringTypeOrdinal];
                sharedThrowingConstruction =
                    currentType.identity.fullyQualifiedName() == "NativeSharp.Core.Internal.SharedControlStorage" &&
                    (currentMember.name.starts_with("ConstructThrowing__g") ||
                     currentMember.name.starts_with("ConstructValueThrowing__g"));
                sharedNonthrowingConstruction =
                    currentType.identity.fullyQualifiedName() == "NativeSharp.Core.Internal.SharedControlStorage" &&
                    (currentMember.name.starts_with("Construct__g") ||
                     currentMember.name.starts_with("ConstructValue__g"));
            }
            if ((sharedThrowingConstruction || sharedNonthrowingConstruction) &&
                targetType.kind == frontend::NamedTypeDeclarationKind::Class)
            {
                if (targetType.isPrecompiled && (memberModel_ == nullptr ||
                    !memberModel_->isNonEscapingLifecycleTypeKey("type:" + targetType.identity.key())))
                    return fail(error, "shared payload class has no verifiable precompiled lifecycle body");
                std::unordered_set<std::size_t> lifecycleTypes;
                std::optional<std::size_t> type = *node->targetTypeOrdinal;
                while (type.has_value() && lifecycleTypes.insert(*type).second)
                {
                    if (*type >= nameLookup_.namedTypes().size())
                        return fail(error, "shared payload lifecycle type is unavailable");
                    const auto& currentType = nameLookup_.namedTypes()[*type];
                    if (currentType.isPrecompiled && (memberModel_ == nullptr ||
                        !memberModel_->isNonEscapingLifecycleTypeKey("type:" + currentType.identity.key())))
                        return fail(error, "shared payload class has no verifiable precompiled lifecycle body");
                    const auto* layout = mirModel_.classLayoutForType(*type);
                    type = layout == nullptr ? std::nullopt : layout->baseTypeOrdinal;
                }
                using ScanTarget = std::pair<const frontend::ProjectOopMirFunction*, std::optional<std::size_t>>;
                std::vector<ScanTarget> pending;
                for (const auto& function : mirModel_.functions())
                    if (lifecycleTypes.contains(function.declaringTypeOrdinal) &&
                        (function.constructor || function.destructor))
                        pending.emplace_back(&function, std::nullopt);
                std::unordered_map<const frontend::ProjectOopMirFunction*, std::unordered_set<std::size_t>> visited;
                std::unordered_map<std::string, std::unordered_set<std::size_t>> returnedSources;
                std::unordered_map<std::string,
                    std::unordered_map<const frontend::ProjectOopMirFunction*, std::unordered_set<std::size_t>>>
                    returnDependents;
                while (!pending.empty())
                {
                    const auto [function, sourceParameter] = pending.back();
                    pending.pop_back();
                    const std::size_t sourceKey = sourceParameter.value_or(static_cast<std::size_t>(-1));
                    if (!visited[function].insert(sourceKey).second) continue;
                    const ReceiverPublicationScan scan =
                        scanReceiverPublication(function->body.get(), sourceParameter, returnedSources, mirModel_);
                    if (scan.publishes)
                        return fail(error, "shared payload publishes its receiver during construction or destruction");
                    for (const auto& callee : scan.returnDependencies)
                        returnDependents[callee][function].insert(sourceKey);
                    if (scan.returnsSource && returnedSources[function->mangledName].insert(sourceKey).second)
                    {
                        const auto dependents = returnDependents.find(function->mangledName);
                        if (dependents != returnDependents.end())
                            for (const auto& [dependent, keys] : dependents->second)
                                for (const auto key : keys)
                                {
                                    visited[dependent].erase(key);
                                    pending.emplace_back(dependent, key == static_cast<std::size_t>(-1)
                                        ? std::nullopt : std::optional<std::size_t>(key));
                                }
                    }
                    for (const auto& [callee, memberOrdinal] : scan.directReceiverCallees)
                    {
                        const auto* target = mirModel_.functionForMangledName(callee);
                        if (target != nullptr && target->body != nullptr)
                        {
                            pending.emplace_back(target, std::nullopt);
                            continue;
                        }
                        // A constructor of a precompiled base is covered by the
                        // producer's lifecycle certificate checked above. No
                        // such certificate proves an arbitrary instance method.
                        bool certifiedBaseConstructor = false;
                        if (target != nullptr && memberOrdinal.has_value() &&
                            *memberOrdinal < memberModel_->members().size() &&
                            target->memberOrdinal == *memberOrdinal && target->mangledName == callee &&
                            target->constructor && function->constructor)
                        {
                            const auto& member = memberModel_->members()[*memberOrdinal];
                            const auto owner = member.declaringTypeOrdinal;
                            bool ownerIsAncestor = false;
                            std::unordered_set<std::size_t> traversed;
                            std::optional<std::size_t> current = function->declaringTypeOrdinal;
                            while (current.has_value() && traversed.insert(*current).second)
                            {
                                const auto* layout = mirModel_.classLayoutForType(*current);
                                if (layout == nullptr || !layout->baseTypeOrdinal.has_value()) break;
                                current = layout->baseTypeOrdinal;
                                if (*current == owner) { ownerIsAncestor = true; break; }
                            }
                            certifiedBaseConstructor = member.kind == frontend::ClassMemberKind::Constructor &&
                                ownerIsAncestor && lifecycleTypes.contains(owner) &&
                                owner < nameLookup_.namedTypes().size() && nameLookup_.namedTypes()[owner].isPrecompiled &&
                                memberModel_->isNonEscapingLifecycleTypeKey(
                                    "type:" + nameLookup_.namedTypes()[owner].identity.key());
                        }
                        if (!certifiedBaseConstructor)
                            return fail(error, "shared payload receiver invokes a callable with unverifiable body");
                    }
                    for (const auto& [callee, argument] : scan.directArgumentCallees)
                    {
                        const auto* target = mirModel_.functionForMangledName(callee);
                        if (target == nullptr || target->body == nullptr)
                            return fail(error, "shared payload receiver passed to a callable with unverifiable body");
                        pending.emplace_back(target, argument);
                    }
                    for (const auto& [receiverType, slot, argument] : scan.virtualArgumentCallees)
                        for (const auto& candidate : mirModel_.dispatchTables())
                        {
                            const auto* ancestor = &candidate;
                            while (ancestor != nullptr && ancestor->typeKey != receiverType)
                                ancestor = ancestor->baseTypeOrdinal.has_value()
                                    ? mirModel_.dispatchTableForType(*ancestor->baseTypeOrdinal) : nullptr;
                            if (ancestor != nullptr && slot < candidate.slotCalleeNames.size())
                                if (const auto* target = mirModel_.functionForMangledName(
                                        candidate.slotCalleeNames[slot]); target != nullptr)
                                    pending.emplace_back(target, argument);
                        }
                    for (const auto& [interfaceType, slot, argument] : scan.interfaceArgumentCallees)
                        for (const auto& candidate : mirModel_.dispatchTables())
                            for (const auto& binding : candidate.interfaceTables)
                                if (binding.interfaceTypeOrdinal == interfaceType && slot < binding.calleeNames.size())
                                    if (const auto* target = mirModel_.functionForMangledName(
                                            binding.calleeNames[slot]); target != nullptr)
                                        pending.emplace_back(target, argument);
                    for (const auto slot : scan.virtualReceiverSlots)
                        for (const auto typeOrdinal : lifecycleTypes)
                            if (const auto* table = mirModel_.dispatchTableForType(typeOrdinal);
                                table != nullptr && slot < table->slotCalleeNames.size())
                                if (const auto* target = mirModel_.functionForMangledName(table->slotCalleeNames[slot]);
                                    target != nullptr)
                                    pending.emplace_back(target, std::nullopt);
                    for (const auto& [interfaceType, slot] : scan.interfaceReceiverSlots)
                        for (const auto typeOrdinal : lifecycleTypes)
                            if (const auto* table = mirModel_.dispatchTableForType(typeOrdinal); table != nullptr)
                                for (const auto& binding : table->interfaceTables)
                                    if (binding.interfaceTypeOrdinal == interfaceType && slot < binding.calleeNames.size())
                                        if (const auto* target = mirModel_.functionForMangledName(
                                                binding.calleeNames[slot]); target != nullptr)
                                            pending.emplace_back(target, std::nullopt);
                }
            }
            // Follow the actual this/base initializer chain, including implicit
            // base work inlined into a constructor body.  A reachable source
            // constructor must expose its MIR body; a marker contributes only
            // the type whose field initializers it explicitly completes.
            const auto* sharedConstructor = mirModel_.functionForMangledName(node->calleeName);
            bool sharedConstructorCleansFields = sharedThrowingConstruction && constructorThrows &&
                memberModel_ != nullptr &&
                sharedConstructor != nullptr && sharedConstructor->constructor &&
                node->targetTypeOrdinal.has_value() &&
                sharedConstructor->declaringTypeOrdinal == *node->targetTypeOrdinal;
            if (sharedConstructorCleansFields)
            {
                std::unordered_set<std::size_t> sourceTypes;
                std::optional<std::size_t> importedBaseType;
                bool importedBaseConstructorSeen = false;
                std::optional<std::size_t> type = *node->targetTypeOrdinal;
                while (type.has_value() && sharedConstructorCleansFields)
                {
                    if (*type >= nameLookup_.namedTypes().size() ||
                        !sourceTypes.insert(*type).second)
                    {
                        sharedConstructorCleansFields = false;
                        break;
                    }
                    if (nameLookup_.namedTypes()[*type].isPrecompiled)
                    {
                        importedBaseType = *type;
                        sourceTypes.erase(*type);
                        break;
                    }
                    const auto* layout = mirModel_.classLayoutForType(*type);
                    if (layout == nullptr) break; // A struct has no class bases.
                    type = layout->baseTypeOrdinal;
                }
                std::unordered_set<std::size_t> coveredTypes;
                std::unordered_set<const frontend::ProjectOopMirFunction*> visited;
                std::vector<const frontend::ProjectOopMirFunction*> pending{sharedConstructor};
                while (!pending.empty() && sharedConstructorCleansFields)
                {
                    const auto* function = pending.back();
                    pending.pop_back();
                    if (function == nullptr || !function->constructor ||
                        function->body == nullptr ||
                        function->body->kind != frontend::ProjectOopMirNodeKind::Block ||
                        !sourceTypes.contains(function->declaringTypeOrdinal) ||
                        !visited.insert(function).second)
                    {
                        sharedConstructorCleansFields = false;
                        break;
                    }
                    coveredTypes.insert(function->declaringTypeOrdinal);
                    for (const auto& statement : function->body->children)
                    {
                        if (statement == nullptr) continue;
                        if (statement->stringValue == "constructor.implicit.base.completed")
                        {
                            if (!statement->targetTypeOrdinal.has_value() ||
                                !sourceTypes.contains(*statement->targetTypeOrdinal))
                            {
                                sharedConstructorCleansFields = false;
                                break;
                            }
                            coveredTypes.insert(*statement->targetTypeOrdinal);
                        }
                        const auto* call = statement.get();
                        if (call->kind == frontend::ProjectOopMirNodeKind::FullExpressionCleanup &&
                            call->children.size() == 1U) call = call->children.front().get();
                        if (call == nullptr ||
                            call->kind != frontend::ProjectOopMirNodeKind::DirectInstanceCall ||
                            !call->memberOrdinal.has_value() ||
                            *call->memberOrdinal >= memberModel_->members().size() ||
                            memberModel_->members()[*call->memberOrdinal].kind !=
                                frontend::ClassMemberKind::Constructor ||
                            call->children.empty() || call->children.front() == nullptr ||
                            call->children.front()->kind != frontend::ProjectOopMirNodeKind::ThisValue)
                            continue;
                        const auto& member = memberModel_->members()[*call->memberOrdinal];
                        if (member.declaringTypeOrdinal >= nameLookup_.namedTypes().size())
                        {
                            sharedConstructorCleansFields = false;
                            break;
                        }
                        if (nameLookup_.namedTypes()[member.declaringTypeOrdinal].isPrecompiled)
                        {
                            if (!call->baseReceiver || !importedBaseType.has_value() ||
                                member.declaringTypeOrdinal != *importedBaseType ||
                                !member.declaredErrorTypeKey.empty())
                            {
                                sharedConstructorCleansFields = false;
                                break;
                            }
                            importedBaseConstructorSeen = true;
                            continue;
                        }
                        const auto* callee = mirModel_.functionForMangledName(call->calleeName);
                        if (callee == nullptr || callee->memberOrdinal != *call->memberOrdinal ||
                            callee->declaringTypeOrdinal != member.declaringTypeOrdinal)
                        {
                            sharedConstructorCleansFields = false;
                            break;
                        }
                        pending.push_back(callee);
                    }
                }
                for (const std::size_t sourceType : sourceTypes)
                {
                    if (!coveredTypes.contains(sourceType) ||
                        (sourceType != *node->targetTypeOrdinal &&
                         !objectDestructionFunctions_.contains(sourceType)))
                        sharedConstructorCleansFields = false;
                }
                if (importedBaseType.has_value() &&
                    !memberModel_->isTrivialCleanupRootTypeKey(
                        "type:" + nameLookup_.namedTypes()[*importedBaseType].identity.key()) &&
                    !importedBaseConstructorSeen)
                    sharedConstructorCleansFields = false;
            }
            bool safePartialCleanup = false;
            if ((constructorThrows || sharedNonthrowingConstruction) &&
                node->zeroInitialize && memberModel_ != nullptr && !targetType.isPrecompiled &&
                !node->allocatorContextLocalOrdinal.has_value())
            {
                const std::vector<frontend::ObjectFieldLayout>* fields = nullptr;
                bool simple = false;
                if (targetType.kind == frontend::NamedTypeDeclarationKind::Class)
                {
                    for (const auto& layout : mirModel_.classLayouts())
                        if (layout.typeOrdinal == *node->targetTypeOrdinal)
                        {
                            fields = &layout.fields;
                            simple = true;
                            // Throwing shared construction uses the source constructor's cleanup
                            // regions for completed bases and owned fields. The nonthrowing path
                            // retains its earlier conservative test. Value fields needing drop
                            // require an established partial lifetime below.
                            std::optional<std::size_t> base = layout.baseTypeOrdinal;
                            while (base.has_value() && simple)
                            {
                                if (*base >= nameLookup_.namedTypes().size())
                                {
                                    simple = false;
                                    break;
                                }
                                const auto& baseType = nameLookup_.namedTypes()[*base];
                                if (baseType.isPrecompiled)
                                {
                                    const std::string baseKey = "type:" + baseType.identity.key();
                                    simple = memberModel_->isTrivialCleanupRootTypeKey(baseKey) ||
                                        (sharedConstructorCleansFields &&
                                         memberModel_->isNonEscapingLifecycleTypeKey(baseKey) &&
                                         descriptorAccessorFunctions_.contains(*base));
                                    break;
                                }
                                const frontend::ProjectOopMirClassLayout* baseLayout =
                                    mirModel_.classLayoutForType(*base);
                                if (baseLayout == nullptr) { simple = false; break; }
                                if (!sharedThrowingConstruction && memberModel_->hasMembersDeclaredByTypeIndex())
                                {
                                    for (const std::size_t ordinal : memberModel_->membersDeclaredByType(*base))
                                        if (memberModel_->members()[ordinal].kind ==
                                            frontend::ClassMemberKind::Destructor)
                                            { simple = false; break; }
                                }
                                else if (!sharedThrowingConstruction)
                                {
                                    for (const auto& member : memberModel_->members())
                                        if (member.declaringTypeOrdinal == *base &&
                                            member.kind == frontend::ClassMemberKind::Destructor)
                                            { simple = false; break; }
                                }
                                for (const auto& field : baseLayout->fields)
                                {
                                    if (field.memberOrdinal >= memberModel_->members().size())
                                        { simple = false; break; }
                                    const bool owned = memberModel_->members()[field.memberOrdinal].declaredOwnership ==
                                        sema::OwnershipKind::Owned;
                                    if ((owned && (!sharedThrowingConstruction ||
                                                   !requiresOwnedDestruction(field.typeKey))) ||
                                        (memberModel_->typeRequiresValueDrop(field.typeKey) &&
                                         !(sharedConstructorCleansFields && sharedEmptyDefault(field.typeKey))))
                                        { simple = false; break; }
                                }
                                base = baseLayout->baseTypeOrdinal;
                            }
                            break;
                        }
                }
                else if (targetType.kind == frontend::NamedTypeDeclarationKind::Struct)
                {
                    for (const auto& layout : mirModel_.structLayouts())
                        if (layout.typeOrdinal == *node->targetTypeOrdinal)
                        {
                            fields = &layout.fields;
                            simple = !layout.isUnion && !layout.hasExplicitFieldOffsets;
                            break;
                        }
                }
                std::vector<const frontend::ObjectFieldLayout*> ownedFields;
                if (fields != nullptr && simple)
                {
                    for (const auto& field : *fields)
                    {
                        if (field.memberOrdinal >= memberModel_->members().size()) { simple = false; break; }
                        const auto& symbol = memberModel_->members()[field.memberOrdinal];
                        const bool ownedPointer = symbol.declaredOwnership == sema::OwnershipKind::Owned &&
                            requiresOwnedDestruction(field.typeKey);
                        if (ownedPointer) ownedFields.push_back(&field);
                        else if (memberModel_->typeRequiresValueDrop(field.typeKey) &&
                                 !(sharedConstructorCleansFields &&
                                   (sharedEmptyDefault(field.typeKey) ||
                                    simpleValueFieldConstructor(
                                        sharedConstructor, *fields, *memberModel_, mirModel_))))
                            { simple = false; break; }
                    }
                }
                safePartialCleanup = fields != nullptr && simple;
                if (simple && !ownedFields.empty())
                {
                    LLVMBasicBlockRef outerUnwind = buildCurrentUnwindDestination(error);
                    if (!error.empty()) return false;
                    LLVMBasicBlockRef sourceBlock = LLVMGetInsertBlock(builder_);
                    allocatorFailureCleanup = appendBlock("placement.fields.failure");
                    enterBlock(allocatorFailureCleanup);
                    LLVMValueRef cleanupPad = LLVMBuildCleanupPad(
                        builder_, activeFuncletPad(), nullptr, 0, "placement.fields.failure.pad");
                    if (cleanupPad == nullptr) return fail(error, "placement field cleanup has no pad");
                    funcletStack_.push_back(FuncletContext{
                        cleanupPad, FuncletKind::Cleanup, activeCleanupRegions_.size(), nullptr, outerUnwind});
                    LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
                    for (auto it = ownedFields.rbegin(); it != ownedFields.rend(); ++it)
                    {
                        const auto& field = **it;
                        LLVMValueRef fieldAddress = byteOffset(storage, field.offset, "placement.failure.field.addr");
                        LLVMValueRef value = LLVMBuildLoad2(builder_, ptr, fieldAddress, "placement.failure.field.load");
                        LLVMBuildStore(builder_, LLVMConstNull(ptr), fieldAddress);
                        if (!emitOwnedPointerDestroy(value, field.typeKey, "placement.failure.field", error))
                            return false;
                    }
                    LLVMBuildCleanupRet(builder_, cleanupPad, outerUnwind);
                    funcletStack_.pop_back();
                    enterBlock(sourceBlock);
                    LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);
                }
            }
            if (sharedThrowingConstruction && constructorThrows && !safePartialCleanup)
                return fail(error, "shared throwing construction cannot safely clean this payload shape");
            if (targetType.kind == frontend::NamedTypeDeclarationKind::Class &&
                node->allocatorContextLocalOrdinal.has_value() &&
                node->allocatorDeallocateMemberOrdinal.has_value() &&
                node->allocatorTypeOrdinal.has_value())
            {
                LLVMBasicBlockRef outerUnwind = buildCurrentUnwindDestination(error);
                if (!error.empty()) return false;
                allocatorFailureCleanup = emitAllocatorConstructionFailureCleanup(
                    storage, outerUnwind, error);
                if (allocatorFailureCleanup == nullptr) return false;
                LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);
            }
            if (allocatorFailureCleanup != nullptr)
            {
                LLVMBasicBlockRef normalBlock = appendBlock("placement.ctor.cont");
                LLVMValueRef invoke = buildInvokeInCurrentFunclet(
                    constructor->second.functionType,
                    constructor->second.function,
                    constructorArguments.data(),
                    static_cast<unsigned>(constructorArguments.size()),
                    normalBlock,
                    allocatorFailureCleanup,
                    "");
                if (invoke == nullptr)
                    return fail(error,
                        "NS304-T13-F07 failed to emit allocator constructor invoke");
                enterBlock(normalBlock);
            }
            else if (buildPotentiallyThrowingCall(
                         constructor->second.functionType,
                         constructor->second.function,
                         constructorArguments.data(),
                         static_cast<unsigned>(constructorArguments.size()),
                         "",
                         error) == nullptr && !error.empty())
            {
                return false;
            }
        }
        return true;
    }

bool OopLlvmImplementation::runPlacementInitializers(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error)
{
        LLVMValueRef previousOverride = constructionThisOverride_;
        constructionThisOverride_ = storage;
        for (std::size_t index = node->argumentCount + 1; index < node->children.size(); ++index)
        {
            if (lowerExpression(node->children[index].get(), error) == nullptr)
            {
                constructionThisOverride_ = previousOverride;
                return false;
            }
        }
        constructionThisOverride_ = previousOverride;
        return true;
    }

LLVMValueRef OopLlvmImplementation::lowerPlacementConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef storageOverride ,
        const std::vector<LLVMValueRef>* argumentOverrides )
{
        if (node->children.empty() || node->children.front() == nullptr)
            return failValue(error, "item 318 placement construction requires destination storage");
        if (node->immediate == 0 || node->alignment == 0)
            return failValue(error, "item 318 placement construction has no target layout");
        if (node->argumentCount + 1 > node->children.size())
            return failValue(error, "item 318 placement construction has an invalid argument count");
        // The destination is evaluated exactly once and before every constructor argument. Its
        // semantic type is exact T*, so no conversion or hidden temporary can substitute another
        // location after this point.
        LLVMValueRef storage = storageOverride != nullptr
            ? storageOverride : lowerExpression(node->children.front().get(), error);
        if (storage == nullptr) return nullptr;
        std::vector<LLVMValueRef> orderedArguments;
        if (!collectConstructionArguments(
                node, 1, argumentOverrides, orderedArguments, error)) return nullptr;
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef notNull = LLVMBuildICmp(
            builder_, LLVMIntNE, storage, LLVMConstNull(pointerType), "placement.notnull");
        if (!emitGuard(notNull, LlvmTrapReason::NullReference, "placement.storage", error))
            return nullptr;
        if (!node->targetTypeOrdinal.has_value() ||
            *node->targetTypeOrdinal >= nameLookup_.namedTypes().size())
            return failValue(error, "item 318 placement construction has no target type ordinal");
        const frontend::NamedTypeDeclaration& targetType =
            nameLookup_.namedTypes()[*node->targetTypeOrdinal];
        if (!initializePlacementStorage(node, storage, targetType, error)) return nullptr;
        if (!invokePlacementConstructor(
                node, storage, targetType, orderedArguments, error)) return nullptr;
        if (!runPlacementInitializers(node, storage, error)) return nullptr;
        // NS304-T13-F06: a trailing construction exposes the header struct BY VALUE; load it from
        // the allocated pointer so the caller's value slot receives the initialized header.
        if (targetType.kind == frontend::NamedTypeDeclarationKind::Struct &&
            !node->typeKey.ends_with("*"))
        {
            LLVMTypeRef valueType = llvmType(node->typeKey, error);
            if (valueType == nullptr)
                return failValue(error, "trailing construction has no value layout");
            return LLVMBuildLoad2(builder_, valueType, storage, "trailing.header");
        }
        return storage;
    }

bool OopLlvmImplementation::emitObjectOnlyValueDestruction(
        LLVMValueRef address, std::string_view pointerTypeKey, std::string& error)
{
        if (!pointerTypeKey.ends_with('*')) return true;
        const std::string_view valueType = pointerTypeKey.substr(0, pointerTypeKey.size() - 1U);
        if (memberModel_ == nullptr || !memberModel_->typeRequiresValueDrop(valueType)) return true;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, address, LLVMConstNull(ptr), "destroy.value.isnull");
        LLVMBasicBlockRef destroyBlock = appendBlock("destroy.value");
        LLVMBasicBlockRef doneBlock = appendBlock("destroy.value.done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);
        enterBlock(destroyBlock);
        if (!emitValueDropAddress(address, valueType, "destroy.value", error)) return false;
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);
        enterBlock(doneBlock);
        return true;
}

LLVMValueRef OopLlvmImplementation::lowerObjectDestruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() != 1)
            return failValue(error, "item 319 object destruction requires one operand");

        LLVMValueRef object = lowerExpression(node->children.front().get(), error);
        if (object == nullptr) return nullptr;

        // Value drop runs in place and does not release the borrowed storage.
        if (!node->dynamicDestruction)
        {
            if (!emitObjectOnlyValueDestruction(object, node->elementTypeKey, error)) return nullptr;
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
            return failValue(error, "item 319 object destruction has no descriptor ABI");

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, object, LLVMConstNull(ptr), "destroy.object.isnull");
        LLVMBasicBlockRef destroyBlock = appendBlock("destroy.object");
        LLVMBasicBlockRef doneBlock = appendBlock("destroy.object.done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);

        enterBlock(destroyBlock);
        LLVMValueRef descriptor = LLVMBuildLoad2(
            builder_, ptr, object, "destroy.object.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 7, "destroy.object.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress, "destroy.object.entry");
        LLVMValueRef arguments[] = {object};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return nullptr;
        }
        if (!terminated()) LLVMBuildBr(builder_, doneBlock);

        enterBlock(doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

LLVMValueRef OopLlvmImplementation::lowerDeallocation(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef loweredOperandOverride )
{
        if (node->children.size() != 1)
        {
            return failValue(error, "Stage 35 deallocation MIR requires one operand");
        }
        LLVMValueRef loweredOperand = loweredOperandOverride != nullptr
            ? loweredOperandOverride
            : lowerExpression(node->children.front().get(), error);
        if (loweredOperand == nullptr)
        {
            return nullptr;
        }

        // Stage 52: `delete <value-typed expression>;` (e.g. an int-valued generic collection
        // element) was already evaluated above for its side effects (array-index bounds checks
        // in particular), but its result is not a heap pointer at all -- there is nothing to
        // free. Discard it rather than passing a non-pointer bit pattern to the runtime
        // allocator, which would misbehave or fail LLVM verification.
        if (node->valueTypeNoOpDeletion)
        {
            // EN: Freeze 3.0 C-12: an addressed struct element is dropped in place, then zeroed.
            // ES: Congelamiento 3.0 C-12: un elemento struct con direccion se destruye y queda en cero.
            if (node->booleanValue &&
                !emitValueElementDelete(
                    loweredOperand, node->elementTypeKey,
                    takeArrayElementLiveness(node->children.front().get()), error))
                return nullptr;
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        LLVMValueRef pointer = loweredOperand;
        if (node->booleanValue)
        {
            LLVMTypeRef storedType = llvmType(node->elementTypeKey, error);
            if (storedType == nullptr) return nullptr;
            pointer = LLVMBuildLoad2(
                builder_, storedType, loweredOperand, "delete.element.load");
            // Clear before calling user destruction.  If a destructor transfers control through
            // EH, every cleanup path already observes an empty owner slot.
            LLVMBuildStore(builder_, LLVMConstNull(storedType), loweredOperand);
        }

        if (node->children.front() != nullptr && isDelegateReferenceType(nameLookup_, node->children.front()->typeKey))
        {
            if (!emitDelegateDestroy(pointer, "delete.delegate", error)) return nullptr;
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        const std::string_view deletionType = node->booleanValue && !node->elementTypeKey.empty()
            ? std::string_view(node->elementTypeKey)
            : std::string_view(node->children.front()->typeKey);
        if (frontend::ProjectTypeConversion::isNativeArray(deletionType))
        {
            if (!emitNativeArrayDestroy(
                    pointer,
                    deletionType,
                    "delete.array",
                    node->storageOwnership == sema::OwnershipKind::Owned,
                    error)) return nullptr;
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        if (!node->dynamicDestruction)
        {
            LLVMValueRef arguments[] = {pointer};
            buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        }

        if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
        {
            return failValue(error, "Stage 35 dynamic delete has no class-descriptor destruction ABI");
        }

        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, pointer, LLVMConstNull(ptr), "delete.isnull");
        LLVMBasicBlockRef destroyBlock = appendBlock("delete.destroy");
        LLVMBasicBlockRef doneBlock = appendBlock("delete.done");
        LLVMBuildCondBr(builder_, isNull, doneBlock, destroyBlock);

        enterBlock(destroyBlock);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, pointer, "delete.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 5, "delete.entry.addr");
        LLVMValueRef destructionEntry = LLVMBuildLoad2(
            builder_, ptr, entryAddress, "delete.entry");
        LLVMValueRef arguments[] = {pointer};
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty())
        {
            return nullptr;
        }
        if (!terminated())
        {
            LLVMBuildBr(builder_, doneBlock);
        }

        enterBlock(doneBlock);
        return LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
    }

}
