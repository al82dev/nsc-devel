#include "nativesharp/Driver/CrashBoundary.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LlvmProcessTrace.h"

#include <algorithm>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
int runCompiler(int argc, char** argv)
{
    const auto isTraceWorkerArgument = [](const char* argument) {
        return std::string_view(argument) ==
            nativesharp::llvm_backend::kLlvmTraceWorkerArgument;
    };
    const bool traceWorker = std::any_of(argv + 1, argv + argc, isTraceWorkerArgument);

#if defined(_WIN32)
    if (!traceWorker)
    {
        nativesharp::llvm_backend::LlvmTraceChildResult child;
        if (nativesharp::llvm_backend::runCurrentExecutableWithCapturedStandardError(
                nativesharp::llvm_backend::kLlvmTraceWorkerArgument, child))
        {
            const nativesharp::llvm_backend::LlvmProcessTraceLog trace =
                nativesharp::llvm_backend::splitLlvmProcessTrace(child.standardError);
            std::cerr << trace.diagnostics;
            if (!child.launchError.empty())
            {
                std::cerr << "nsc: warning: LLVM trace supervisor could not read its child: "
                          << child.launchError << '\n';
            }

            if (child.exitCode != 0 && !trace.passLog.empty())
            {
                nativesharp::llvm_backend::LlvmOptimizationObservation observation;
                observation.captureVerified = trace.framingObserved && trace.framingComplete;
                observation.llvmStandardErrorCaptured = trace.llvmStandardErrorCaptured;
                observation.capturedBytes = trace.capturedBytes;
                nativesharp::llvm_backend::observeLlvmPassLog(trace.passLog, observation);
                if (!observation.lastObservedPass.empty())
                {
                    std::cerr << "nsc: LLVM optimization trace: ultimo pass observado '"
                              << observation.lastObservedPass << "'";
                    if (!observation.lastObservedUnit.empty())
                    {
                        std::cerr << "; ultima funcion/unidad observada '"
                                  << observation.lastObservedUnit << "'";
                    }
                    std::cerr << "; " << observation.observedPassCount
                              << " pases observados\n";
                }
            }
            return static_cast<int>(child.exitCode);
        }

        // Observar no puede crear una forma nueva de impedir la compilacion. Si ni siquiera se
        // pudo crear el hijo, se ejecuta el driver localmente y se conserva la causa en stderr.
        std::cerr << "nsc: warning: LLVM trace supervisor unavailable: "
                  << child.launchError << '\n';
    }
    else
    {
        nativesharp::llvm_backend::setLlvmProcessTraceActive(true);
    }
#endif

    std::vector<std::string_view> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);

    for (int index = 1; index < argc; ++index)
    {
        if (isTraceWorkerArgument(argv[index])) continue;
        arguments.emplace_back(argv[index]);
    }

    const nativesharp::driver::Driver driver;
    const nativesharp::ExitCode exitCode = driver.run(arguments, std::cout, std::cerr);
    return static_cast<int>(exitCode);
}
}

int main(int argc, char** argv)
{
    return nativesharp::driver::runWithCrashBoundary(
        [&]() { return runCompiler(argc, argv); }, std::cerr);
}
