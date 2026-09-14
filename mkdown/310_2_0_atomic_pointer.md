# 310 — Atomics de punteros / Pointer atomics

## Resumen / Summary

Esta entrega incorpora `NativeSharp.Core.Threading.AtomicPointer`, una ubicación atómica para
direcciones nativas `void*` no propietarias. La API cubre carga, almacenamiento, intercambio y
compare-exchange con formas legacy y ordenadas. El backend conserva el tipo puntero hasta LLVM.

This delivery adds `NativeSharp.Core.Threading.AtomicPointer`, an atomic location for non-owning
native `void*` addresses. The API covers load, store, exchange, and compare-exchange through legacy
and explicitly ordered forms. The backend retains pointer identity through LLVM.

## Superficie pública / Public surface

```csharp
public class AtomicPointer
{
    public AtomicPointer(void* initialValue);

    public void* Load();
    public void* Load(MemoryOrder order);

    public void Store(void* value);
    public void Store(void* value, MemoryOrder order);

    public void* Exchange(void* value);
    public void* Exchange(void* value, MemoryOrder order);

    public void* CompareExchange(void* value, void* comparand);
    public void* CompareExchange(
        void* value,
        void* comparand,
        MemoryOrder successOrder,
        MemoryOrder failureOrder);
}
```

La dirección puede ser `null`. El tipo no adquiere ownership del pointee: Store, Exchange y CAS no
destruyen, liberan ni transfieren el objeto apuntado. El llamador debe asegurar que una dirección
observada siga viva durante su uso.

Null is valid. The type never acquires pointee ownership: store, exchange, and CAS do not destroy,
free, or transfer the referenced object. The caller remains responsible for pointee lifetime.

## Semántica de operaciones / Operation semantics

- `Load` devuelve la dirección observada.
- `Store` publica una dirección y no devuelve valor.
- `Exchange` publica la dirección nueva y devuelve la anterior.
- `CompareExchange` publica `value` sólo si la dirección observada equivale a `comparand`; siempre
  devuelve la dirección observada antes del intento.

Las formas sin orden son `SequentiallyConsistent`. Load permite Relaxed, Acquire o
SequentiallyConsistent. Store permite Relaxed, Release o SequentiallyConsistent. Exchange permite
los cinco órdenes. CompareExchange usa órdenes independientes de éxito y fallo y aplica la misma
matriz validada por los atomics enteros.

Legacy forms are sequentially consistent. Ordered operations reuse the established load, store,
RMW, and compare-exchange legality matrix. Orders are compile-time constants, never runtime flags.

## Sin aritmética implícita / No implicit arithmetic

`AtomicPointer` no publica Increment, Decrement ni Add. Un `void*` no tiene tamaño de elemento y la
API no inventa si una unidad significa byte, palabra o estructura. El programa calcula una nueva
dirección con un puntero tipado y la publica mediante Store, Exchange o CompareExchange.

AtomicPointer intentionally has no Increment, Decrement, or Add. A `void*` has no element size, so
the API does not invent one. Programs compute a new address through a typed pointer and publish it
with Store, Exchange, or CompareExchange.

## Diseño e implementación / Design and implementation

`ProjectMethodBodyModel` reconoce únicamente las firmas exactas de AtomicPointer y reutiliza la
validación común de `MemoryOrder`. Resuelve una vez el campo privado `value_` y conserva su ordinal
en `implicitFieldMemberOrdinal`.

`ProjectOopHirModel` evalúa el receptor una sola vez, construye la dirección sintética del campo y
elimina los argumentos de orden ya plegados. HIR y MIR tienen identidades explícitas para las cuatro
familias de operación, incluida la metadata compacta de órdenes de CAS.

`ProjectOopLlvmLowerer` emite directamente:

- `load atomic ptr`;
- `store atomic ptr`;
- `atomicrmw xchg ptr`;
- `cmpxchg ptr`.

Todas declaran alineación 8 en Windows x64. No se usa `ptrtoint`/`inttoptr`; por tanto no se pierde
la naturaleza de dirección ni se acopla el contrato a un entero firmado. Tampoco se añade un hook
de runtime, un import `InterlockedExchangePointer` o una dependencia `__atomic_*`.

The frontend recognizes exact signatures, evaluates the receiver once, and carries explicit HIR
and MIR operation identities. LLVM emits pointer-typed atomics directly with eight-byte alignment.
There is no integer round-trip, runtime hook, Win32 import, or external atomic helper.

## Rechazos deliberados / Deliberate rejections

- Load con Release o AcquireRelease.
- Store con Acquire o AcquireRelease.
- failure order Release o AcquireRelease.
- failure order más fuerte que success.
- cualquier orden fuera de `MemoryOrder`.
- cualquier orden calculado en ejecución.

Estos rechazos ocurren antes de LLVM y comparten los diagnósticos de las operaciones enteras.

## Archivos creados / Created files

- `tests/unit/Item310AtomicPointerTests.cpp`: prueba O0 positiva, negativa, IR y ejecución.
- `mkdown/310_2_0_atomic_pointer.md`: historia bilingüe y decisiones de diseño.

## Archivos modificados / Modified files

- `lib/NativeSharp.Core.Threading/Atomics.ns`: superficie pública `AtomicPointer`.
- headers frontend: nuevas identidades de operación Method/HIR/MIR.
- `src/Frontend/ProjectMethodBodyModel.cpp`: reconocimiento y validación de firmas/órdenes.
- `src/Frontend/ProjectOopHirModel.cpp`: dirección implícita del campo y argumentos.
- `src/Frontend/ProjectOopMirModel.cpp`: transporte exacto HIR→MIR.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: lowering atómico pointer-typed.
- `CMakeLists.txt`: objetivo `nsc_item310_atomic_pointer_tests`.
- documentación 1.0.1: especificación, contrato, mapas, estado, changelog y roadmap.

## Evidencia enfocada / Focused evidence

```text
cmake --build A:\nsc-devel\build-tests --target nsc_item310_atomic_pointer_tests -j 8
ctest --test-dir A:\nsc-devel\build-tests -R "^nsc.item310.atomic-pointer$" --output-on-failure
1/1 passed
```

La prueba compila y ejecuta un programa O0 con tres direcciones de stack, `null`, operaciones
legacy/ordenadas y CAS exitoso/fallido. También inspecciona IR `ptr`, alineación 8 y ausencia de
helpers, y exige cuatro diagnósticos de órdenes inválidos. No se ejecutó baseline.

The O0 test compiles and runs stack-address, null, legacy/ordered, and successful/failed CAS cases.
It also inspects pointer IR, alignment, helper absence, and four invalid-order diagnostics. No
baseline was run.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Implementa atomics nativos de punteros / Implement native pointer atomics`

**Descripción / Description**

`Añade AtomicPointer para void* no propietario con Load, Store, Exchange y CompareExchange legacy/ordenados, lowering LLVM ptr alineado y pruebas O0 de identidad, null y órdenes. / Add AtomicPointer for non-owning void* with legacy/ordered Load, Store, Exchange, and CompareExchange, aligned LLVM ptr lowering, and O0 identity, null, and ordering tests.`
