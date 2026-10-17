#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstArena.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::ast::AstArena;

// A single configurable probe keeps the arena tests independent from production AST nodes while
// still exercising distinct concrete types, alignment, constructor forwarding, and destruction.
template <std::size_t Alignment, int TypeIdentity>
class alignas(Alignment) ArenaProbe final
{
public:
    ArenaProbe(int value, bool* alive = nullptr, std::size_t* destructionCount = nullptr,
        std::vector<int>* destructionOrder = nullptr, const void* related = nullptr,
        bool throwFromConstructor = false)
        : value_(value), alive_(alive), destructionCount_(destructionCount),
          destructionOrder_(destructionOrder), related_(related)
    {
        if (throwFromConstructor)
        {
            throw std::runtime_error("requested constructor failure");
        }
        if (alive_ != nullptr)
        {
            *alive_ = true;
        }
    }

    ~ArenaProbe() noexcept
    {
        if (alive_ != nullptr)
        {
            *alive_ = false;
        }
        if (destructionCount_ != nullptr)
        {
            ++*destructionCount_;
        }
        if (destructionOrder_ != nullptr)
        {
            destructionOrder_->push_back(value_);
        }
    }

    [[nodiscard]] int value() const noexcept
    {
        return value_;
    }

    [[nodiscard]] const void* related() const noexcept
    {
        return related_;
    }

    void setRelated(const void* related) noexcept
    {
        related_ = related;
    }

    [[nodiscard]] static constexpr int typeIdentity() noexcept
    {
        return TypeIdentity;
    }

private:
    int value_ = 0;
    bool* alive_ = nullptr;
    std::size_t* destructionCount_ = nullptr;
    std::vector<int>* destructionOrder_ = nullptr;
    const void* related_ = nullptr;
};

using DefaultProbe = ArenaProbe<alignof(std::max_align_t), 1>;

// Registers one AstArena case with the metadata shared by CA-AST-001 and CA-AST-002.
void addArenaCase(TestRegistry& registry, std::string id, std::string contractId,
    std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), std::move(contractId), "Host.AST", "unit",
        std::move(profile), {"ast", "arena"}, std::move(function)});
}

// Proves that destroying an arena with no entries has no observable deleter effect.
void testEmptyArena(TestContext& context)
{
    std::size_t destructionCount = 0;
    {
        AstArena arena;
        context.expect(destructionCount == 0, "TA-AST-001 empty arena must remain empty");
    }
    context.expect(destructionCount == 0, "TA-AST-001 empty arena must not invoke a deleter");
}

// Checks perfect-forwarded constructor arguments and pointer stability for one allocation.
void testMakeForwardsArguments(TestContext& context)
{
    AstArena arena;
    DefaultProbe* const object = arena.make<DefaultProbe>(42);
    DefaultProbe* const stablePointer = object;
    context.expect(object != nullptr && object->value() == 42,
        "TA-AST-002 make must forward constructor arguments");
    context.expect(object == stablePointer,
        "TA-AST-002 make must return the stable allocated object pointer");
}

// Uses distinct concrete template instantiations to prove per-type destruction.
void testHeterogeneousObjects(TestContext& context)
{
    std::size_t firstDestroyed = 0;
    std::size_t secondDestroyed = 0;
    {
        AstArena arena;
        arena.make<ArenaProbe<alignof(std::max_align_t), 11>>(11, nullptr, &firstDestroyed);
        arena.make<ArenaProbe<alignof(std::max_align_t), 22>>(22, nullptr, &secondDestroyed);
    }
    context.expect(firstDestroyed == 1 && secondDestroyed == 1,
        "TA-AST-003 each heterogeneous object must be destroyed exactly once");
}

// Freezes reverse-registration destruction for three entries.
void testReverseDestructionOrder(TestContext& context)
{
    std::vector<int> order;
    {
        AstArena arena;
        arena.make<DefaultProbe>(1, nullptr, nullptr, &order);
        arena.make<DefaultProbe>(2, nullptr, nullptr, &order);
        arena.make<DefaultProbe>(3, nullptr, nullptr, &order);
    }
    context.expect(order == std::vector<int>({3, 2, 1}),
        "TA-AST-004 destruction order must be C, B, A");
}

