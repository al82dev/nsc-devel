# 307 — Fences de compilador y CPU / Compiler and CPU fences

## Resumen / Summary

Esta entrega publica dos barreras explícitas en `NativeSharp.Core.Threading.AtomicFence`: una de
alcance entre hilos y otra exclusiva del compilador. Ambas se emiten como instrucciones LLVM, sin
llamadas de runtime ni dependencias del sistema operativo.

This delivery publishes two explicit barriers in `NativeSharp.Core.Threading.AtomicFence`: a
cross-thread fence and a compiler-only fence. Both lower directly to LLVM instructions with no
runtime call or operating-system dependency.

## API pública / Public API

```csharp
AtomicFence.Thread(MemoryOrder order);
AtomicFence.Compiler(MemoryOrder order);
```

`Thread` ordena ante compilador y hardware con alcance de sistema. `Compiler` usa el alcance LLVM
`singlethread`, de modo que restringe el movimiento del optimizador pero no presenta una barrera
de comunicación entre otros hilos.

Thread orders both compiler and hardware at system scope. Compiler uses LLVM singlethread scope,
constraining optimizer motion without claiming cross-thread hardware synchronization.

## Contrato de órdenes / Ordering contract

- Admitidos: Acquire, Release, AcquireRelease y SequentiallyConsistent.
- Rechazado: Relaxed, porque no expresa barrera y LLVM no lo admite en `fence`.
- Rechazado: cualquier valor fuera del ABI MemoryOrder 0..4.
- Rechazado: una variable de runtime; el orden debe ser constante de compilación.

Los fences no vuelven atómicos los accesos ordinarios y no corrigen por sí solos una data race.
El programa debe componerlos con operaciones atómicas que publican o consumen datos.

## Diseño e implementación / Design and implementation

### Reconocimiento semántico

`ProjectMethodBodyModel.cpp` reconoce clase, nombre, firma estática, retorno void e identidad
nominal de `MemoryOrder`. El evaluador constante consume el argumento y guarda sólo el valor ABI
validado en `MethodCoreBinding::atomicMemoryOrder`.

### HIR y MIR

Se agregaron nodos distintos `CoreAtomicThreadFence` y `CoreAtomicCompilerFence`. HIR comprueba
que existe exactamente un argumento sintáctico ya plegado, pero no crea un hijo ejecutable para
él. MIR conserva el orden en `integerValue` y diferencia explícitamente ambos alcances.

Los mapas exhaustivos de core operations, HIR y MIR se actualizaron. La primera compilación
enfocada confirmó que el guardián sin `default` detecta correctamente cualquier enumerador nuevo
no clasificado.

### LLVM

`LlvmCApi.h` declara `LLVMBuildFence`, ya exportado por la API C de LLVM enlazada. El backend
traduce el enum NativeSharp con el mismo mapa explícito de atomics y emite:

- `fence acquire/release/acq_rel/seq_cst` para `Thread`;
- `fence syncscope("singlethread") ...` para `Compiler`.

La instrucción se crea con nombre vacío. El primer intento le asignó un nombre SSA y el
verificador LLVM lo rechazó porque `fence` devuelve void; retirar ese nombre es parte de la
corrección final y está cubierto por la verificación del módulo.

## Archivos modificados / Modified files

- `lib/NativeSharp.Core.Threading/Atomics.ns`: clase pública AtomicFence.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: identidades core.
- `include/nativesharp/Frontend/ProjectOopHirModel.h`: nodos HIR.
- `include/nativesharp/Frontend/ProjectOopMirModel.h`: nodos MIR.
- `src/Frontend/ProjectMethodBodyModel.cpp`: reconocimiento y validación del orden.
- `src/Frontend/ProjectOopHirModel.cpp`: lowering dedicado y mapa exhaustivo.
- `src/Frontend/ProjectOopMirModel.cpp`: nombres y conversión HIR a MIR.
- `src/LLVM/LlvmCApi.h`: declaración C de LLVMBuildFence.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: emisión global/singlethread.
- `tests/unit/Stage54_2AtomicsTests.cpp`: IR positivo y diagnósticos negativos.
- `examples/project_stage54_2/Main.ns`: uso de ambas barreras con cuatro órdenes.
- documentación 1.0.1: NS-MEM-008, conformidad, estado, changelog y roadmap.

## Evidencia enfocada / Focused evidence

```text
cmake --build A:\nsc-devel\build-tests --target nsc_stage54_2_atomics_tests -j 8
ctest --test-dir A:\nsc-devel\build-tests -R "^nsc.stage54-2.atomics$" --output-on-failure
1/1 passed
```

La prueba verifica módulos LLVM, exige ambos alcances y los cuatro órdenes útiles, comprueba
diagnósticos de Relaxed/fuera de ABI/runtime y compila/ejecuta el ejemplo histórico en O0/O2.
No se ejecutó baseline.

The test verifies LLVM modules, requires both scopes and all four meaningful orders, checks
Relaxed/out-of-ABI/runtime diagnostics, and builds/runs the established example at O0/O2. No
baseline was run.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Añade fences de CPU y compilador / Add CPU and compiler fences`

**Descripción / Description**

`Publica AtomicFence.Thread/Compiler, valida órdenes constantes y emite fences LLVM globales o singlethread sin runtime. / Publish AtomicFence.Thread/Compiler, validate compile-time orders, and emit system-scope or singlethread LLVM fences without runtime support.`
