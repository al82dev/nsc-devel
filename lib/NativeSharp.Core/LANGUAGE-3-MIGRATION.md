# NativeSharp.Core — auditoría final de Language 3

## Alcance canónico

- Componente: `stdlib/NativeSharp.Core`.
- Proyecto: `NativeSharp.Core.nsproj`.
- Lenguaje declarado: `3`.
- Producto: biblioteca estática `NativeSharpCore` para Windows x64.
- Dependencias de biblioteca: ninguna.
- Fuentes de producto: 42 archivos `.ns`; no se encontraron duplicados ni source references.
- Revisión: auditoría archivo, tipo y método sobre el árbol `development` del 5 de octubre de 2026.

## Matriz de cobertura

| Área | Archivos revisados | Contrato comprobado |
|---|---|---|
| Memoria I | `Allocator.ns`, `ArenaAllocator.ns` | Ownership explícito y vida del almacenamiento. |
| Memoria II | `ByteBuffer.ns`, `CString.ns` | Buffers owned y vistas crudas. |
| Binario | `BinaryReader.ns`, `BinaryWriter.ns` | Cursor borrowed copy, errores por `Optional<T>` y endianess. |
| Plataforma | `Console.ns`, `Environment.ns`, `NativeClock.ns` | Superficie estática sin ownership oculto. |
| Errores | `Assertions.ns`, `Exception.ns` | Jerarquía, `owned` de causa y contratos `throws`. |
| Genéricos | `Lazy.ns`, `Optional.ns` | Copy, move, almacenamiento y extracción de payloads. |
| Texto I | `String.ns`, `StringBuilder.ns` | Owned string y buffer mutable. |
| Texto II | `StringView.ns`, `Utf8.ns` | Vista borrowed copy y codificación. |
| Valores | `DateTime.ns`, `TimeSpan.ns`, `Half.ns` | Tipos pequeños publicados como `copy struct`. |
| Números I | `DeterministicMath.ns`, `MathUtil.ns` | Operaciones matemáticas sin ownership. |
| Números II | `Number.ns`, `NumericLimits.ns` | Conversiones y límites. |
| Bits | `BitOperations.ns`, `HardwareIntrinsics.ns` | Superficie intrínseca preservada sin estado propietario. |
| Objetos | `Object.ns`, `Random.ns` | Identidad, estado mutable y ausencia de copias de clases. |
| Vistas | `Span.ns` | `Span<T>` y `ReadOnlySpan<T>` como `copy borrowed struct`. |
| SIMD 32 | `Float32x2.ns`, `Float32x3.ns`, `Float32x4.ns`, `Float32x8.ns` | Carriers copy y operaciones por valor. |
| SIMD 64 | `Float64x4.ns`, `Int64x2.ns`, `Int64x4.ns` | Carriers copy y operaciones por valor. |
| SIMD int | `Int32x4.ns`, `Int32x8.ns` | Carriers copy, carga y almacenamiento por puntero. |
| Matrices | `Float32x3x3.ns`, `Float32x4x4.ns` | Columnas copy y transformaciones por valor. |
| Packed | `Rgb10A2Unorm.ns`, `Rgba8Unorm.ns` | Representación compacta copy sin ownership. |

La matriz cubre los 42 archivos del proyecto. Se revisaron constructores, métodos estáticos, métodos de
instancia, indexadores, propiedades, destructores implícitos y ramas de error de cada tipo.

## Correcciones de esta auditoría

### `Lazy<T>`

`Lazy<T>` devuelve el mismo valor almacenado en llamadas sucesivas. En Language 3 eso exige que el payload
sea copiable. La clase ahora declara `where T : copy`; una materialización con un struct move-only se rechaza
en el consumidor con el diagnóstico de la restricción.

`Value()` también publica `throws InvalidOperationException`, porque detecta recursión de la fábrica y lanza
ese error. Los consumidores deben capturarlo o declararlo.

### `Optional<T>`