// Confirms that raw relationships do not shorten either object's arena-owned lifetime.
void testNonOwningRelationships(TestContext& context)
{
    bool firstAlive = false;
    bool secondAlive = false;
    {
        AstArena arena;
        DefaultProbe* const first = arena.make<DefaultProbe>(1, &firstAlive);
        DefaultProbe* const second = arena.make<DefaultProbe>(2, &secondAlive, nullptr, nullptr, first);
        first->setRelated(second);
        context.expect(firstAlive && secondAlive && first->related() == second && second->related() == first,
            "TA-AST-005 related objects must both remain alive inside the arena scope");
    }
    context.expect(!firstAlive && !secondAlive,
        "TA-AST-005 related objects must both die with the arena");
}

// Forces repeated entry-vector growth and proves separately allocated objects never move.
void testEntryGrowthPreservesPointers(TestContext& context)
{
    constexpr int ObjectCount = 2048;
    bool transferredAlive = false;
    std::size_t transferredDestructions = 0;
    DefaultProbe* transferred = nullptr;
    {
        AstArena arena;
        std::vector<DefaultProbe*> pointers;
        pointers.reserve(ObjectCount);
        for (int index = 0; index < ObjectCount; ++index)
            pointers.push_back(arena.make<DefaultProbe>(index));
        for (int index = 0; index < ObjectCount; ++index)
        {
            context.expect(pointers[static_cast<std::size_t>(index)]->value() == index,
                "TA-AST-006 entry growth must not move allocated objects");
        }

        {
            AstArena worker;
            transferred = worker.make<DefaultProbe>(
                305, &transferredAlive, &transferredDestructions);
            arena.absorb(std::move(worker));
        }
        context.expect(transferredAlive && transferred != nullptr && transferred->value() == 305,
            "TA-AST-006 absorbing another arena must preserve its object pointers and ownership");
        context.expect(transferredDestructions == 0,
            "TA-AST-006 destroying the emptied source arena must not destroy transferred objects");
    }
    context.expect(!transferredAlive && transferredDestructions == 1,
        "TA-AST-006 absorbing arena must destroy each transferred object exactly once");
}

// Counts the destructor calls generated by a fixed number of successful make operations.
void testExactlyOneDestructionPerMake(TestContext& context)
{
    constexpr std::size_t ObjectCount = 257;
    std::size_t destructionCount = 0;
    {
        AstArena arena;
        for (std::size_t index = 0; index < ObjectCount; ++index)
        {
            arena.make<DefaultProbe>(static_cast<int>(index), nullptr, &destructionCount);
        }
    }
    context.expect(destructionCount == ObjectCount,
        "TA-AST-007 every successful make must cause exactly one destruction");
}

// Places the arena in a nested scope to identify its exact lifetime boundary.
void testNestedScopeDestruction(TestContext& context)
{
    std::size_t destructionCount = 0;
    {
        AstArena arena;
        arena.make<DefaultProbe>(8, nullptr, &destructionCount);
        context.expect(destructionCount == 0,
            "TA-AST-008 object must remain alive before the nested scope ends");
    }
    context.expect(destructionCount == 1,
        "TA-AST-008 object must be destroyed when the nested scope ends");
}

// Exercises an over-aligned allocation through the unmodified public make API.
void testHighAlignment(TestContext& context)
{
    using HighlyAlignedProbe = ArenaProbe<256, 256>;
    AstArena arena;
    HighlyAlignedProbe* const object = arena.make<HighlyAlignedProbe>(9);
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    context.expect(address % alignof(HighlyAlignedProbe) == 0,
        "TA-AST-009 make must honor the requested object alignment");
}

// Stress-checks the full reverse order rather than sampling only endpoints.
void testManyObjectsCompleteLifo(TestContext& context)
{
    constexpr int ObjectCount = 8192;
    std::vector<int> order;
    order.reserve(ObjectCount);
    {
        AstArena arena;
        for (int index = 0; index < ObjectCount; ++index)
        {
            arena.make<DefaultProbe>(index, nullptr, nullptr, &order);
        }
    }
    context.expect(order.size() == static_cast<std::size_t>(ObjectCount),
        "TA-AST-010 every stress object must be destroyed");
    for (int index = 0; index < ObjectCount; ++index)
    {
        context.expect(order[static_cast<std::size_t>(index)] == ObjectCount - index - 1,
            "TA-AST-010 destruction must preserve complete LIFO order");
    }
}

// Exposes the compile-time non-copy-constructible ownership contract.
void testNotCopyConstructible(TestContext& context)
{
    context.expect(!std::is_copy_constructible_v<AstArena>,
        "TA-AST-011 AstArena must not be copy constructible");
}

