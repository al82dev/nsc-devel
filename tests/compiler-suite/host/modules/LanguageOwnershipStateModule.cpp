#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/OwnershipSemantics.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-017", "Host.Language", "unit", "PR",
        {"language", "ownership"}, std::move(function)});
}

void registerCopyWholeMoveCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-217", [](TestContext& context) {
        const std::array<std::vector<bool>, 4> fieldStates = {
            std::vector<bool>{}, {true}, {false}, {true, false, true}};
        for (const std::vector<bool>& fields : fieldStates)
        {
            MoveState state{true, true, fields};
            std::string error = "stale";
            context.expect(moveWholeValue(state, error) && error.empty() && state.isCopy &&
                    state.hasCustomDestructor && state.initializedFields == fields,
                "TL-LANG-217 a Copy whole move must be a successful no-op for every field state");
        }
    });
}

void registerNonCopyWholeMoveCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-218", [](TestContext& context) {
        for (const std::vector<bool>& fields : {std::vector<bool>{}, std::vector<bool>{false, false}})
        {
            MoveState state{false, false, fields};
            std::string error;
            context.expect(!moveWholeValue(state, error) && error == "use after move" &&
                    state.initializedFields == fields,
                "TL-LANG-218 a non-Copy value without initialized fields must fail without mutation");
        }
        for (const std::vector<bool>& fields : {std::vector<bool>{true},
                std::vector<bool>{false, true, false}, std::vector<bool>{true, true, true}})
        {
            MoveState state{false, false, fields};
            std::string error = "stale";
            context.expect(moveWholeValue(state, error) && error.empty() &&
                    state.initializedFields == std::vector<bool>(fields.size(), false),
                "TL-LANG-218 a non-Copy whole move must clear every field when any field is live");
        }
    });
}

void registerFieldMoveCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-219", [](TestContext& context) {
        for (std::size_t field = 0; field < 3; ++field)
        {
            MoveState state{false, false, {true, true, true}};
            std::string error = "stale";
            context.expect(moveField(state, field, error) && error.empty() &&
                    !state.initializedFields[field],
                "TL-LANG-219 a valid field move must clear its selected field");
            for (std::size_t sibling = 0; sibling < state.initializedFields.size(); ++sibling)
                if (sibling != field)
                    context.expect(state.initializedFields[sibling],
                        "TL-LANG-219 a valid field move must preserve every sibling field");
        }
    });
}

void registerInvalidFieldMoveCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-220", [](TestContext& context) {
        struct Sample final
        {
            MoveState state;
            std::size_t field;
            const char* error;
        };
        const std::array samples = {
            Sample{{false, false, {true, true}}, 2, "field is absent or already moved"},
            Sample{{false, false, {true, false}}, 1, "field is absent or already moved"},
            Sample{{false, true, {true, true}}, 0,
                "partial move is forbidden for a type with a custom destructor"},
        };
        for (const Sample& sample : samples)
        {
            MoveState state = sample.state;
            std::string error;
            context.expect(!moveField(state, sample.field, error) && error == sample.error &&
                    state.initializedFields == sample.state.initializedFields,
                "TL-LANG-220 every invalid field move must preserve state and report its exact error");
        }
    });
}

void registerValueClassificationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-221", [](TestContext& context) {
        context.expect(classifyValueState({false, false, {true, true}}) == ValueState::Initialized &&
                classifyValueState({false, false, {false, false}}) == ValueState::Moved &&
                classifyValueState({false, false, {true, false}}) == ValueState::PartiallyMoved &&
                classifyValueState({false, false, {}}) == ValueState::Moved,
            "TL-LANG-221 all-live, all-moved, mixed, and empty field states must classify exactly");
    });
}

void registerDropFlagCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-222", [](TestContext& context) {
        context.expect(requiredDropFlagCount({true, false, {true, true}}) == 0 &&
                requiredDropFlagCount({true, false, {true, false, true}}) == 3 &&
                requiredDropFlagCount({false, false, {true, true}}) == 0 &&
                requiredDropFlagCount({false, false, {false, false}}) == 0 &&
                requiredDropFlagCount({false, false, {true, false, true, false}}) == 4,
            "TL-LANG-222 only partial states require one drop flag per field, independent of Copy");
    });
}

void registerBorrowOverlapCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-223", [](TestContext& context) {
        const BorrowPlace root{"packet", {}};
        const BorrowPlace header{"packet", {"header"}};
        const BorrowPlace nested{"packet", {"header", "size"}};
        const BorrowPlace payload{"packet", {"payload"}};
        context.expect(!borrowsOverlap(header, {"other", {"header"}}) &&
                borrowsOverlap(root, header) && borrowsOverlap(header, root) &&
                borrowsOverlap(header, header) && borrowsOverlap(header, nested) &&
                borrowsOverlap(nested, header) && !borrowsOverlap(header, payload) &&
                !borrowsOverlap(nested, {"packet", {"header", "kind"}}),
            "TL-LANG-223 roots, equality, prefixes, and divergent projections must form the exact matrix");
    });
}

void registerExclusiveBorrowCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-224", [](TestContext& context) {
        struct Sample final { BorrowPlace left; BorrowPlace right; bool overlaps; };
        const std::array samples = {
            Sample{{"a", {}}, {"b", {}}, false},
            Sample{{"a", {}}, {"a", {"field"}}, true},
            Sample{{"a", {"left"}}, {"a", {"right"}}, false},
            Sample{{"a", {"field"}}, {"a", {"field", "nested"}}, true},
        };
        for (const Sample& sample : samples)
            context.expect(mayTakeExclusiveBorrows(sample.left, sample.right) == !sample.overlaps,
                "TL-LANG-224 exclusive borrowing must be the exact negation of known overlap cases");
    });
}
}

void registerLanguageOwnershipStateModule(TestRegistry& registry)
{
    registerCopyWholeMoveCase(registry);
    registerNonCopyWholeMoveCase(registry);
    registerFieldMoveCase(registry);
    registerInvalidFieldMoveCase(registry);
    registerValueClassificationCase(registry);
    registerDropFlagCase(registry);
    registerBorrowOverlapCase(registry);
    registerExclusiveBorrowCase(registry);
}
}
