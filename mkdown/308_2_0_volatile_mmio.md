# 308 — Memoria `volatile` para MMIO / Volatile MMIO memory

## Resumen / Summary

Esta entrega incorpora `volatile` como modificador de campos para registros de memoria mapeada a
dispositivos. El contrato atraviesa lexer, parser, modelo de miembros, HIR, MIR, LLVM y NSMETA. No
es una segunda API de atomics y no agrega llamadas de runtime.

This delivery adds `volatile` as a field modifier for memory-mapped device registers. The contract
crosses the lexer, parser, member model, HIR, MIR, LLVM, and NSMETA. It is not a second atomic API
and adds no runtime calls.

## Superficie pública / Public surface

```csharp
public struct DeviceRegisters
{
    public volatile uint Status;
    public volatile ushort Control;
    public uint Reserved;
}

public class Device
{
    private static volatile uint Doorbell;

    public static uint Touch(DeviceRegisters* registers)
    {
        registers->Status = registers->Status + 1u;
        registers->Control += 2;
        Doorbell = registers->Status;
        return Doorbell + registers->Reserved;
    }
}
```

Se permiten campos de instancia y estáticos de tipo escalar nativo, enum o puntero crudo, siempre
que no lleven ownership. Una asignación compuesta conserva un solo cálculo de dirección y marca
como volatile tanto su carga como su almacén.

Instance and static fields of native scalar, enum, or raw-pointer type are accepted when they do
not carry ownership. A compound assignment evaluates its address once and marks both its load and
store as volatile.

## Lo que garantiza y lo que no / Guarantees and non-guarantees

Un acceso a un campo volatile es observable para el compilador. LLVM recibe `load volatile` o
`store volatile` sobre la ubicación exacta. Esto impide que el acceso se elimine o se sustituya por
una observación que nunca toca el registro declarado.

No garantiza:

- atomicidad;
- exclusión mutua;
- publicación entre hilos;
- orden de memoria acquire/release;
- barrera de CPU o compilador;
- lifetime u ownership de una referencia.

Los protocolos entre hilos siguen usando `AtomicInt32`, `AtomicInt64`, `MemoryOrder` y
`AtomicFence`. `volatile` cubre MMIO; atomics cubren sincronización.

A volatile field access is observable to the compiler and lowers to an LLVM `load volatile` or
`store volatile` on the exact location. It provides no atomicity, synchronization, publication,
memory order, fence, or ownership guarantee. Thread protocols continue to use the atomic API.

## Rechazos deliberados / Deliberate rejections

El compilador rechaza volatile en:

- strings, arrays, clases, structs agregados y delegates;
- campos `owned` o `borrowed`;
- combinaciones con `readonly` o `const`;
- métodos, properties, eventos, indexers, operadores, constructores, destructores y tipos anidados;
- modificadores `volatile` duplicados.

La restricción de tipo evita prometer una sola transacción observable para copias físicas que
pueden requerir varias operaciones o maquinaria de lifetime. Los bitfields volatile permanecen en
el ítem 330 porque necesitan reglas adicionales de read-modify-write y ancho de acceso.

Managed/aggregate storage, ownership-qualified fields, immutable combinations, non-field members,
and duplicate modifiers are rejected. Volatile bit fields remain item 330 because their access
width and read-modify-write behavior require a separate contract.

## Diseño e implementación / Design and implementation

### Frontend

`KwVolatile` se añadió a `TokenKind`, al mapa de keywords y al nombre estable del token. El parser
lo acepta en la lista de modificadores de miembros, diagnostica duplicados y colocaciones inválidas
y sólo lo adjunta a nodos de campo.

`ProjectMemberModel::collectTypeMembers` fija `ClassMemberSymbol::isVolatile` y valida el tipo y
ownership mediante `isVolatileFieldType`. La decisión se toma una sola vez y los dumpers la hacen
visible para diagnóstico.

`KwVolatile` was added to the token vocabulary. Parsing diagnoses invalid placement and only
attaches it to fields. Member collection validates the complete type/ownership rule once and stores
an explicit bit rather than asking later stages to reinterpret source syntax.

### HIR y MIR