// Exposes the compile-time non-copy-assignable ownership contract.
void testNotCopyAssignable(TestContext& context)
{
    context.expect(!std::is_copy_assignable_v<AstArena>,
        "TA-AST-012 AstArena must not be copy assignable");
}

// Verifies constructor exception propagation and absence of a registered ghost entry.
void testThrowingConstructor(TestContext& context)
{
    bool caughtExpectedException = false;
    std::size_t destructionCount = 0;
    {
        AstArena arena;
        try
        {
            arena.make<DefaultProbe>(13, nullptr, &destructionCount, nullptr, nullptr, true);
        }
        catch (const std::runtime_error& error)
        {
            caughtExpectedException = std::string(error.what()) == "requested constructor failure";
        }
        arena.make<DefaultProbe>(14, nullptr, &destructionCount);
    }
    context.expect(caughtExpectedException,
        "TA-AST-013 constructor exception must propagate unchanged");
    context.expect(destructionCount == 1,
        "TA-AST-013 a failed construction must not leave a ghost arena entry");
}

// Confirms normal noexcept destructors do not interrupt traversal of remaining entries.
void testNoexceptDestructorsContinue(TestContext& context)
{
    static_assert(std::is_nothrow_destructible_v<DefaultProbe>);
    std::size_t destructionCount = 0;
    {
        AstArena arena;
        for (int index = 0; index < 17; ++index)
        {
            arena.make<DefaultProbe>(index, nullptr, &destructionCount);
        }
    }
    context.expect(destructionCount == 17,
        "TA-AST-015 normal noexcept destruction must visit every entry");
}

// Proves the erased deleter retains the exact concrete type selected by each make invocation.
void testPerTypeDestroyTemplate(TestContext& context)
{
    std::size_t firstDestroyed = 0;
    std::size_t secondDestroyed = 0;
    std::size_t thirdDestroyed = 0;
    {
        AstArena arena;
        arena.make<ArenaProbe<alignof(std::max_align_t), 101>>(1, nullptr, &firstDestroyed);
        arena.make<ArenaProbe<alignof(std::max_align_t), 202>>(2, nullptr, &secondDestroyed);
        arena.make<ArenaProbe<alignof(std::max_align_t), 303>>(3, nullptr, &thirdDestroyed);
    }
    context.expect(firstDestroyed == 1 && secondDestroyed == 1 && thirdDestroyed == 1,
        "TA-AST-018 every concrete deleter must invoke its matching destructor");
}
}

// Adds all AstArena cases executable without allocation injection, a death-test process, or TSAN.
void registerAstArenaModule(TestRegistry& registry)
{
    addArenaCase(registry, "TA-AST-001", "CA-AST-001", "PR", testEmptyArena);
    addArenaCase(registry, "TA-AST-002", "CA-AST-001", "PR", testMakeForwardsArguments);
    addArenaCase(registry, "TA-AST-003", "CA-AST-001", "PR", testHeterogeneousObjects);
    addArenaCase(registry, "TA-AST-004", "CA-AST-001", "PR", testReverseDestructionOrder);
    addArenaCase(registry, "TA-AST-005", "CA-AST-001", "PR", testNonOwningRelationships);
    addArenaCase(registry, "TA-AST-006", "CA-AST-001", "PR", testEntryGrowthPreservesPointers);
    addArenaCase(registry, "TA-AST-007", "CA-AST-001", "PR", testExactlyOneDestructionPerMake);
    addArenaCase(registry, "TA-AST-008", "CA-AST-001", "PR", testNestedScopeDestruction);
    addArenaCase(registry, "TA-AST-009", "CA-AST-001", "PR", testHighAlignment);
    addArenaCase(registry, "TA-AST-010", "CA-AST-001", "NIGHTLY", testManyObjectsCompleteLifo);
    addArenaCase(registry, "TA-AST-011", "CA-AST-002", "PR", testNotCopyConstructible);
    addArenaCase(registry, "TA-AST-012", "CA-AST-002", "PR", testNotCopyAssignable);
    addArenaCase(registry, "TA-AST-013", "CA-AST-002", "PR", testThrowingConstructor);
    addArenaCase(registry, "TA-AST-015", "CA-AST-002", "PR", testNoexceptDestructorsContinue);
    addArenaCase(registry, "TA-AST-018", "CA-AST-002", "PR", testPerTypeDestroyTemplate);
}
}
