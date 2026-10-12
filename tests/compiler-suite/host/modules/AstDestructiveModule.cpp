#include "IsolatedProcessTestSupport.h"

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstDumper.h"
#include "nativesharp/AST/TypeSyntax.h"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <new>
#include <ostream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>

#if defined(NSC_AST_DESTRUCTIVE_TESTS_ENABLED)
namespace
{
std::atomic<bool> failNextScalarAllocation = false;
}

// TA-AST-014 needs failure after T's constructor but before AstArena records ownership.  The
// replacement is inert unless the probe explicitly arms it, and the injected failure is consumed
// by exactly one scalar allocation (the arena vector's first storage allocation).
void* operator new(std::size_t size)
{
    if (failNextScalarAllocation.exchange(false, std::memory_order_acq_rel))
        throw std::bad_alloc();
    if (void* const memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}
#endif

namespace nsc::suite
{
namespace
{
using namespace std::chrono_literals;
using nativesharp::ast::AstArena;
using nativesharp::ast::AstDumper;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::ast::TypeSyntax;

#if defined(NSC_AST_DESTRUCTIVE_TESTS_ENABLED)
constexpr DWORD terminateExitCode = 0xE1;

class RegistrationFailureProbe final
{
public:
    RegistrationFailureProbe(bool& alive, std::size_t& destructionCount,
        RegistrationFailureProbe*& address)
        : alive_(alive), destructionCount_(destructionCount)
    {
        alive_ = true;
        address = this;
        failNextScalarAllocation.store(true, std::memory_order_release);
    }

    ~RegistrationFailureProbe()
    {
        alive_ = false;
        ++destructionCount_;
    }

private:
    bool& alive_;
    std::size_t& destructionCount_;
};

class ThrowingDestructorProbe final
{
public:
    ~ThrowingDestructorProbe() noexcept(false)
    {
        writeMarker("THROWING_DESTRUCTOR\n");
        throw std::runtime_error("TA-AST-016 requested destructor failure");
    }

    static void writeMarker(const char* marker) noexcept
    {
        DWORD written = 0;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), marker,
            static_cast<DWORD>(std::char_traits<char>::length(marker)), &written, nullptr);
    }
};

class RemainingDestructorProbe final
{
public:
    ~RemainingDestructorProbe()
    {
        ThrowingDestructorProbe::writeMarker("REMAINING_ENTRY_DESTROYED\n");
    }
};

class DiscardStreamBuffer final : public std::streambuf
{
protected:
    int_type overflow(int_type character) override
    {
        return traits_type::not_eof(character);
    }
};

[[noreturn]] void exitNormallyFromProbe()
{
    ExitProcess(0);
}

[[noreturn]] void runDestructorThrowProbe()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    std::set_terminate([] { ExitProcess(terminateExitCode); });
    {
        AstArena arena;
        arena.make<RemainingDestructorProbe>();
        arena.make<ThrowingDestructorProbe>();
    }
    exitNormallyFromProbe();
}

[[noreturn]] void runNullTypeProbe()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    ThrowingDestructorProbe::writeMarker("NULL_TYPE_PROBE_READY\n");
    TypeSyntax generic({}, nativesharp::lexer::TokenKind::Identifier, "Box");
    generic.addTypeArgument(nullptr);
    AstNode node(AstNodeKind::IdentifierExpression, {});
    node.setTypeSyntax(&generic);
    DiscardStreamBuffer sinkBuffer;
    std::ostream sink(&sinkBuffer);
    AstDumper{}.dump(&node, sink);
    exitNormallyFromProbe();
}

[[noreturn]] void runSelfCycleProbe()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    ThrowingDestructorProbe::writeMarker("SELF_CYCLE_PROBE_READY\n");
    AstNode node(AstNodeKind::IdentifierExpression, {});
    node.setFirst(&node);
    DiscardStreamBuffer sinkBuffer;
    std::ostream sink(&sinkBuffer);
    AstDumper{}.dump(&node, sink);
    exitNormallyFromProbe();
}

[[noreturn]] void runTwoNodeCycleProbe()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    ThrowingDestructorProbe::writeMarker("TWO_NODE_CYCLE_PROBE_READY\n");
    AstNode first(AstNodeKind::IdentifierExpression, {});
    AstNode second(AstNodeKind::IdentifierExpression, {});
    first.addChild(&second);
    second.addChild(&first);
    DiscardStreamBuffer sinkBuffer;
    std::ostream sink(&sinkBuffer);
    AstDumper{}.dump(&first, sink);
    exitNormallyFromProbe();
}
#endif

