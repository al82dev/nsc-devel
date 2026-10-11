#include "LlvmTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace llvm_support;
using namespace nativesharp::llvm_backend;

void budgetContract(TestContext& context, std::size_t variant)
{
    LlvmOptimizationBudgetTracker tracker(10);
    tracker.observe(LlvmOptimizationLevel::O0, std::chrono::milliseconds(100));
    tracker.observe(LlvmOptimizationLevel::O2, std::chrono::nanoseconds(0));
    context.expect(tracker.invocations() == 0 && tracker.elapsed().count() == 0,
        "budget must ignore non-O2 and non-positive observations");
    tracker.observe(LlvmOptimizationLevel::O2, std::chrono::milliseconds(variant));
    tracker.observe(LlvmOptimizationLevel::O2, std::chrono::milliseconds(variant));
    context.expect(tracker.invocations() == 2, "positive O2 observations must be counted exactly");
    context.expect(tracker.elapsed() == std::chrono::milliseconds(variant * 2),
        "O2 elapsed duration must accumulate exactly");
    context.expect(tracker.exceeded() == (variant * 2 > 10), "budget threshold must use strict greater-than");
    context.expect(tracker.report().find("invocations=2") != std::string::npos,
        "budget report must expose exact invocation count");
}

void traceParserContract(TestContext& context, std::size_t variant)
{
    LlvmOptimizationObservation observation;
    std::string log = "noise\nRunning pass: InstCombinePass on Main\nRunning pass: SimplifyCFGPass on Main\n";
    if (variant == 8)
        for (std::size_t index = 0; index < 65; ++index) log += "Running pass: LoopPass on Loop\n";
    observeLlvmPassLog(log, observation);
    context.expect(observation.observedPassCount == (variant == 8 ? 67 : 2),
        "trace parser must count every observed pass line");
    context.expect(observation.lastObservedPass == (variant == 8 ? "LoopPass" : "SimplifyCFGPass"),
        "trace parser must retain the final observed pass without claiming causality");
    context.expect(observation.convergenceSuspected == (variant == 8),
        "convergence suspicion must trigger only above the repetition threshold");
    const std::string framed = std::string("diagnostic-before\n") + std::string(kLlvmProcessTraceBegin) + "\n" +
        "Running pass: SROAPass on Main\n" + std::string(kLlvmStandardErrorProbe) + "\n" +
        std::string(kLlvmProcessTraceEnd) + "\ndiagnostic-after\n";
    const LlvmProcessTraceLog split = splitLlvmProcessTrace(framed);
    context.expect(split.framingObserved && split.framingComplete && split.llvmStandardErrorCaptured,
        "trace splitter must distinguish framing and LLVM stderr probe");
    context.expect(split.passLog.find("SROAPass") != std::string::npos &&
        split.diagnostics.find("diagnostic-before") != std::string::npos,
        "trace splitter must separate pass log from compiler diagnostics");
}

void trapAbiContract(TestContext& context, std::size_t variant)
{
    const std::array<std::uint32_t, 16> expected = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const std::array<LlvmTrapReason, 16> reasons = {LlvmTrapReason::DivisionByZero,
        LlvmTrapReason::SignedDivisionOverflow, LlvmTrapReason::InvalidShiftCount,
        LlvmTrapReason::BoundsViolation, LlvmTrapReason::FloatingToIntegerOutOfRange,
        LlvmTrapReason::AllocationSizeOverflow, LlvmTrapReason::NullReference,
        LlvmTrapReason::InterfaceDispatchFailure, LlvmTrapReason::InvalidCast,
        LlvmTrapReason::AllocationFailure, LlvmTrapReason::ThrowReturned,
        LlvmTrapReason::ImportBindingFailure, LlvmTrapReason::CallbackExceptionEscape,
        LlvmTrapReason::Panic, LlvmTrapReason::AssertionFailed, LlvmTrapReason::Unreachable};
    for (std::size_t index = 0; index < reasons.size(); ++index)
        context.expect(static_cast<std::uint32_t>(reasons[index]) == expected[index],
            "trap reason numeric ABI must remain contiguous and stable");
    if (variant == 8)
        context.expect(static_cast<std::uint32_t>(LlvmTrapReason::ThrowReturned) == 11,
            "historical signed-division slot must not renumber later trap reasons");
}
}

void registerLlvmObservationLowererModule(TestRegistry& registry)
{
    addLlvmContract(registry, 17, "unit", budgetContract);
    addLlvmContract(registry, 18, "unit", traceParserContract);
    addLlvmContract(registry, 21, "unit", trapAbiContract);
}
}
