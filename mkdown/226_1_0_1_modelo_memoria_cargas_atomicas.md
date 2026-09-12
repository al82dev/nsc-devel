# 226 — Modelo de memoria: carga y publicación atómicas / Memory model: atomic load and publication

**Línea / Line:** `1.0.1-dev`

**Contrato / Contract:** `NS-MEM-002`

## Resumen

`AtomicInt32` y `AtomicInt64` ya podían hacer operaciones read-modify-write `seq_cst`, pero no
tenían una carga ni una escritura atómica directas. La propiedad `Value` era deliberadamente una
lectura ordinaria y la única forma de leer con sincronización era ejecutar un
`CompareExchange`. La entrega agrega:

```csharp
int value = counter.Load();
counter.Store(next);
```

y las variantes equivalentes de 64 bits. Ambas son operaciones atómicas reales con orden
secuencialmente consistente. No se agregan órdenes seleccionables ni fences: 1.0.1 conserva una
regla única, segura por defecto, y evita exponer combinaciones inválidas de acquire/release sin un
consumidor que las justifique.

## Decisiones de diseño

### `Value` no cambia silenciosamente de significado

La propiedad `Value` continúa siendo una observación ordinaria útil para diagnóstico. Convertirla
en acquire habría cambiado el costo y la semántica de código existente sin indicación en la fuente.
`Load()` hace visible que el programa participa en un protocolo entre hilos.

### Orden único en 1.0.1

`Load`, `Store`, `Increment`, `Decrement`, `Add`, `Exchange` y `CompareExchange` usan `seq_cst`.
Esto ofrece atomicidad, acquire/release y un orden total. Una futura API de órdenes debe validar las
combinaciones por clase de operación; no se acepta un entero abierto que LLVM pueda interpretar de
manera distinta.

### Ubicación implícita

Como las operaciones existentes, `Load` y `Store` son métodos de instancia. El frontend resuelve el
campo privado `value_` y OOP HIR sintetiza su `FieldAddress`. El usuario no necesita obtener una
dirección cruda y la implementación conserva un único mecanismo de layout.

### Runtime nativo

`WindowsRuntimeBuilder` emite cuatro funciones nuevas:

- `__nativesharp_atomic_load_int32`;
- `__nativesharp_atomic_store_int32`;
- `__nativesharp_atomic_load_int64`;
- `__nativesharp_atomic_store_int64`.

Sus cuerpos contienen `load atomic`/`store atomic` con `seq_cst` y alineación natural. No llaman a
Win32 ni a una biblioteca administrada.

## Flujo implementado

1. `ProjectMethodBodyModel` reconoce clase, nombre y firma exactos.
2. `MethodCoreOperationKind` conserva ancho y operación.
3. OOP HIR sintetiza la dirección del campo y agrega el valor sólo para `Store`.
4. OOP MIR preserva cuatro nodos distintos.
5. `ProjectOopLlvmLowerer` llama al hook exacto del runtime.
6. `WindowsRuntimeBuilder` emite la operación LLVM atómica real.

## Archivos modificados

| Archivo | Implementación |
|---|---|
| `lib/NativeSharp.Core.Threading/Atomics.ns` | API pública `Load`/`Store` de 32 y 64 bits |
| `include/nativesharp/Frontend/ProjectMethodBodyModel.h` | Cuatro operaciones núcleo semánticas |
| `include/nativesharp/Frontend/ProjectOopHirModel.h` | Cuatro nodos OOP HIR |
| `include/nativesharp/Frontend/ProjectOopMirModel.h` | Cuatro nodos OOP MIR |
| `src/Frontend/ProjectMethodBodyModel.cpp` | Reconocimiento por firma e identidad del campo |
| `src/Frontend/ProjectOopHirModel.cpp` | Dirección implícita, aridad y mapeo HIR |
| `src/Frontend/ProjectOopMirModel.cpp` | Preservación HIR→MIR y nombres estables de dump |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | Declaraciones y llamadas de los cuatro hooks |
| `src/Runtime/WindowsRuntimeBuilder.cpp` | `load atomic`/`store atomic` nativos `seq_cst` |
| `tests/unit/Stage54_2AtomicsTests.cpp` | Superficie, calls y forma LLVM para ambos anchos |
| `docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md` | Precisa qué miembros satisfacen NS-MEM-002 |
| `docs/1.0.1/book/NativeSharp_El_Lenguaje_de_Programacion_ES.md` | Actualiza el inventario público de Core |

## Evidencia y disciplina

La prueba dirigida exige catorce llamadas núcleo y comprueba `load atomic i32/i64`,
`store atomic i32/i64` y `seq_cst` en el runtime. No se ejecuta durante la iteración de esta entrega.
Después del commit se reconstruye desde cero sólo `nsc`; las pruebas y el baseline acumulativo se
reservan para el cierre posterior a la entrega 234.

---

## English

`AtomicInt32` and `AtomicInt64` already provided sequentially consistent read-modify-write
operations, but lacked direct atomic load and store. Delivery 226 adds `Load()` and `Store(value)`
for both widths. `Value` deliberately remains an ordinary diagnostic snapshot, so existing source
does not acquire a hidden synchronization cost.

The frontend recognizes exact class/name/signature contracts, synthesizes the existing `value_`
field address, preserves distinct HIR and MIR operations, and calls four native runtime hooks.
Those hooks emit naturally aligned LLVM `load atomic` and `store atomic` instructions with
`seq_cst` ordering. No Win32 or managed helper is involved.

NativeSharp 1.0.1 still exposes one safe ordering rather than a selectable-order matrix or public
fences. The directed test records all fourteen atomic calls and both load/store widths. Per the
active batch policy, only a clean compiler rebuild follows the commit; execution and the full
baseline are deferred until delivery 234.