Los nodos `FieldAddress`, `InstanceFieldLoad` y `StaticFieldLoad` conservan `volatileAccess`. La
conversión HIR→MIR copia la marca y `ProjectOopMirStaticField` conserva la declaración estática.
Los inicializadores de instancia también marcan su destino para que una escritura explícita de
inicialización respete el mismo contrato.

HIR field-address/load nodes carry `volatileAccess`; MIR copies it without re-inference and static
field records retain the declaration bit. Instance initializer targets use the same path.

### LLVM

`LlvmCApi.h` declara `LLVMSetVolatile`. El lowerer lo aplica a:

- cargas de campo de instancia;
- cargas de campo estático;
- el load y store de `CompoundStore`;
- el store de una asignación ordinaria cuyo destino es volatile.

La instrucción permanece no-atómica. No se emiten `atomic`, `atomicrmw`, `cmpxchg` ni `fence` como
efecto colateral del modificador.

### NSMETA 1.14

El bit 23 de los flags de miembro y layout representa volatile. `LibraryMetadata.cpp` lo escribe en
ambos registros y `Driver::buildPrecompiledPublicSource` reconstruye `volatile` en campos públicos
de instancia o estáticos. Así un consumidor `LibraryReference` no degrada los accesos.

El formato sube de 1.13 a 1.14 porque el bit cambia semántica pública. `writeModel` elimina el bit
si se solicita 1.13 o anterior, y el verificador actual rechaza el bit en un miembro que no sea
campo. El Native ABI y Runtime ABI permanecen 1.0: no cambian layout, símbolos ni calling
convention.

Bit 23 in member and layout-field flags represents volatile. NSMETA 1.14 preserves and reconstructs
it for LibraryReference consumers. Down-level output strips the bit; the current verifier rejects
it on non-field members. Native ABI and Runtime ABI remain 1.0.

## Archivos creados / Created files

- `tests/unit/Item308VolatileMmioTests.cpp`: prueba O0 de sintaxis, semántica, LLVM, NSMETA y errores.
- `mkdown/308_2_0_volatile_mmio.md`: registro bilingüe de esta entrega.

## Archivos modificados / Modified files

- `CMakeLists.txt`: objetivo y prueba focalizada del ítem 308.
- lexer/parser: `TokenKind.h`, `Lexer.cpp`, `TokenKind.cpp`, `Parser.cpp`.
- frontend: modelos/dumpers de miembros, OOP HIR y OOP MIR.
- LLVM: `LlvmCApi.h` y `ProjectOopLlvmLowerer.cpp`.
- metadata/consumo: `LibraryMetadata.h`, `LibraryMetadata.cpp` y `Driver.cpp`.
- documentación 1.0.1: especificación, contrato, mapas, estado, versión, changelog y roadmaps.
- `Stage69MigrationBaselineTests.cpp`: expectativa del formato NSMETA actual 1.14.

## Evidencia enfocada / Focused evidence

```text
cmake --build A:\nsc-devel\build-tests --target nsc_item308_volatile_mmio_tests -j 8
ctest --test-dir A:\nsc-devel\build-tests -R "^nsc.item308.volatile-mmio$" --output-on-failure
1/1 passed
```

La prueba inspecciona campos de instancia/estáticos, accesos por `->`, asignación ordinaria y
compuesta, IR verifier-clean, ausencia de atomics/fences, round-trip NSMETA 1.14, stripping 1.13 y
cinco categorías negativas. No se ejecutó baseline; corresponde al cierre de la tanda 5.

The focused test covers instance/static fields, pointer-member access, ordinary and compound
assignments, verifier-clean IR, absence of atomics/fences, NSMETA 1.14 round-trip, 1.13 stripping,
and five negative categories. No baseline was run; the batch baseline remains at tanda 5 closure.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Implementa memoria volatile para MMIO / Implement volatile MMIO memory`

**Descripción / Description**

`Añade volatile field-only para escalares, enums y punteros, conserva su semántica en HIR/MIR/LLVM y NSMETA 1.14, y certifica que no implica atomics ni fences. / Add field-only volatile for scalars, enums, and pointers, preserve its semantics through HIR/MIR/LLVM and NSMETA 1.14, and certify that it implies neither atomics nor fences.`
