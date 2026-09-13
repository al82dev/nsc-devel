#include "nativesharp/LLVM/LlvmOptimizationObservation.h"

#include <sstream>
#include <unordered_map>

namespace nativesharp::llvm_backend
{
namespace
{
thread_local LlvmOptimizationBudgetTracker* activeBudgetTracker = nullptr;
}

LlvmOptimizationBudgetTracker::LlvmOptimizationBudgetTracker(
    std::uint64_t budgetMilliseconds) noexcept
    : budgetMilliseconds_(budgetMilliseconds)
{
}

void LlvmOptimizationBudgetTracker::setBudgetMilliseconds(std::uint64_t value) noexcept
{
    budgetMilliseconds_ = value;
}

void LlvmOptimizationBudgetTracker::observe(
    LlvmOptimizationLevel level, std::chrono::nanoseconds elapsed) noexcept
{
    if (level != LlvmOptimizationLevel::O2 || elapsed.count() <= 0) return;
    ++invocations_;
    const auto remaining = std::chrono::nanoseconds::max() - elapsed_;
    elapsed_ = elapsed > remaining ? std::chrono::nanoseconds::max() : elapsed_ + elapsed;
}

std::uint64_t LlvmOptimizationBudgetTracker::budgetMilliseconds() const noexcept
{
    return budgetMilliseconds_;
}

std::chrono::nanoseconds LlvmOptimizationBudgetTracker::elapsed() const noexcept
{
    return elapsed_;
}

std::size_t LlvmOptimizationBudgetTracker::invocations() const noexcept
{
    return invocations_;
}

bool LlvmOptimizationBudgetTracker::exceeded() const noexcept
{
    return elapsed_ > std::chrono::milliseconds(budgetMilliseconds_);
}

std::string LlvmOptimizationBudgetTracker::report() const
{
    std::ostringstream text;
    text << "optimization-budget level=O2 budget_ms=" << budgetMilliseconds_
         << " elapsed_us="
         << std::chrono::duration_cast<std::chrono::microseconds>(elapsed_).count()
         << " invocations=" << invocations_
         << " status=" << (exceeded() ? "exceeded" : "within");
    return text.str();
}

LlvmOptimizationBudgetScope::LlvmOptimizationBudgetScope(
    LlvmOptimizationBudgetTracker& tracker) noexcept
    : previous_(activeBudgetTracker)
{
    activeBudgetTracker = &tracker;
}

LlvmOptimizationBudgetScope::~LlvmOptimizationBudgetScope()
{
    activeBudgetTracker = previous_;
}

void observeActiveLlvmOptimizationBudget(
    LlvmOptimizationLevel level, std::chrono::nanoseconds elapsed) noexcept
{
    if (activeBudgetTracker != nullptr) activeBudgetTracker->observe(level, elapsed);
}

// El registro de LLVM tiene la forma «Running pass: <Pase> on <Unidad>» (a veces con un sufijo
// entre parentesis). Aqui NO se interpreta nada: se cuenta lo que se vio y se guarda lo ultimo.
void observeLlvmPassLog(std::string_view log, LlvmOptimizationObservation& observation)
{
    static constexpr std::string_view kMarker = "Running pass: ";
    std::unordered_map<std::string, std::size_t> repeats;
    std::size_t position = log.find(kMarker);
    while (position != std::string_view::npos)
    {
        const std::size_t begin = position + kMarker.size();
        const std::size_t end = log.find('\n', begin);
        std::string_view entry = log.substr(
            begin, end == std::string_view::npos ? std::string_view::npos : end - begin);
        while (!entry.empty() && (entry.back() == '\r' || entry.back() == ' ')) entry.remove_suffix(1);

        std::string_view pass = entry;
        std::string_view unit;
        const std::size_t on = entry.find(" on ");
        if (on != std::string_view::npos)
        {
            pass = entry.substr(0, on);
            unit = entry.substr(on + 4);
        }
        observation.lastObservedPass.assign(pass);
        observation.lastObservedUnit.assign(unit);
        ++observation.observedPassCount;
        const std::size_t seen = ++repeats[std::string(entry)];
        if (seen > observation.maxRepeatedPass) observation.maxRepeatedPass = seen;

        position = end == std::string_view::npos ? std::string_view::npos : log.find(kMarker, end);
    }
    observation.convergenceSuspected =
        observation.maxRepeatedPass > kLlvmRepeatedPassSuspicion;
}

LlvmProcessTraceLog splitLlvmProcessTrace(std::string_view standardError)
{
    LlvmProcessTraceLog result;
    std::size_t cursor = 0;
    for (;;)
    {
        const std::size_t begin = standardError.find(kLlvmProcessTraceBegin, cursor);
        if (begin == std::string_view::npos)
        {
            result.diagnostics.append(standardError.substr(cursor));
            break;
        }

        result.framingObserved = true;
        result.framingComplete = true;
        result.diagnostics.append(standardError.substr(cursor, begin - cursor));
        std::size_t contentBegin = begin + kLlvmProcessTraceBegin.size();
        while (contentBegin < standardError.size() &&
               (standardError[contentBegin] == '\r' || standardError[contentBegin] == '\n'))
        {
            ++contentBegin;
        }
        const std::size_t end = standardError.find(kLlvmProcessTraceEnd, contentBegin);
        const std::string_view frame = standardError.substr(
            contentBegin,
            end == std::string_view::npos ? std::string_view::npos : end - contentBegin);

        // Quitar la linea completa de la sonda. Buscar sólo el token y borrar esos bytes dejaria
        // `%"" = type opaque` dentro del registro, que no es una linea de pase pero tampoco es un
        // diagnostico real.
        std::size_t frameCursor = 0;
        for (;;)
        {
            const std::size_t probe = frame.find(kLlvmStandardErrorProbe, frameCursor);
            if (probe == std::string_view::npos)
            {
                result.passLog.append(frame.substr(frameCursor));
                break;
            }
            result.llvmStandardErrorCaptured = true;
            const std::size_t lineBeginCandidate = frame.rfind('\n', probe);
            const std::size_t lineBegin =
                lineBeginCandidate == std::string_view::npos ? 0 : lineBeginCandidate + 1;
            result.passLog.append(frame.substr(frameCursor, lineBegin - frameCursor));
            const std::size_t lineEnd = frame.find('\n', probe);
            frameCursor = lineEnd == std::string_view::npos ? frame.size() : lineEnd + 1;
        }

        if (end == std::string_view::npos)
        {
            result.framingComplete = false;
            break;
        }
        cursor = end + kLlvmProcessTraceEnd.size();
        while (cursor < standardError.size() &&
               (standardError[cursor] == '\r' || standardError[cursor] == '\n'))
        {
            ++cursor;
        }
    }

    result.capturedBytes = result.passLog.size();
    return result;
}

std::string LlvmOptimizationObservation::report() const
{
    std::ostringstream text;
    text << "LLVM " << llvmMajor << '.' << llvmMinor << '.' << llvmPatch;
    text << "; pipeline '" << pipeline << "'";
    text << "; nivel del TargetMachine " << llvmOptimizationLevelName(level);
    if (!moduleTriple.empty()) text << "; triple del modulo '" << moduleTriple << "'";
    if (!moduleDataLayout.empty()) text << "; data layout '" << moduleDataLayout << "'";
    text << "; el modulo " << (verifiedBefore ? "verificaba" : "NO verificaba") << " antes";
    text << " y " << (verifiedAfter ? "verifica" : "NO verifica") << " despues";

    // «Observado», no «culpable». El log prueba orden de ejecucion; la causalidad no se deduce de
    // una secuencia. Ver LlvmOptimizationObservation.h.
    if (!lastObservedUnit.empty())
    {
        text << "; ultima funcion/unidad observada '" << lastObservedUnit << "'";
    }
    if (!lastObservedPass.empty())
    {
        text << "; ultimo pass observado '" << lastObservedPass << "' de "
             << observedPassCount << " observados";
    }
    else
    {
        // DECIR POR QUE, no solo que no hubo. «No se observo ningun pass» a secas manda a mirar el
        // sitio equivocado la mitad de las veces.
        text << "; no se observo ningun pass: ";
        if (processTraceActive)
        {
            text << "el registro se emitio al supervisor del proceso; este reporte del hijo no "
                    "puede afirmar todavia que el padre lo recibio";
        }
        else if (!captureVerified)
        {
            text << "NO SE PUDO DESVIAR la salida de error de este proceso, asi que no se habria "
                    "capturado nada aunque LLVM hubiera escrito";
        }
        else if (!llvmStandardErrorCaptured)
        {
            // La distincion que costo una corrida entera averiguar: nuestro descriptor desviado no
            // implica el de LLVM. Si la sonda de la biblioteca no vuelve, el registro podria estar
            // emitiendose y perdiendose por otro sitio.
            text << "el desvio funciona para ESTE proceso pero la salida de error de LLVM NO PASA "
                    "por el --biblioteca con su propio CRT--, asi que no se puede afirmar que LLVM "
                    "no escribiera";
        }
        else if (capturedBytes == 0)
        {
            text << "la captura funciona Y la salida de error de LLVM pasa por ella --las dos "
                    "sondas volvieron-- y aun asi LLVM NO ESCRIBIO NADA: esta build no emite el "
                    "registro de pases por la API C";
        }
        else
        {
            text << "la captura funciona y se recogieron " << capturedBytes
                 << " bytes, pero ninguno es una linea «Running pass:»";
        }
    }
    if (convergenceSuspected)
    {
        text << "; SE SOSPECHA FALTA DE CONVERGENCIA: un mismo par (pass, unidad) se repitio "
             << maxRepeatedPass << " veces";
    }
    if (timeBudgetExceeded)
    {
        text << "; TIEMPO EXCESIVO: " << seconds << " s";
    }
    return text.str();
}
}