El comentario existente prometía payloads move-only, pero el constructor copiaba `item` al campo y la
materialización fallaba. El constructor ahora transfiere el parámetro con `move(item)`.

Se agregaron tres operaciones de consumo:

- `Take() throws InvalidOperationException`: extrae el payload y deja el optional vacío;
- `TakeOr(T fallback)`: extrae el payload o consume el fallback;
- `UncheckedTake()`: extrae sin comprobar presencia.

`Value()`, `ValueOr()` y `UncheckedValue()` conservan la semántica de observación por copia. La fábrica
`Optionals.Some<T>` declara `where T : copy`; un payload move-only usa el constructor directo y una operación
`Take`. `Optionals.None<T>` continúa siendo válida para cualquier T.

### Estilo

Las cuatro expresiones de `Float32x4x4.Transform` se dividieron sin alterar el algoritmo para cumplir el
límite obligatorio de 120 caracteres.

## Ownership, copy y borrowed

- Los carriers numéricos, fechas, horas y formatos packed permanecen `copy struct`.
- `Span<T>`, `ReadOnlySpan<T>`, `StringView` y `BinaryReader` permanecen `copy borrowed struct`.
- `ArenaBlock`, `ArenaAllocator`, `ByteBuffer`, `CString`, `Exception` y `StringBuilder` conservan sus campos
  `owned`; no se convirtió ownership en referencias ordinarias.
- `Optional<T>` permanece move-only y puede transportar un payload move-only mediante extracción explícita.
- `Lazy<T>` sólo acepta payloads copy porque su contrato observable permite leer el valor varias veces.
- No se añadieron `copy` ni `move(...)` a tipos cuyo contrato no lo requiere.

## Unsafe y punteros

Se revisaron los punteros de `Allocator`, `ArenaAllocator`, `CString`, `Span`, `StringView`, `BinaryReader`,
`Int32x4`, `String` y `HardwareIntrinsics`. Son parte de la superficie de memoria de Core o del contrato
intrínseco existente. El NSC oficial acepta esos cuerpos en Language 3 y no emitió un requisito `unsafe`
pendiente. No se añadieron bloques `unsafe` artificiales.

## Excepciones comprobadas

- `Assertions` publica `throws AssertionException` en los métodos que lanzan.
- `Optional.Value` y `Optional.Take` publican `throws InvalidOperationException`.
- `Lazy.Value` publica ahora `throws InvalidOperationException`.
- No se encontraron otros `throw new` sin contrato en las 42 fuentes.

## Evidencia O0

Compilador autorizado:

```text
C:\native-sharp\bin\nsc.exe
NativeSharp Compiler 3.0.0
Build hash: 2a44eedad51a7a9b
```

El proyecto completo compiló con `--emit-project-oop-lib` y `-O0` en un root privado de `A:`:

```text
42/42 fuentes
11 code units
15 archive members
0 library references
NativeSharpCore.lib
NativeSharpCore.h
NativeSharpCore.nsmeta
```

La integración se validó mediante referencia binaria contra esos tres artefactos y con:

```text
A:\nslink-devel\build-release\bin\nslink.exe
```

Resultados puntuales:

- smoke de `Optional<int>`, `Optional<MoveToken>`, `Lazy<int>`, fechas, SIMD, bits, binario y texto: exit 42;
- sonda de `Optional<MoveToken>` con `UncheckedTake`: compiló, enlazó y ejecutó con exit 42;
- sonda negativa de `Lazy<MoveToken>`: rechazo esperado por `where T : copy`;
- metadata NSMETA 1.38 válida: 71 tipos, 671 miembros y ABI fingerprint verificado;
- coherencia metadata/archive válida: 15 miembros de archive y 647 símbolos requeridos;
- artefactos producidos y ejecutables: Windows AMD64, O0, enlazados con NSLink release.

## Estado

Estado del componente: `Verificado` para la migración Language 3 y la matriz puntual O0 acordada.

No se ejecutaron baseline, minibaseline, O1, O2 ni O3. No se instaló ni escribió ningún artefacto en `C:`.
