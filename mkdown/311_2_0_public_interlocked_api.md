# 311 — API pública Interlocked / Public Interlocked API

## Resumen / Summary

Esta entrega publica `NativeSharp.Core.Threading.Interlocked` para aplicar operaciones atómicas a
almacenamiento que ya existe. La ubicación se pasa con referencias reales NativeSharp:
`ref int`, `ref long` o `ref void*`.

This delivery publishes `NativeSharp.Core.Threading.Interlocked` for atomic operations over
existing storage. Locations use true NativeSharp references: `ref int`, `ref long`, or `ref void*`.

## Superficie pública / Public surface

Para `int` y `long`:

- `Load` y `Store`;
- `Increment`, `Decrement` y `Add`;
- `Exchange` y `CompareExchange`;
- una sobrecarga legacy `SequentiallyConsistent` y otra con `MemoryOrder` explícito.

Para `void*`:

- `Load`, `Store`, `Exchange` y `CompareExchange`;
- formas legacy y ordenadas;
- ninguna suma o incremento implícito.

`CompareExchange` ordenado recibe órdenes separados de éxito y fallo. Las reglas son las mismas
que ya certifican los wrappers: Load no admite Release, Store no admite Acquire, el fallo de CAS no
admite Release/AcquireRelease ni un orden más fuerte que el éxito, y ningún orden puede decidirse
en ejecución.

## Semántica de referencias y ownership / Reference and ownership semantics

La palabra `ref` es obligatoria en la firma y en la llamada. Su expresión se analiza con las reglas
ordinarias de addressability, por lo que no se crea una copia ni un temporal oculto. El primer
argumento produce la dirección exacta de la local, parámetro o campo y se evalúa una sola vez.

Interlocked no asigna storage, no adquiere ownership y no extiende el lifetime. En la familia
`void*`, la dirección es no propietaria: publicar otra dirección o `null` no destruye ni libera el
pointee. Los accesos ordinarios realizados fuera de Interlocked continúan siendo ordinarios.

The call-site `ref` expression supplies the exact address of existing mutable storage and is
evaluated once. No copy, hidden temporary, allocation, ownership transfer, or lifetime extension is
introduced. Pointer operations retain non-owning semantics.

## Contratos de retorno / Return contracts

- Load devuelve el valor observado.
- Store no devuelve valor.
- Increment, Decrement y Add devuelven el valor nuevo.
- Exchange devuelve el valor anterior.
- CompareExchange siempre devuelve el valor anterior; si la comparación falla, la ubicación no
  cambia y el retorno es el valor observado.

These contracts are independent of memory order and match the previously certified wrapper API.

## Diseño del compilador / Compiler design

`ProjectMethodBodyModel::analyzeCall` reconoce exclusivamente la clase
`NativeSharp.Core.Threading.Interlocked`, métodos estáticos y un primer parámetro con
`ParameterPassKind::Ref` cuyo pointee sea exactamente `builtin:int`, `builtin:long` o
`builtin:void*`. El desplazamiento de parámetros permite reutilizar las mismas comprobaciones de
nombre, firma, retorno y órdenes que `AtomicInt32`, `AtomicInt64` y `AtomicPointer`.

Los wrappers conservan `MethodCoreBinding::implicitFieldMemberOrdinal` para identificar `value_`.
Interlocked deja ese dato vacío. `ProjectOopHirModel` interpreta las dos formas así:

1. wrapper: evalúa el receptor y sintetiza `FieldAddress(value_)`;
2. Interlocked: baja el primer argumento `ref`, ya representado como un valor dirección.

Después de obtener la ubicación, ambos caminos crean los mismos nodos HIR atómicos. No se agregaron
operaciones MIR, funciones runtime ni ramas LLVM exclusivas de Interlocked. El lowering existente
mantiene ancho, alineación, overflow, órdenes y valores de retorno.

The frontend distinguishes only the source of the location. Once obtained, wrapper and static
calls converge on the same Method/HIR/MIR/LLVM operation identities. This avoids semantic drift
and keeps one implementation of each atomic primitive.

## Código LLVM / LLVM code

Las formas enteras legacy conservan los hooks runtime ya certificados; las formas ordenadas emiten
`load atomic`, `store atomic`, `atomicrmw` y `cmpxchg` con i32/i64 y alineación 4/8. Los punteros
emiten operaciones `ptr` alineadas a ocho bytes. No hay `ptrtoint`/`inttoptr`, import Win32 ni
helper `__atomic_*` adicional.

## Archivos creados / Created files

- `tests/unit/Item311InterlockedApiTests.cpp`: prueba enfocada O0 de API, IR, diagnósticos, enlace
  y ejecución.
- `mkdown/311_2_0_public_interlocked_api.md`: historia bilingüe, superficie y decisiones.

## Archivos modificados / Modified files

- `lib/NativeSharp.Core.Threading/Atomics.ns`: clase pública `Interlocked` y comentario actualizado.
- `src/Frontend/ProjectMethodBodyModel.cpp`: reconocimiento exacto y desplazamiento del argumento
  ubicación.
- `src/Frontend/ProjectOopHirModel.cpp`: ubicación implícita de wrapper o explícita por `ref`.
- `CMakeLists.txt`: objetivo `nsc_item311_interlocked_api_tests`.
- especificación, contrato del compilador, mapas de conformidad, estado, changelog y roadmap 2.0.

No fue necesario modificar MIR, LLVM ni runtime porque el diseño reutiliza sus operaciones
existentes.

## Evidencia enfocada / Focused evidence

```text
cmake --build A:\nsc-devel\build-tests --target nsc_item311_interlocked_api_tests -j 8
ctest --test-dir A:\nsc-devel\build-tests -R "^nsc.item311.interlocked-api$" --output-on-failure
1/1 passed
```

La prueba compila y ejecuta un programa O0 que cubre `int`, `long` y `void*`, formas
legacy/ordenadas, CAS exitoso/fallido, `null` y contratos de retorno. Inspecciona IR por ancho y
familia, y comprueba diagnósticos para órdenes inválidos, orden dinámico y omisión de `ref`.

The focused O0 test covers all three location families, legacy and ordered calls, successful and
failed CAS, null, return contracts, emitted IR, illegal/dynamic orders, and missing `ref`. No
baseline was run for this item.

## Decisiones / Decisions

- Usar `ref` en vez de exponer `int*`/`long*` como conveniencia pública.
- Mantener los wrappers para valores atómicos que pertenecen a objetos.
- No duplicar lowering por ser una llamada estática.
- No publicar aritmética de `void*`.
- Exigir órdenes de compilación y conservar una sola matriz de validez.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Publica la API nativa Interlocked / Publish the native Interlocked API`

**Descripción / Description**

`Expone operaciones atómicas estáticas sobre ref int, ref long y ref void*, reutiliza la validación
y el lowering existentes, y certifica órdenes, retornos y direcciones en O0. / Expose static atomic
operations over ref int, ref long, and ref void*, reuse existing validation and lowering, and
certify ordering, return values, and addresses at O0.`