#if defined(NSC_AST_DESTRUCTIVE_TESTS_ENABLED)
bool dispatchDestructiveProbe(std::string_view expected)
{
    if (isolated_process_support::probeMode() != expected) return false;
    if (expected == "TA-AST-016") runDestructorThrowProbe();
    if (expected == "TA-AST-215") runNullTypeProbe();
    if (expected == "TA-AST-223") runSelfCycleProbe();
    if (expected == "TA-AST-224") runTwoNodeCycleProbe();
    return true;
}

void testRegistrationFailure(TestContext& context)
{
    bool alive = false;
    std::size_t destructionCount = 0;
    RegistrationFailureProbe* orphan = nullptr;
    bool allocationFailed = false;
    {
        AstArena arena;
        try
        {
            arena.make<RegistrationFailureProbe>(alive, destructionCount, orphan);
        }
        catch (const std::bad_alloc&)
        {
            allocationFailed = true;
        }
    }
    context.expect(allocationFailed,
        "TA-AST-014 registration allocation failure must propagate as bad_alloc");
    const bool leakedAllocation = orphan != nullptr && alive && destructionCount == 0;

    // Preserve process hygiene after observing the defect.  Cleanup is deliberately performed
    // after capturing the outcome so the current leak remains a failing product oracle.
    if (leakedAllocation) delete orphan;

    context.expect(!leakedAllocation,
        "TA-AST-014 failed registration leaked the constructed object outside AstArena ownership");
    context.expect(!alive && destructionCount == 1,
        "TA-AST-014 failed registration must destroy the object exactly once without a phantom entry");
}

void testThrowingDestructor(TestContext& context)
{
    if (dispatchDestructiveProbe("TA-AST-016")) return;
    const auto result = isolated_process_support::runCurrentExecutable(
        "TA-AST-016", "TA-AST-016");
    context.expect(result.started, "TA-AST-016 isolated process must start: " + result.error);
    context.expect(result.exitCode == 0,
        "TA-AST-016 throwing destruction terminated the process instead of completing cleanup");
    context.expect(result.output.find("THROWING_DESTRUCTOR") != std::string::npos,
        "TA-AST-016 child must reach the throwing destructor");
    context.expect(result.output.find("REMAINING_ENTRY_DESTROYED") != std::string::npos,
        "TA-AST-016 throwing destruction prevented cleanup of the remaining arena entry");
}
#endif

#if defined(NSC_AST_DESTRUCTIVE_TESTS_ENABLED)
void testNullTypeArgument(TestContext& context)
{
    if (dispatchDestructiveProbe("TA-AST-215")) return;
    const auto result = isolated_process_support::runCurrentExecutable(
        "TA-AST-215", "TA-AST-215");
    context.expect(result.started, "TA-AST-215 isolated process must start: " + result.error);
    context.expect(result.output.find("NULL_TYPE_PROBE_READY") != std::string::npos,
        "TA-AST-215 child must reach the null TypeSyntax probe");
    context.expect(result.exitCode == 0,
        "TA-AST-215 null TypeSyntax terminated the dumper instead of being handled safely");
}

#endif

#if defined(NSC_AST_DESTRUCTIVE_TESTS_ENABLED)
void addCase(TestRegistry& registry, std::string id, std::string contractId,
    std::string layer, std::vector<std::string> tags, TestFunction function,
    TestDisposition disposition = TestDisposition::KnownProductDefect)
{
    registry.add(TestCase{std::move(id), std::move(contractId), "Host.AST", std::move(layer),
        "NIGHTLY", std::move(tags), std::move(function), disposition});
}
#endif
}

void registerAstDestructiveModule(TestRegistry& registry)
{
#if defined(NSC_AST_DESTRUCTIVE_TESTS_ENABLED)
    addCase(registry, "TA-AST-014", "CA-AST-002", "unit",
        {"ast", "arena", "allocation-failure"},
        testRegistrationFailure, TestDisposition::Required);
    addCase(registry, "TA-AST-016", "CA-AST-002", "unit",
        {"ast", "arena", "death-test"}, testThrowingDestructor, TestDisposition::Required);
#endif
#if defined(NSC_AST_DESTRUCTIVE_TESTS_ENABLED)
    addCase(registry, "TA-AST-215", "CA-AST-019", "component",
        {"ast", "dumper", "death-test"}, testNullTypeArgument, TestDisposition::Required);
#else
    static_cast<void>(registry);
#endif
}
}
