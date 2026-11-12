# Parche propuesto al integrador para el caso 73

## Estado y alcance

Esta propuesta no está aplicada. Afecta `ComptimeEngine.cpp`,
`ComptimeFunctionEvaluator.cpp`, la prueba nueva y el `CMakeLists.txt` raíz,
todos compartidos o protegidos para laptop 03. El integrador debe cotejarla con
su HEAD y resolver cambios concurrentes. No amplía funciones, efectos ni tipos.

## 1. Cancelación transaccional

Contrato recomendado: si la unidad observa cancelación, `success` es falso,
`error` es `comptime evaluation cancelled`, `source` conserva exactamente la
entrada de la unidad y no se publican evaluaciones, dependencias, atributos,
diagnósticos authored, capabilities ni expansiones parciales. Los contadores de
pasos y memoria pueden conservarse como telemetría del trabajo descartado.

En `ComptimeEngine::execute`, conservar el `string_view` de entrada antes de
que `initializeExecution` lo redirija a `result.source` y centralizar el retorno:

```cpp
const std::string_view authoredSource = source;
ComptimeResult result;
const auto cancelledResult = [&result, authoredSource]
{
    ComptimeResult cancelled;
    cancelled.source = authoredSource;
    cancelled.error = "comptime evaluation cancelled";
    cancelled.executedSteps = result.executedSteps;
    cancelled.memoryUsed = result.memoryUsed;
    return cancelled;
};
```

Usar `cancelledResult()` en los cuatro caminos posibles: cancelación devuelta
por `initializeExecution`, sondeo al inicio del bucle, fallo cancelado de
`processRunDirective` y un sondeo final antes de establecer `success=true`.
Los demás errores deben conservar el `result` actual y sus diagnósticos.

No basta insertar un sondeo antes de `commitRunEvaluation`: la cancelación
puede hacerse visible después del commit y antes de terminar el escaneo.

Regresión propuesta: ejecutar primero una fuente anidada con un callback que
cuente todos los sondeos y nunca cancele. Repetirla cancelando en cada umbral
desde 1 hasta ese total. Para cada resultado cancelado exigir:

```cpp
expect(!candidate.success && contains(candidate.error, "cancelled"),
    "every cancellation threshold reports cancellation");
expect(candidate.source == cancellationSource &&
       candidate.evaluations.empty() && candidate.dependencies.empty(),
    "cancelled evaluation publishes no partial unit result");
```

Así la prueba no depende del número de sondeo 163 observado en esta revisión.

## 2. Profundidad distinta del ancho de continuaciones

`ComptimeFunctionEvaluator.cpp` debe cambiar la condición actual:

```cpp
if (work.size() > limits.maximumNesting ||
    work.back().depth >= limits.maximumNesting)
```

por:

```cpp
if (work.back().depth >= limits.maximumNesting)
```

El máximo de 16 parámetros, 4096 bytes de fuente/expresión y 100000 pasos ya
acotan el worklist. Si se desea además una cota independiente, debe añadirse un
campo con nombre y diagnóstico propios; no debe reutilizar `maximumNesting`.

Regresión propuesta: una función superficial de 16 parámetros y una única
llamada debe producir el valor esperado con `maximumNesting = 2`. Una cadena
de llamadas cuya profundidad alcance 2 debe seguir fallando con el diagnóstico
`comptime nesting limit exceeded`.

## 3. Registro de la prueba de laptop 03

Agregar junto a `nsc_comptime_function_calls_tests`:

```cmake
add_executable(nsc_comptime_function_contract_verification_tests
    tests/unit/ComptimeFunctionContractVerificationTests.cpp)
target_link_libraries(nsc_comptime_function_contract_verification_tests
    PRIVATE nsc_frontend)
target_compile_features(nsc_comptime_function_contract_verification_tests
    PRIVATE cxx_std_20)
nsc_enable_warnings(nsc_comptime_function_contract_verification_tests)
add_test(NAME nsc.comptime-function-contract-verification
    COMMAND nsc_comptime_function_contract_verification_tests)
```

## Gates dirigidos de integración

Construir incrementalmente solo los dos objetivos de llamadas y los cuatro
objetivos históricos 385--388. Ejecutarlos por nombre, repetir
`tests/integration/comptime_calls.py` con el `nsc` aislado de la integración y
pasar `clang-tidy` sobre los dos archivos funcionales y ambas pruebas. No usar
esta propuesta para acreditar compiler-suite, interoperabilidad o batería total.
