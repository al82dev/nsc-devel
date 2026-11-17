# Tanda 19: nivel de CPU por proyecto y vectores de 256 bits (T19-036)

## Defectos y faltantes

1. **Sin nivel de CPU.** NativeSharp compilaba siempre con el destino x86-64 base. `TargetInfo` tenía
   `x86-64-v2/v3/v4`, pero nada lo elegía. Sin AVX, los vectores de 256 y 512 bits no tienen una convención
   coherente; ni Clang contra Clang (registro `t19-vectorcall-avx-verificacion.md`).
2. **Vectores de 256 bits rechazados.** Sueltos (`__m256`, `__m256d`, `__m256i`) o en agregados `__vectorcall`:
   - el plan de Clang no los representaba;
   - el backend solo aceptaba miembros de 128 bits.
3. **`double` x2 suelto (`__m128d`).** No tenía portador: NativeSharp.Core no tenía `Float64x2`.
4. **Vectores enteros con `__vectorcall`.** Se rechazaban con `integer vectors are not mapped for __vectorcall`.
5. **Defecto de `1a130ffb9`.** Un encabezado C++ con vectores solo en funciones `cdecl` no tenía plan de Clang.
   El generador de código de la sesión solo se creaba si alguna función usaba `regcall`, `sysv_abi`,
   `preserve_none`, `vectorcall` o una convención planificada. Los casos de `1a130ffb9` siempre incluían una
   función `__vectorcall`, así que no lo veían. Daba `it has no Clang ABI plan`.

## Cambio

- **Nivel de CPU de la compilación.**
  - Propiedad del proyecto raíz `CpuLevel` (`x86-64`, `x86-64-v2`, `x86-64-v3`, `x86-64-v4`) y opción
    `--cpu-level=<nivel>`, que tiene prioridad (`ProjectLoader`, `CompilerInvocation`, `CompilerOptionParser`).
  - `CompilerSession` fija el nivel para toda la compilación con `target::CpuFeatureLevelScope`.
  - `LlvmTargetMachine::createWindowsX64` usa el nivel elegido, así que todas las máquinas destino de la
    compilación lo comparten.
  - Las sesiones de Clang reciben `-march` del mismo nivel (`CppSessionOptions::targetCpu`); el nivel entra en la
    identidad de configuración de la sesión solo cuando no es el base.
  - La clave de caché de cada unidad de código ya incluía el CPU y las características de la máquina destino.
  - Un valor desconocido se rechaza con su diagnóstico.
- **Plan de Clang** (`ClangAbiPlan.cpp`). Con la convención C de Win64 o `__vectorcall` se representa un vector
  suelto de 128 bits, de 256 si el destino tiene AVX y de 512 si tiene AVX-512F. Las demás convenciones siguen
  fuera, como comprueba `nsc.cpp-interop.clang-abi-probe`.
- **Portadores** (`cppVectorValueType`, backend):
  - `Float32x4`, `Float64x2` (nuevo en NativeSharp.Core), `Int32x4`, `Int64x2`;
  - `Float32x8`, `Float64x4`, `Int32x8`, `Int64x4`.

  `simdCarrierShape`, `simdCarrierVectorType` y `planVectorMatchesCarrier` comparan los carriles con el plan.
  Con `__vectorcall`, cualquiera de ellos viaja como el vector de sus carriles (XMM o YMM), también los enteros.
- **Agregados `__vectorcall`** de vectores de 128, 256 o 512 bits: todos los miembros tienen el mismo ancho
  (cambio preparado en `av1`, aplicado ahora).
- **Comprobación del nivel** (`declareFunction`). Una importación cuyo plan pasa en registros un vector más ancho
  que el nivel de la compilación se rechaza con
  `passes 256-bit vectors in registers: set the project CpuLevel to x86-64-v3`. Esto cubre un plan precompilado de
  un productor con un nivel mayor que el consumidor. Sin plan, el frontend da
  `a 256-bit vector needs AVX registers: set the project CpuLevel to x86-64-v3`.
- `ClangSession.cpp`: una función C++ con un vector por valor también crea el generador de código del plan.

