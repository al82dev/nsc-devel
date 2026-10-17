#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include "nativesharp/Language/CoroutineSemantics.h"

#include <algorithm>
#include <limits>
#include <string_view>

namespace nativesharp::frontend
{
namespace
{
ProjectOopMirAbiPassing abiPassingForSize(std::uint64_t size) noexcept
{
    switch (size)
    {
        case 1: case 2: case 4: case 8: return ProjectOopMirAbiPassing::Direct;
        default: return ProjectOopMirAbiPassing::Indirect;
    }
}

std::string sanitizeDispatchName(std::string_view value)
{
    return compilation::symbolFragmentForIdentityKey(value);
}
}

bool ProjectOopMirModel::importLayouts(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectOopHirModel& hirModel,
    std::string& error)
{
    for (const StructValueLayout& layout : objectLayoutModel.structLayouts())
    {
        const bool imported = layout.typeOrdinal < nameLookup.namedTypes().size() &&
            nameLookup.namedTypes()[layout.typeOrdinal].isPrecompiled;
        const bool variantPayloadCarrier = layout.isUnion && std::ranges::any_of(
            layout.fields, [&](const ObjectFieldLayout& field)
            {
                return field.memberOrdinal < memberModel.members().size() &&
                    memberModel.members()[field.memberOrdinal].name.starts_with("__nsc_case_");
            });
        const bool requiresDrop = !variantPayloadCarrier &&
            memberModel.typeRequiresValueDrop(layout.typeKey);
        const std::string dropGlueEntryName = requiresDrop
            ? "__nsc_drop_value_" + sanitizeDispatchName(layout.typeKey)
            : std::string{};
        structLayouts_.push_back(ProjectOopMirStructLayout{
            layout.typeOrdinal, layout.typeKey, layout.size, layout.alignment,
            layout.isUnion, layout.pack, layout.minimumAlignment,
            layout.hasExplicitFieldOffsets, abiPassingForSize(layout.size),
            imported, requiresDrop, dropGlueEntryName, layout.fields});
    }
    // Structural tuples do not have named-type ordinals, but from MIR onward they obey the same
    // Win64 value-carrier and ABI rules as declared structs. Discover every tuple key used by
    // HIR (including nested element tuples), sort the keys for reproducible dumps, and publish
    // synthetic physical layouts in the ordinary struct-layout catalogue.
    std::vector<std::string> tupleKeys;
    const auto collectTypeKey = [&](const auto& self, std::string_view key) -> void
    {
        const std::optional<std::vector<std::string>> elements =
            ProjectTypeConversion::tupleElementTypeKeys(key);
        if (!elements.has_value()) return;
        tupleKeys.emplace_back(key);
        for (const std::string& element : *elements) self(self, element);
    };
    const auto collectNode = [&](const auto& self, const ProjectOopHirNode* node) -> void
    {
        if (node == nullptr) return;
        collectTypeKey(collectTypeKey, node->typeKey);
        collectTypeKey(collectTypeKey, node->elementTypeKey);
        for (const std::string& key : node->argumentTypeKeys) collectTypeKey(collectTypeKey, key);
        for (const auto& child : node->children) self(self, child.get());
    };
    for (const ProjectOopHirFunction& function : hirModel.functions())
    {
        collectTypeKey(collectTypeKey, function.returnTypeKey);
        for (const std::string& key : function.parameterTypeKeys) collectTypeKey(collectTypeKey, key);
        for (const std::string& key : function.captureTypeKeys) collectTypeKey(collectTypeKey, key);
        collectNode(collectNode, function.body.get());
    }
    std::ranges::sort(tupleKeys);
    tupleKeys.erase(std::unique(tupleKeys.begin(), tupleKeys.end()), tupleKeys.end());
    for (const std::string& key : tupleKeys)
    {
        const std::optional<std::vector<std::string>> elements =
            ProjectTypeConversion::tupleElementTypeKeys(key);
        const std::optional<language::AggregateLayout> layout =
            objectLayoutModel.tupleLayoutForTypeKey(nameLookup, key);
        if (!elements.has_value() || !layout.has_value() ||
            elements->size() != layout->offsets.size())
        {
            error = "NativeSharp 3.0 tuple has no deterministic physical layout: " + key;
            return false;
        }
        std::vector<ObjectFieldLayout> fields;
        fields.reserve(elements->size());
        for (std::size_t index = 0; index < elements->size(); ++index)
        {
            const auto elementLayout = objectLayoutModel.valueLayoutForTypeKey(
                nameLookup, (*elements)[index]);
            if (!elementLayout.has_value())
            {
                error = "NativeSharp 3.0 tuple element has no physical layout: " +
                    (*elements)[index];
                return false;
            }
            fields.push_back(ObjectFieldLayout{
                index,
                static_cast<std::uint64_t>(layout->offsets[index]),
                elementLayout->first,
                elementLayout->second,
                (*elements)[index],
                std::nullopt,
                std::nullopt});
        }
        structLayouts_.push_back(ProjectOopMirStructLayout{
            (std::numeric_limits<std::size_t>::max)(),
            key,
            static_cast<std::uint64_t>(layout->size),
            static_cast<std::uint64_t>(layout->alignment),
            false,
            0,
            0,
            false,
            abiPassingForSize(layout->size),
            false,
            memberModel.typeRequiresValueDrop(key),
            {},
            std::move(fields)});
    }
    for (const ClassObjectLayout& layout : objectLayoutModel.classLayouts())
    {
        classLayouts_.push_back(ProjectOopMirClassLayout{
            layout.typeOrdinal,
            layout.typeKey,
            layout.size,
            layout.alignment,
            layout.headerSize,
            layout.baseTypeOrdinal,
            layout.baseSubobjectSize,
            layout.referenceSize,
            layout.referenceAlignment,
            layout.fields});
    }
    // NS304-T05-F03: `future:<T>` es la superficie resoluble del future opaco de una funcion
    // async. Layout uniforme { i32 state; void* frame; T result }: el discriminante, el puntero al
    // frame del caller (sin heap implicito) y el resultado. Se descubre igual que los tuples.
    std::vector<std::string> futureKeys;
    const auto collectFutureKey = [&](std::string_view key)
    {
        if (language::futureInnerTypeKey(key).has_value()) futureKeys.emplace_back(key);
    };
    const auto collectFutureNode = [&](const auto& self, const ProjectOopHirNode* node) -> void
    {
        if (node == nullptr) return;
        collectFutureKey(node->typeKey);
        collectFutureKey(node->elementTypeKey);
        for (const std::string& key : node->argumentTypeKeys) collectFutureKey(key);
        for (const auto& child : node->children) self(self, child.get());
    };
    for (const ProjectOopHirFunction& function : hirModel.functions())
    {
        collectFutureKey(function.returnTypeKey);
        for (const std::string& key : function.parameterTypeKeys) collectFutureKey(key);
        for (const std::string& key : function.captureTypeKeys) collectFutureKey(key);
        collectFutureNode(collectFutureNode, function.body.get());
    }
    std::ranges::sort(futureKeys);
    futureKeys.erase(std::unique(futureKeys.begin(), futureKeys.end()), futureKeys.end());
    const auto alignUp = [](std::uint64_t value, std::uint64_t alignment) -> std::uint64_t
    {
        if (alignment <= 1) return value;
        return (value + alignment - 1) / alignment * alignment;
    };
    for (const std::string& key : futureKeys)
    {
        const std::optional<std::string_view> inner = language::futureInnerTypeKey(key);
        // NS304-T05-F07: a throwing future (`future:<T>|<E>`) carries an error slot after result.
        const std::optional<std::string_view> errorInner = language::futureErrorTypeKey(key);
        const std::optional<std::pair<std::uint64_t, std::uint64_t>> resultLayout =
            (inner.has_value() && *inner == "builtin:void")
                ? std::optional<std::pair<std::uint64_t, std::uint64_t>>(
                      std::pair<std::uint64_t, std::uint64_t>{0U, 1U})
                : objectLayoutModel.valueLayoutForTypeKey(
                      nameLookup, inner.has_value() ? std::string(*inner) : std::string{});
        const std::optional<std::pair<std::uint64_t, std::uint64_t>> errorLayout =
            errorInner.has_value()
                ? objectLayoutModel.valueLayoutForTypeKey(nameLookup, std::string(*errorInner))
                : std::optional<std::pair<std::uint64_t, std::uint64_t>>(
                      std::pair<std::uint64_t, std::uint64_t>{0U, 1U});
        if (!inner.has_value() || !resultLayout.has_value() || !errorLayout.has_value())
        {
            error = "NativeSharp async future has no physical layout: " + key;
            return false;
        }
        constexpr std::uint64_t stateSize = 4;
        constexpr std::uint64_t stateAlignment = 4;
        constexpr std::uint64_t frameSize = 8;
        constexpr std::uint64_t frameAlignment = 8;
        const std::uint64_t frameOffset = alignUp(stateSize, frameAlignment);
        const std::uint64_t resultOffset = alignUp(frameOffset + frameSize, resultLayout->second);
        // NS304-T05-F09: the future carries its poll function pointer (index 3) so `await` over a
        // variable can resolve the poll without a direct call site.
        constexpr std::uint64_t pollFnSize = 8;
        constexpr std::uint64_t pollFnAlignment = 8;
        const std::uint64_t pollFnOffset = alignUp(resultOffset + resultLayout->first, pollFnAlignment);
        const std::uint64_t errorOffset = errorInner.has_value() && errorLayout->first > 0
            ? alignUp(pollFnOffset + pollFnSize, errorLayout->second)
            : 0;
        const std::uint64_t totalAlignment =
            std::max({stateAlignment, frameAlignment, resultLayout->second, pollFnAlignment,
                      errorLayout->second});
        const std::uint64_t lastFieldEnd = errorInner.has_value() && errorLayout->first > 0
            ? errorOffset + errorLayout->first
            : pollFnOffset + pollFnSize;
        const std::uint64_t totalSize = alignUp(lastFieldEnd, totalAlignment);
        std::vector<ObjectFieldLayout> fields;
        fields.push_back(ObjectFieldLayout{
            0, 0, stateSize, stateAlignment, "builtin:int", std::nullopt, std::nullopt});
        fields.push_back(ObjectFieldLayout{
            1, frameOffset, frameSize, frameAlignment, "builtin:void*", std::nullopt, std::nullopt});
        fields.push_back(ObjectFieldLayout{
            2, resultOffset, resultLayout->first, resultLayout->second,
            std::string(*inner), std::nullopt, std::nullopt});
        fields.push_back(ObjectFieldLayout{
            3, pollFnOffset, pollFnSize, pollFnAlignment, "builtin:void*", std::nullopt, std::nullopt});
        if (errorInner.has_value() && errorLayout->first > 0)
        {
            fields.push_back(ObjectFieldLayout{
                4, errorOffset, errorLayout->first, errorLayout->second,
                std::string(*errorInner), std::nullopt, std::nullopt});
        }
        structLayouts_.push_back(ProjectOopMirStructLayout{
            (std::numeric_limits<std::size_t>::max)(),
            key,
            totalSize,
            totalAlignment,
            false,
            0,
            0,
            false,
            abiPassingForSize(totalSize),
            false,
            false,
            {},
            std::move(fields)});
    }
    // NS304-T06-F03: `iterator:<T>` es la superficie resoluble del iterator opaco de un generator
    // sincrono. Layout uniforme { i32 state; void* frame; T item }, sin heap implicito.
    std::vector<std::string> iteratorKeys;
    const auto collectIteratorKey = [&](std::string_view key)
    {
        if (language::iteratorInnerTypeKey(key).has_value() ||
            language::asyncIteratorInnerTypeKey(key).has_value())
            iteratorKeys.emplace_back(key);
    };
    const auto collectIteratorNode = [&](const auto& self, const ProjectOopHirNode* node) -> void
    {
        if (node == nullptr) return;
        collectIteratorKey(node->typeKey);
        collectIteratorKey(node->elementTypeKey);
        for (const std::string& key : node->argumentTypeKeys) collectIteratorKey(key);
        for (const auto& child : node->children) self(self, child.get());
    };
    for (const ProjectOopHirFunction& function : hirModel.functions())
    {
        collectIteratorKey(function.returnTypeKey);
        for (const std::string& key : function.parameterTypeKeys) collectIteratorKey(key);
        for (const std::string& key : function.captureTypeKeys) collectIteratorKey(key);
        collectIteratorNode(collectIteratorNode, function.body.get());
    }
    std::ranges::sort(iteratorKeys);
    iteratorKeys.erase(std::unique(iteratorKeys.begin(), iteratorKeys.end()), iteratorKeys.end());
    for (const std::string& key : iteratorKeys)
    {
        auto item = language::iteratorInnerTypeKey(key);
        if (!item.has_value()) item = language::asyncIteratorInnerTypeKey(key);
        const std::optional<std::pair<std::uint64_t, std::uint64_t>> itemLayout =
            (item.has_value() && *item == "builtin:void")
                ? std::optional<std::pair<std::uint64_t, std::uint64_t>>(
                      std::pair<std::uint64_t, std::uint64_t>{0U, 1U})
                : objectLayoutModel.valueLayoutForTypeKey(
                      nameLookup, item.has_value() ? std::string(*item) : std::string{});
        if (!item.has_value() || !itemLayout.has_value())
        {
            error = "NativeSharp generator iterator has no physical layout: " + key;
            return false;
        }
        constexpr std::uint64_t stateSize = 4;
        constexpr std::uint64_t stateAlignment = 4;
        constexpr std::uint64_t frameSize = 8;
        constexpr std::uint64_t frameAlignment = 8;
        const std::uint64_t frameOffset = alignUp(stateSize, frameAlignment);
        const std::uint64_t itemOffset = alignUp(frameOffset + frameSize, itemLayout->second);
        // NS304-T06-F08: the iterator carries its MoveNext function pointer (field 3) so `it.MoveNext()`
        // over a variable can resolve the state machine without a direct call site.
        constexpr std::uint64_t moveNextFnSize = 8;
        constexpr std::uint64_t moveNextFnAlignment = 8;
        const std::uint64_t moveNextFnOffset = alignUp(itemOffset + itemLayout->first, moveNextFnAlignment);
        // NS304-T06-F07: the iterator carries its Drop function pointer (field 4) so abandoning the
        // foreach destroys the frame's live locals.
        constexpr std::uint64_t dropFnSize = 8;
        constexpr std::uint64_t dropFnAlignment = 8;
        const std::uint64_t dropFnOffset = alignUp(moveNextFnOffset + moveNextFnSize, dropFnAlignment);
        // NS304-T06-F07: Iterator<T,E> carries an error slot (field 5) so a `throw` in the generator
        // is propagated as a typed error, not a native unwind.
        const std::optional<std::string_view> errorInner = language::iteratorErrorTypeKey(key);
        std::optional<std::pair<std::uint64_t, std::uint64_t>> errorLayout;
        std::uint64_t errorOffset = 0;
        if (errorInner.has_value())
        {
            errorLayout = objectLayoutModel.valueLayoutForTypeKey(nameLookup, std::string(*errorInner));
            if (!errorLayout.has_value())
            {
                error = "NativeSharp generator iterator has no error layout: " + key;
                return false;
            }
            errorOffset = alignUp(dropFnOffset + dropFnSize, errorLayout->second);
        }
        const std::uint64_t totalAlignment =
            std::max({stateAlignment, frameAlignment, itemLayout->second, moveNextFnAlignment,
                      dropFnAlignment, errorLayout.has_value() ? errorLayout->second : 1U});
        const std::uint64_t totalSize = alignUp(
            errorLayout.has_value() ? errorOffset + errorLayout->first : dropFnOffset + dropFnSize,
            totalAlignment);
        std::vector<ObjectFieldLayout> fields;
        fields.push_back(ObjectFieldLayout{
            0, 0, stateSize, stateAlignment, "builtin:int", std::nullopt, std::nullopt});
        fields.push_back(ObjectFieldLayout{
            1, frameOffset, frameSize, frameAlignment, "builtin:void*", std::nullopt, std::nullopt});
        fields.push_back(ObjectFieldLayout{
            2, itemOffset, itemLayout->first, itemLayout->second,
            std::string(*item), std::nullopt, std::nullopt});
        fields.push_back(ObjectFieldLayout{
            3, moveNextFnOffset, moveNextFnSize, moveNextFnAlignment, "builtin:void*",
            std::nullopt, std::nullopt});
        fields.push_back(ObjectFieldLayout{
            4, dropFnOffset, dropFnSize, dropFnAlignment, "builtin:void*",
            std::nullopt, std::nullopt});
        if (errorLayout.has_value())
        {
            fields.push_back(ObjectFieldLayout{
                5, errorOffset, errorLayout->first, errorLayout->second,
                std::string(*errorInner), std::nullopt, std::nullopt});
        }
        structLayouts_.push_back(ProjectOopMirStructLayout{
            (std::numeric_limits<std::size_t>::max)(),
            key,
            totalSize,
            totalAlignment,
            false,
            0,
            0,
            false,
            abiPassingForSize(totalSize),
            false,
            true,  // requiresDrop: the iterator carries a Drop function (field 4).
            "__nativesharp_drop_iterator",
            std::move(fields)});
    }
    return true;
}
}
