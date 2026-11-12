# Verificación independiente del caso 73 en laptop 03

## SCOPE LOCK

Esta tanda verifica el contrato acotado ya integrado de llamadas puras desde
`#run`. No amplía funciones, tipos, resolución ni efectos admitidos. Se trabajó
en `development-fixes-limitaciones-laptop-03`, derivada del commit base
`71cd86d35220f654140e5619e9f9db0020f0b158`; el HEAD inicial fue
`5f00d5b2be3627a7a93151c41d6f7642fbd5389b`. El cambio del caso 73
`fc7ea5bb74d77116581917c47d0d6d0985197978` ya era ancestro de la base.

El ejemplo original ausente fue dispensado expresamente por el usuario. No se
reconstruyó ni se atribuyó una muestra independiente a esa fuente.

## Entorno y método

Se leyó `A:/LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md`, el lineamiento de desarrollo
del compilador y `TAREAS-LAPTOP-03.md`. Se comprobó escritura real en el repo y
`.git`. La configuración inicial vacía quedó en
`A:/nsc-fixes-laptop-03-build`, con Clang 23.1.0 y Ninja. Después se usaron solo
compilaciones incrementales y ejecuciones dirigidas por nombre desde CMD.

No se usó CTest general: su configuración conserva una ruta de compilador en
`C:/native-sharp`. Tampoco se construyeron `nsc`, interoperabilidad, SDK ni la
batería completa. Los registros están en `A:/nsc-fixes-laptop-03-evidencia`.

## Resultado demostrado

El objetivo `nsc_comptime_function_calls_tests` se construyó con código 0. La
prueba nueva y cuatro regresiones históricas se ejecutaron directamente y
pasaron 5/5:

- `nsc.comptime-function-calls`;
- `nsc.item385.comptime-core`;
- `nsc.item386.comptime-sandbox`;
- `nsc.item387.comptime-cache`;
- `nsc.item388.comptime-reproducibility`.

La prueba independiente `ComptimeFunctionContractVerificationTests.cpp` cubre
resultados, parámetros, llamada anidada, cambio de cuerpo y digest, límites
exactos de pasos y memoria contable, cancelación inicial, diagnósticos por
estado y host, y ausencia de reutilización de valores sin contabilizar. Se
compila manualmente contra las bibliotecas ya construidas porque CMake es un
archivo compartido protegido en esta tarea.

Su compilación con `/W4` y ejecución terminaron con código 0. `clang-tidy` con
`clang-analyzer-*` también terminó con código 0. Los registros son
`build-contract-verification.log`, `run-contract-verification.log` y
`tidy-contract-verification.log` en el directorio de evidencia indicado.
El parche propuesto al integrador para registrarla, junto al objetivo existente,
es crear `nsc_comptime_function_contract_verification_tests`, enlazarlo con
`nsc_frontend`, solicitar `cxx_std_20`, aplicar `nsc_enable_warnings` y añadir
el test `nsc.comptime-function-contract-verification` que ejecute ese objetivo.

El perfil observado sigue siendo deliberadamente pequeño: misma unidad y
namespace, o método estático del mismo tipo contenedor; hasta 16 parámetros
`int`; un solo `return`; enteros de 32 bits y operadores documentados. Estado,
I/O, host, FFI, async, excepciones, control de flujo, otros tipos y resolución
semántica general continúan rechazados. La reserva de memoria mide contabilidad
del evaluador, no heap ni RSS reales.

## Defectos confirmados y propuesta al integrador

Una sonda dirigida encontró dos defectos reproducibles en archivos compartidos,
que no se editaron simultáneamente:

1. Cancelación tardía no atómica. Una ejecución anidada terminó el valor 40 y
   publicó una entrada en `evaluations`; el siguiente sondeo canceló y devolvió
   `success=false`. La causa está entre `evaluateRunExpression` y
   `commitRunEvaluation` en `ComptimeEngine.cpp`: no hay sondeo antes del
   commit. Propuesta: comprobar `cancellationRequested(result)` inmediatamente
   antes de `commitRunEvaluation`, y exigir en la regresión error cancelado,
   fuente sin reemplazar y `evaluations.empty()`.

2. El ancho de la pila se confunde con profundidad. Una llamada superficial de
   16 argumentos falla con `maximumNesting` de 2 a 16 y pasa con 17, porque
   `ComptimeFunctionEvaluator.cpp` compara `work.size()` con
   `maximumNesting`. Propuesta: aplicar ese límite solo a `task.depth`, o crear
   un límite de continuaciones distinto con nombre y diagnóstico propios.

También se comprobó una frontera no documentada: en tipos anidados la llamada
sin calificar funciona, pero `Inner.Value()` y `Outer.Inner.Value()` se
rechazan. No se propone ampliar el lenguaje silenciosamente. El integrador debe
documentar que el calificador solo cubre tipos contenedores superiores, o
aprobar por separado resolución de tipos anidados con pruebas de ambigüedad.

## Objetivo independiente de compiler-suite

La inscripción está presente en `tests/compiler-suite/CMakeLists.txt`, pero el
objetivo monolítico incluye cinco módulos de interoperabilidad y fuentes bajo
`src/Interop`. Además su configuración exige `nsc`, NSLink y LLVM. Construirlo
violaría la exclusión de esta laptop; por ello no se ejecutó y sigue **NO
VERIFICADO**. La comprobación estática de inscripción no equivale a ejecución.

## Límites y entrega

No se modificaron `ComptimeEngine`, parser, modelos compartidos ni CMake. No se
hizo push, merge, rebase, deploy ni acceso de escritura a `C:/native-sharp`.
La pasada acredita solo las pruebas enumeradas; no certifica todo el compilador,
el heap real, interoperabilidad, consumo de SDK binario ni compiler-suite.
