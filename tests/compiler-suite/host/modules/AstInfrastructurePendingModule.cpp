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
#include <thread>
#include <utility>
#include <vector>

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define NSC_AST_THREAD_SANITIZER_ENABLED 1
#endif
#endif
#if defined(__SANITIZE_THREAD__)
#define NSC_AST_THREAD_SANITIZER_ENABLED 1
#endif

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

#if defined(NSC_AST_THREAD_SANITIZER_ENABLED)
struct ConcurrentArenaProbe final
{
    explicit ConcurrentArenaProbe(std::size_t value) : value(value) {}
    std::size_t value;
};

[[noreturn]] void runThreadSanitizerProbe()
{
    AstArena arena;
    std::atomic<unsigned> ready = 0;
    std::atomic<bool> start = false;
    const auto mutate = [&](std::size_t base) {
        ready.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::size_t index = 0; index < 4096; ++index)
            arena.make<ConcurrentArenaProbe>(base + index);
    };
    std::thread first(mutate, 0);
    std::thread second(mutate, 4096);
    while (ready.load(std::memory_order_acquire) != 2) std::this_thread::yield();
    start.store(true, std::memory_order_release);
    first.join();
    second.join();
    exitNormallyFromProbe();
}
#endif

bool dispatchProbe(std::string_view expected)
{
    if (isolated_process_support::probeMode() != expected) return false;
    if (expected == "TA-AST-016") runDestructorThrowProbe();
    if (expected == "TA-AST-215") runNullTypeProbe();
    if (expected == "TA-AST-223") runSelfCycleProbe();
    if (expected == "TA-AST-224") runTwoNodeCycleProbe();
#if defined(NSC_AST_THREAD_SANITIZER_ENABLED)
    if (expected == "TA-AST-017") runThreadSanitizerProbe();
#endif
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
    if (dispatchProbe("TA-AST-016")) return;
    const auto result = isolated_process_support::runCurrentExecutable(
        "TA-AST-016", "TA-AST-016", 5s);
    context.expect(result.started, "TA-AST-016 isolated process must start: " + result.error);
    context.expect(!result.timedOut && result.exitCode == 0,
        "TA-AST-016 throwing destruction terminated the process instead of completing cleanup");
    context.expect(result.output.find("THROWING_DESTRUCTOR") != std::string::npos,
        "TA-AST-016 child must reach the throwing destructor");
    context.expect(result.output.find("REMAINING_ENTRY_DESTROYED") != std::string::npos,
        "TA-AST-016 throwing destruction prevented cleanup of the remaining arena entry");
}

#if defined(NSC_AST_THREAD_SANITIZER_ENABLED)
void testConcurrentArenaMutation(TestContext& context)
{
    if (dispatchProbe("TA-AST-017")) return;
    const auto result = isolated_process_support::runCurrentExecutable(
        "TA-AST-017", "TA-AST-017", 15s,
        {{L"TSAN_OPTIONS", L"halt_on_error=1 exitcode=86"}});
    context.expect(result.started, "TA-AST-017 TSAN process must start: " + result.error);
    context.expect(!result.timedOut && result.exitCode == 0,
        "TA-AST-017 concurrent arena mutation triggered the ThreadSanitizer failure exit");
    context.expect(result.output.find("ThreadSanitizer") == std::string::npos &&
            result.output.find("data race") == std::string::npos,
        "TA-AST-017 concurrent arena mutation produced a ThreadSanitizer data-race report");
}
#endif

void testNullTypeArgument(TestContext& context)
{
    if (dispatchProbe("TA-AST-215")) return;
    const auto result = isolated_process_support::runCurrentExecutable(
        "TA-AST-215", "TA-AST-215", 5s);
    context.expect(result.started, "TA-AST-215 isolated process must start: " + result.error);
    context.expect(result.output.find("NULL_TYPE_PROBE_READY") != std::string::npos,
        "TA-AST-215 child must reach the null TypeSyntax probe");
    context.expect(!result.timedOut && result.exitCode == 0,
        "TA-AST-215 null TypeSyntax terminated the dumper instead of being handled safely");
}

void testCycleTraversalCompletes(TestContext& context, std::string_view caseId)
{
    if (dispatchProbe(caseId)) return;
    const auto result = isolated_process_support::runCurrentExecutable(caseId, caseId, 5s);
    const std::string marker = caseId == "TA-AST-223"
        ? "SELF_CYCLE_PROBE_READY" : "TWO_NODE_CYCLE_PROBE_READY";
    context.expect(result.started, std::string(caseId) + " watchdog process must start: " + result.error);
    context.expect(result.output.find(marker) != std::string::npos,
        std::string(caseId) + " child must reach the cyclic traversal probe");
    context.expect(!result.timedOut && result.exitCode == 0,
        std::string(caseId) + " cyclic traversal did not complete safely before the watchdog");
}

void addCase(TestRegistry& registry, std::string id, std::string contractId,
    std::string layer, std::vector<std::string> tags, TestFunction function,
    TestDisposition disposition = TestDisposition::KnownProductDefect)
{
    registry.add(TestCase{std::move(id), std::move(contractId), "Host.AST", std::move(layer),
        "NIGHTLY", std::move(tags), std::move(function), disposition});
}
}

void registerAstInfrastructurePendingModule(TestRegistry& registry)
{
    addCase(registry, "TA-AST-014", "CA-AST-002", "unit",
        {"ast", "arena", "allocation-failure", "known-product-defect"},
        testRegistrationFailure);
    addCase(registry, "TA-AST-016", "CA-AST-002", "unit",
        {"ast", "arena", "death-test", "known-product-defect"}, testThrowingDestructor);
#if defined(NSC_AST_THREAD_SANITIZER_ENABLED)
    addCase(registry, "TA-AST-017", "CA-AST-002", "unit",
        {"ast", "arena", "thread-sanitizer", "known-product-defect"},
        testConcurrentArenaMutation);
#endif
    addCase(registry, "TA-AST-215", "CA-AST-019", "component",
        {"ast", "dumper", "death-test", "known-product-defect"}, testNullTypeArgument);
    addCase(registry, "TA-AST-223", "CA-AST-020", "robustness",
        {"ast", "dumper", "watchdog", "known-product-defect"},
        [](TestContext& context) { testCycleTraversalCompletes(context, "TA-AST-223"); });
    addCase(registry, "TA-AST-224", "CA-AST-020", "robustness",
        {"ast", "dumper", "watchdog", "known-product-defect"},
        [](TestContext& context) { testCycleTraversalCompletes(context, "TA-AST-224"); });
}
}
