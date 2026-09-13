# Entrega 247 — Eliminar trabajo innecesario en O0 / Remove unnecessary O0 work

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 247.
- Resultado / Result: O0 conserva su semántica y verificaciones de frontera, pero deja de pagar la
  supervisión en un segundo proceso, el registro detallado de pases y la verificación completa tras
  cada pase individual.
- Baseline: se ejecutará una sola vez al cerrar la Tanda 1.

## Problema / Problem

### Español

En Windows, `nsc` relanzaba incondicionalmente su propio ejecutable como proceso hijo antes de
interpretar la operación solicitada. El hijo activaba el registro detallado del New Pass Manager de
LLVM y el padre capturaba toda su salida estándar de error. Esto ocurría en O0, en consultas como
`--version` y en cualquier otra invocación, aunque el registro sólo es necesario para diagnosticar
qué transformación falló en pipelines optimizados.

Además, O0 solicitaba a LLVM verificar el módulo completo después de cada pase. `LlvmOptimizer` ya
verifica explícitamente el módulo antes y después del pipeline, por lo que esa política repetía
recorridos completos sin ampliar la frontera de corrección observable del compilador.

### English

On Windows, `nsc` unconditionally relaunched itself as a child process before interpreting the
requested operation. The child enabled LLVM New Pass Manager debug logging and the parent captured
all standard-error output. O0, `--version`, and every other invocation paid that cost even though
the detailed pass trace is only needed to diagnose transformations in optimized pipelines.

O0 also asked LLVM to verify the complete module after every individual pass. `LlvmOptimizer`
already verifies the module before and after the pipeline, so this repeated whole-module work did
not strengthen the compiler's observable correctness boundary.

## Implementación / Implementation

### Selección del supervisor / Supervisor selection

`llvmProcessTraceRequired()` inspecciona los argumentos normalizados antes de considerar el
supervisor de Windows. Sólo compilaciones con `-O1`, `-O2` o `-O3` entran al worker de traza. O0
implícito y explícito se ejecutan directamente, al igual que operaciones que no optimizan LLVM.

The internal worker marker is removed before the policy is evaluated. The worker alone calls
`setLlvmProcessTraceActive(true)`, preventing the normal O0 process from accidentally enabling the
verbose LLVM stream.

### Política de verificación / Verification policy

`llvmVerifyEachPassRequired()` hace explícita y comprobable la política:

- O0 verifica el módulo completo inmediatamente antes y después de `default<O0>`;
- O1, O2 y O3 conservan además la verificación de LLVM después de cada pase;
- IR inválido de O0 sigue siendo rechazado antes de emitir el objeto;
- la legalización dirigida de funclets SEH en O0 permanece intacta.

No se eliminó `default<O0>`. Ese pipeline todavía puede contener transformaciones necesarias, como
materialización de `alwaysinline`, coroutines o annotations. Esta entrega elimina instrumentación
redundante, no pases cuya ausencia pudiera cambiar el programa.

## Evidencia de rendimiento / Performance evidence

La misma compilación enfocada de `tests/language/l1_conditional` con `-O0 --time` produjo:

| Medición | `llvm-optimization` |
|---|---:|
| Antes / Before | 59,270–76,515 us |
| Después / After | 7,436 us |

La medición posterior conserva todas las líneas de fases introducidas por la entrega 246 y no
publica el registro interno de pases en una compilación exitosa.

Estas cifras son evidencia local del cambio, no un presupuesto contractual universal: dependen del
equipo, la carga y el proyecto. El contrato estable es qué trabajo se ejecuta en cada nivel.

## Archivos y funciones / Files and functions

- `include/nativesharp/LLVM/LlvmProcessTrace.h`
  - declara `llvmProcessTraceRequired()`.
- `src/LLVM/LlvmProcessTrace.cpp`
  - clasifica los niveles que requieren el supervisor y la traza completa.
- `src/nsc/main.cpp`
  - normaliza argumentos antes de decidir si relanza el compilador;
  - activa la traza únicamente dentro del worker optimizado.
- `include/nativesharp/LLVM/LlvmOptimizer.h`
  - declara la política comprobable `llvmVerifyEachPassRequired()`.
- `src/LLVM/LlvmOptimizer.cpp`
  - desactiva verify-each sólo para O0;
  - conserva verificación de entrada y salida y los pases necesarios de O0.
- `tests/unit/LlvmOptimizationTests.cpp`
  - prueba selección directa para consultas, O0 implícito y O0 explícito;
  - prueba que O1/O2/O3 mantienen supervisor y verify-each;
  - conserva las pruebas que ejecutan y verifican todos los pipelines.
- `docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md`
  - formaliza la política O0 y las garantías conservadas.
- `docs/1.0.1/compiler/NativeSharp_Compiler_1_0_1_conformance.map`
  - enlaza contrato, implementación y evidencia.
- `docs/1.0.1/NATIVESHARP_2_0_ITEMS_FINAL.md`
  - registra el avance y la regla de un baseline al final de cada tanda.

## Criterios de aceptación / Acceptance criteria

- `--version`, O0 implícito y `-O0` no crean el worker de traza LLVM.
- `-O1`, `-O2` y `-O3` conservan la captura del último pase para fallos.
- O0 no solicita verificación después de cada pase.
- O0 continúa verificando el módulo antes y después del pipeline.
- La legalización SEH necesaria en O0 permanece habilitada.
- Todos los niveles aceptan y verifican el fixture enfocado.
- Una reconstrucción limpia del compilador pasa antes del commit.
- No se ejecuta baseline dentro de esta entrega.

## Commit de referencia / Reference commit

**Título / Title**

`Elimina instrumentación redundante de O0 / Remove redundant O0 instrumentation`

**Descripción / Description**

`Run default and explicit O0 compilations directly without the Windows LLVM trace supervisor, keep
full trace capture for O1 through O3, rely on existing optimizer-boundary verification at O0 instead
of verify-each, and document and test the preserved correctness guarantees.`