## Evidencia

`pruebas-tanda-19-traslados`. Cada caso compara con un control de Clang 23.1.0 compilado con el mismo `-march`, en
estática y DLL, O0–O3, con la forma de cada declaración del IR de NSC igual a la de Clang. Los casos con plantilla
precompilada se prueban con y sin encabezado.

| Caso | Contenido | Resultado |
|---|---|---|
| `vector-solo-cdecl` | vectores solo en funciones `cdecl` (defecto 5) | 8642, igual que Clang; precompilado |
| `vector-suelto-double` | `double` x2 `cdecl` y `__vectorcall`; `int` x4 y `long long` x2 con `__vectorcall` | 34125; precompilado |
| `vector-suelto-avx` (`x86-64-v3`) | `float` x8, `double` x4, `int` x8, `long long` x4 `cdecl`; `float` x8 y `double` x4 `__vectorcall` | 5307; precompilado; sin el nivel se rechaza, directo y desde la plantilla precompilada sin encabezado |
| `vectorcall-avx` (`x86-64-v3`) | agregados `__vectorcall` de vectores de 256 bits (`W8 {f8 a, b}`, `D4 {d4 v[3]}`), incluido uno que ya no cabe en registros | 988002, el valor del código fuente (antes, sin AVX, Clang daba valores incoherentes); precompilado; rechazos sin el nivel |
| `vectorcall-avx512` (`x86-64-v4`) | los mismos más `Z1 {i16 z}` de 512 bits | compila y enlaza; 7 declaraciones con la forma de Clang; no se ejecuta: la CPU del equipo (Comet Lake) no tiene AVX-512 |
| `validar-vectores-c.py` (C) | sumado `double` x2; con `--avx`, 256 bits `cdecl` y `__vectorcall` | 130828 y 132997, iguales a Clang, estática y DLL, precompilado |
| `validar-nivel-cpu.py` | `--cpu-level` sin `CpuLevel`, prioridad sobre `CpuLevel`, valores inválidos en proyecto y opción | correcto |

Registros: `logs/fx-cpu1*.log`, `logs/fx-cpu2*.log`, `logs/vc2*.log`, `logs/ncpu1.log`.

## Regresión

Validación agrupada `validar-vs7` (registros en `logs/`):

- Fixture de traslados de la tanda 19 completa: estática (`fx-vs7t.log`) y DLL (`fx-vs7td.log`), todos los casos.
- Vectores C de 128 bits (130828) y de 128 y 256 bits con `--avx` (132997), estática y DLL (`vc2*.log`); nivel de
  CPU (`ncpu1.log`).
- T08: vectorcall, callbacks e identidad de funciones C (`t08-*-vc8.log`).
- CTest dirigido (`lote-ct2.log`: clang-abi-probe, regcall-record-plan, variadic-pointer, project-loader,
  item331–345, identity): todos pasan. `ctest-vs7.log` (57 pruebas): fallan `nsc.cpp-interop.session` y
  `nsc.stage42.delegates-events`, las dos ya fallaban antes de este cambio (línea base y validaciones anteriores).
- Tandas 9 a 18, estática (`regresion-vs7.log`): 1154 pasan; fallan los dos validadores
  `cpp-variadica-preserve-none` (bloqueo de Clang, T19-032) y `cpp-tamano-objeto/validar-indice-expresion` e
  `validar-indice-llamada` con cinco procesos (T19-026), repetidos solos en `regresion-t09-to-vs7.log`.
- Tandas 9 a 18, DLL (`regresion-dll-vs7d.log`): 61 de 61 pasan.

## Sigue pendiente (T19-036)

- **Ejecución de 512 bits.** Hay que ejecutar `vectorcall-avx512` en una CPU con AVX-512. En este equipo solo se
  verifica que compila, enlaza y tiene la forma de Clang.
- **Vectores sueltos de 512 bits.** NativeSharp.Core no tiene portadores de 512 bits (`Float32x16`…). Hoy se
  rechazan con `only 128-bit and 256-bit vectors are mapped`.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único: el plan precompilado ya viajaba.
