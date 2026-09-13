#pragma once

#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
// NSC-LIMIT-005 -- LO QUE SE OBSERVA DE UNA CORRIDA DEL OPTIMIZADOR.
//
// EL DEFECTO QUE OBLIGA A ESTO. `LLVMRunPasses` es UNA SOLA LLAMADA OPACA: entra un modulo y sale
// cambiado. Cuando `c38_enum_ancho` devolvia 42 en O0 y 39 en O2 no habia forma de decir que
// pipeline se ejecuto, con que version de LLVM, ni por donde iba cuando el valor dejo de ser el
// correcto. La mitigacion --normalizar la decision a `i64`-- dejo el programa bien y la pregunta
// sin responder.
//
// LO QUE NO ES ESTA ESTRUCTURA, y conviene decirlo antes que lo que si es: NO nombra un culpable.
// El log de LLVM prueba ORDEN DE EJECUCION, no causalidad. Que un pase sea el ultimo que se vio
// antes de un fallo no lo convierte en la causa: puede haber roto algo un pase anterior y este
// haber sido el primero en tropezar con ello. Por eso los campos se llaman «ultimo observado» y el
// texto que sale al usuario tambien. Un nombre que afirme mas de lo que el dato sostiene es
// exactamente la clase de error que este proyecto persigue.
//
// LA FRONTERA. Todo esto se obtiene por la API C --`LLVMPassBuilderOptionsSetDebugLogging` y la
// salida de error del proceso--. No se incluye ninguna cabecera C++ de LLVM: atribuir un fallo a
// un pase con certeza exigiria `PassInstrumentationCallbacks`, que es C++, y eso es una decision
// de arquitectura que no se toma como efecto secundario de un criterio de cierre.
struct LlvmOptimizationObservation final
{
    // Version de la biblioteca CARGADA, que no tiene por que ser contra la que se compilo. Va la
    // primera por ese motivo: sin ella, dos corridas que difieren no se pueden ni comparar.
    unsigned llvmMajor = 0;
    unsigned llvmMinor = 0;
    unsigned llvmPatch = 0;

    // El texto EXACTO del pipeline --«default<O2>»-- y el nivel del TargetMachine. Son dos cosas
    // distintas y pueden no coincidir; el optimizador ya rechaza el caso, y el reporte los muestra
    // por separado para que se vea cual es cual.
    std::string pipeline;
    LlvmOptimizationLevel level = LlvmOptimizationLevel::O0;

    // El triple y el data layout SEGUN EL MODULO, leidos justo antes de optimizar. Entre el
    // volcado de `--dump-project-oop-llvm` y este punto corre `configureModuleForTarget`, asi que
    // el IR que ve la API C no es el de aquel volcado. Reproducir la llamada en `opt.exe` con
    // otro triple no reproduce nada.
    std::string moduleTriple;
    std::string moduleDataLayout;

    // La verificacion del modulo a los dos lados. Un modulo que entra invalido y uno que sale
    // invalido son fallos distintos y hay que poder distinguirlos en el reporte.
    bool verifiedBefore = false;
    bool verifiedAfter = false;

    // SI LA CAPTURA FUNCIONO, Y CUANTO SE CAPTURO. Sin estos dos, un `observedPassCount == 0` es
    // ambiguo entre tres cosas MUY distintas, y elegir mal entre ellas manda a arreglar lo que no
    // esta roto:
    //
    //   * el desvio del descriptor 2 no funciona en esta plataforma --defecto NUESTRO--;
    //   * el desvio funciona y LLVM no escribio nada --esta build de LLVM no emite el registro por
    //     la API C, y no hay nada que arreglar aqui--;
    //   * se capturo texto pero sin ningun «Running pass:» --el formato cambio--.
    //
    // `captureVerified` se decide escribiendo NOSOTROS una marca en la salida de error dentro de
    // la ventana de captura y comprobando si vuelve. Es la unica forma de separar «no llego nada»
    // de «no se escucho».
    bool captureVerified = false;
    std::size_t capturedBytes = 0;

    // Y SI LA SALIDA DE ERROR DE LLVM LLEGA A ESA CAPTURA, que es una pregunta distinta.
    // `captureVerified` se decide con una marca que escribimos NOSOTROS: demuestra que nuestro
    // descriptor 2 esta desviado, no que el de la biblioteca lo este. En Windows LLVM puede venir
    // como DLL con su propio CRT, y entonces lo primero es cierto y lo segundo falso. Se decide
    // haciendo que LLVM escriba --`LLVMDumpType`-- dentro de la misma ventana.
    bool llvmStandardErrorCaptured = false;

    // En Windows, con LLVM-C.dll, el registro no puede recogerse desde dentro de este proceso:
    // la DLL tiene su propio descriptor 2. Este bit dice que el optimizador lo emitio delimitado
    // para que el proceso padre lo recoja. No significa que el padre lo haya recibido; esa
    // afirmacion sólo se hace despues de encontrar ambos delimitadores y la sonda de LLVM.
    bool processTraceActive = false;

    // ULTIMO PASE OBSERVADO Y ULTIMA UNIDAD OBSERVADA. Vacios si no se pudo capturar el log --por
    // ejemplo si esta build de LLVM no lo emite--, y eso tambien se dice en el reporte en vez de
    // dejar una linea en blanco que parezca un dato.
    std::string lastObservedPass;
    std::string lastObservedUnit;
    std::size_t observedPassCount = 0;

    // Cuantas veces se repitio el par (pase, unidad) que mas se repitio. LLVM repite pases a
    // proposito --los de bucles, la iteracion del inliner sobre una SCC-- asi que un numero alto
    // no es un defecto por si mismo: es lo unico que se puede observar de una posible falta de
    // convergencia sin entrar en la API C++.
    std::size_t maxRepeatedPass = 0;
    bool convergenceSuspected = false;

    double seconds = 0.0;
    bool timeBudgetExceeded = false;

    // El texto que acompana a un fallo del optimizador. Cumple el cierre del Gate 1: nombra LLVM,
    // el pipeline, la funcion/unidad y el ultimo pase observado.
    std::string report() const;
};

// LEE EL REGISTRO DE PASES Y ANOTA LO QUE VIO. Vive fuera del optimizador para que se pueda probar
// con un registro escrito a mano: provocar una falta de convergencia real es caro y fragil, y una
// deteccion que nadie ha visto dispararse no es una deteccion.
//
// No interpreta: cuenta lo que aparece y guarda lo ultimo. El formato de LLVM es
// «Running pass: <Pase> on <Unidad>».
void observeLlvmPassLog(std::string_view log, LlvmOptimizationObservation& observation);

// Separacion de la salida de error de un hijo observado. `diagnostics` contiene exclusivamente lo
// que se escribio FUERA de las ventanas de LLVM; `passLog` contiene las ventanas sin la sonda.
// De esta manera el supervisor puede ocultar miles de lineas de instrumentacion en una corrida
// sana sin tragarse un diagnostico del compilador.
struct LlvmProcessTraceLog final
{
    bool framingObserved = false;
    bool framingComplete = false;
    bool llvmStandardErrorCaptured = false;
    std::size_t capturedBytes = 0;
    std::string passLog;
    std::string diagnostics;
};

LlvmProcessTraceLog splitLlvmProcessTrace(std::string_view standardError);

// LA MARCA QUE SE ESCRIBE PARA COMPROBAR LA PROPIA CAPTURA. No se parece a ninguna linea de LLVM,
// asi que el lector del registro la ignora sin tenerla que filtrar.
inline constexpr std::string_view kLlvmCaptureProbe = "nsc-capture-probe\n";

// LO QUE ESCRIBE LA SONDA DE LLVM: `LLVMDumpType` de un struct identificado con este nombre. Un
// `i32` era ambiguo: puede aparecer legitimamente dentro del propio registro de pases.
inline constexpr std::string_view kLlvmStandardErrorProbe =
    "nsc.llvm.stderr.probe.7f7e5db1";

// El hijo escribe estos delimitadores con SU CRT alrededor de cada llamada. LLVM escribe entre
// ellos con el CRT de LLVM-C.dll. Como el padre fijo stderr antes de que ambos se inicializaran,
// los tres textos llegan al mismo fichero sin depender de `_dup2`.
inline constexpr std::string_view kLlvmProcessTraceBegin = "nsc-llvm-pass-trace-begin";
inline constexpr std::string_view kLlvmProcessTraceEnd = "nsc-llvm-pass-trace-end";

// Cuantas repeticiones del mismo par (pase, unidad) hacen sospechar de falta de convergencia, y
// cuantos segundos se consideran excesivos. Son UMBRALES DE OBSERVACION, no limites: pasarlos no
// aborta nada, se anota en el reporte. Un limite que aborte convertiria una herramienta de
// diagnostico en una forma nueva de romper builds.
inline constexpr std::size_t kLlvmRepeatedPassSuspicion = 64;
inline constexpr double kLlvmOptimizationTimeBudgetSeconds = 120.0;

// The O2 budget is observational: exceeding it diagnoses a slow build but never aborts an opaque
// LLVMRunPasses call or changes the generated program.
inline constexpr std::uint64_t kDefaultLlvmO2BudgetMilliseconds = 120000;

class LlvmOptimizationBudgetTracker final
{
public:
    explicit LlvmOptimizationBudgetTracker(
        std::uint64_t budgetMilliseconds = kDefaultLlvmO2BudgetMilliseconds) noexcept;

    void setBudgetMilliseconds(std::uint64_t value) noexcept;
    void observe(LlvmOptimizationLevel level, std::chrono::nanoseconds elapsed) noexcept;

    std::uint64_t budgetMilliseconds() const noexcept;
    std::chrono::nanoseconds elapsed() const noexcept;
    std::size_t invocations() const noexcept;
    bool exceeded() const noexcept;
    std::string report() const;

private:
    std::uint64_t budgetMilliseconds_;
    std::chrono::nanoseconds elapsed_{};
    std::size_t invocations_ = 0;
};

// Installs a tracker only for the current compiler thread and restores the previous tracker on
// scope exit. Future worker threads must install their own scope explicitly.
class LlvmOptimizationBudgetScope final
{
public:
    explicit LlvmOptimizationBudgetScope(LlvmOptimizationBudgetTracker& tracker) noexcept;
    ~LlvmOptimizationBudgetScope();

    LlvmOptimizationBudgetScope(const LlvmOptimizationBudgetScope&) = delete;
    LlvmOptimizationBudgetScope& operator=(const LlvmOptimizationBudgetScope&) = delete;

private:
    LlvmOptimizationBudgetTracker* previous_;
};

void observeActiveLlvmOptimizationBudget(
    LlvmOptimizationLevel level, std::chrono::nanoseconds elapsed) noexcept;
}
