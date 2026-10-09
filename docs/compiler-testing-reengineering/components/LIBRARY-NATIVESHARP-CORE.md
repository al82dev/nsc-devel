# Library.NativeSharp.Core — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 48 archivos, 7474 líneas, 629 declaraciones/artefactos.
- **Disposición:** EXCLUIDA=4, INDIRECTA=44.
- **Diseño futuro:** 44 contratos y 264 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Proveer la biblioteca base distribuida: memoria, texto, números, tiempo, ownership, vectores e intrínsecos.

**Dependencias:** runtime Win64;lowering intrínseco de NSC.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000412 | `lib/NativeSharp.Core/Allocator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-001` | Superficie de biblioteca Allocator |
| PF-000413 | `lib/NativeSharp.Core/ArenaAllocator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-002` | Superficie de biblioteca Arena Allocator |
| PF-000414 | `lib/NativeSharp.Core/Assertions.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-003` | Superficie de biblioteca Assertions |
| PF-000415 | `lib/NativeSharp.Core/BinaryReader.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-004` | Superficie de biblioteca Binary Reader |
| PF-000416 | `lib/NativeSharp.Core/BinaryWriter.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-005` | Superficie de biblioteca Binary Writer |
| PF-000417 | `lib/NativeSharp.Core/BitOperations.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-006` | Superficie intrínseca Bit Operations |
| PF-000418 | `lib/NativeSharp.Core/BorrowedReference.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-007` | Superficie de biblioteca Borrowed Reference |
| PF-000419 | `lib/NativeSharp.Core/ByteBuffer.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-008` | Superficie de biblioteca Byte Buffer |
| PF-000420 | `lib/NativeSharp.Core/Console.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-009` | Superficie de biblioteca Console |
| PF-000421 | `lib/NativeSharp.Core/CString.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-010` | Superficie de biblioteca CString |
| PF-000422 | `lib/NativeSharp.Core/DateTime.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-011` | Superficie de biblioteca Date Time |
| PF-000423 | `lib/NativeSharp.Core/DeterministicMath.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-012` | Superficie de biblioteca Deterministic Math |
| PF-000424 | `lib/NativeSharp.Core/Environment.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-013` | Superficie de biblioteca Environment |
| PF-000425 | `lib/NativeSharp.Core/Exception.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-014` | Superficie de biblioteca Exception |
| PF-000426 | `lib/NativeSharp.Core/Float32x2.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-015` | Superficie intrínseca Float32x2 |
| PF-000427 | `lib/NativeSharp.Core/Float32x3.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-016` | Superficie intrínseca Float32x3 |
| PF-000428 | `lib/NativeSharp.Core/Float32x3x3.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-017` | Superficie de biblioteca Float32x3x3 |
| PF-000429 | `lib/NativeSharp.Core/Float32x4.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-018` | Superficie intrínseca Float32x4 |
| PF-000430 | `lib/NativeSharp.Core/Float32x4x4.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-019` | Superficie de biblioteca Float32x4x4 |
| PF-000431 | `lib/NativeSharp.Core/Float32x8.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-020` | Superficie intrínseca Float32x8 |
| PF-000432 | `lib/NativeSharp.Core/Float64x4.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-021` | Superficie intrínseca Float64x4 |
| PF-000433 | `lib/NativeSharp.Core/Half.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-022` | Superficie intrínseca Half |
| PF-000434 | `lib/NativeSharp.Core/HardwareIntrinsics.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-023` | Superficie intrínseca Hardware Intrinsics |
| PF-000435 | `lib/NativeSharp.Core/Int32x4.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-024` | Superficie intrínseca Int32x4 |
| PF-000436 | `lib/NativeSharp.Core/Int32x8.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-025` | Superficie intrínseca Int32x8 |
| PF-000437 | `lib/NativeSharp.Core/Int64x2.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-026` | Superficie intrínseca Int64x2 |
| PF-000438 | `lib/NativeSharp.Core/Int64x4.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-027` | Superficie intrínseca Int64x4 |
| PF-000439 | `lib/NativeSharp.Core/LANGUAGE-3-MIGRATION.md` | `LIBRARY_DOCUMENT` | `EXCLUIDA` | `` | Documento histórico o de migración LANGUAGE 3 MIGRATION; aporta procedencia/contradicciones, no un contrato ejecutable del compilador. |
| PF-000440 | `lib/NativeSharp.Core/Lazy.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-028` | Superficie de biblioteca Lazy |
| PF-000441 | `lib/NativeSharp.Core/MathUtil.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-029` | Superficie de biblioteca Math Util |
| PF-000442 | `lib/NativeSharp.Core/NativeClock.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-030` | Superficie intrínseca Native Clock |
| PF-000443 | `lib/NativeSharp.Core/NativeSharp.Core.nsproj` | `LIBRARY_PROJECT` | `INDIRECTA` | `C12B-COR-031` | Proyecto distribuido Native Sharp.Core |
| PF-000444 | `lib/NativeSharp.Core/Number.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-032` | Superficie intrínseca Number |
| PF-000445 | `lib/NativeSharp.Core/NumericLimits.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-033` | Superficie intrínseca Numeric Limits |
| PF-000446 | `lib/NativeSharp.Core/Object.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-COR-034` | Superficie intrínseca Object |
| PF-000447 | `lib/NativeSharp.Core/Optional.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-035` | Superficie de biblioteca Optional |
| PF-000448 | `lib/NativeSharp.Core/Random.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-036` | Superficie de biblioteca Random |
| PF-000449 | `lib/NativeSharp.Core/README.md` | `LIBRARY_DOCUMENT` | `EXCLUIDA` | `` | Documento histórico o de migración README; aporta procedencia/contradicciones, no un contrato ejecutable del compilador. |
| PF-000450 | `lib/NativeSharp.Core/Rgb10A2Unorm.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-037` | Superficie de biblioteca Rgb10 A2 Unorm |
| PF-000451 | `lib/NativeSharp.Core/Rgba8Unorm.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-038` | Superficie de biblioteca Rgba8 Unorm |
| PF-000452 | `lib/NativeSharp.Core/SPAN-INDEXER-COMPATIBILITY.md` | `LIBRARY_DOCUMENT` | `EXCLUIDA` | `` | Documento histórico o de migración SPAN INDEXER COMPATIBILITY; aporta procedencia/contradicciones, no un contrato ejecutable del compilador. |
| PF-000453 | `lib/NativeSharp.Core/Span.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-039` | Superficie de biblioteca Span |
| PF-000454 | `lib/NativeSharp.Core/String.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-040` | Superficie de biblioteca String |
| PF-000455 | `lib/NativeSharp.Core/StringBuilder.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-041` | Superficie de biblioteca String Builder |
| PF-000456 | `lib/NativeSharp.Core/STRINGVIEW-INDEXER-COMPATIBILITY.md` | `LIBRARY_DOCUMENT` | `EXCLUIDA` | `` | Documento histórico o de migración STRINGVIEW INDEXER COMPATIBILITY; aporta procedencia/contradicciones, no un contrato ejecutable del compilador. |
| PF-000457 | `lib/NativeSharp.Core/StringView.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-042` | Superficie de biblioteca String View |
| PF-000458 | `lib/NativeSharp.Core/TimeSpan.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-043` | Superficie de biblioteca Time Span |
| PF-000459 | `lib/NativeSharp.Core/Utf8.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COR-044` | Superficie de biblioteca Utf8 |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-COR-001` | Superficie de biblioteca Allocator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-002` | Superficie de biblioteca Arena Allocator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-003` | Superficie de biblioteca Assertions | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-004` | Superficie de biblioteca Binary Reader | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-005` | Superficie de biblioteca Binary Writer | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-006` | Superficie intrínseca Bit Operations | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-007` | Superficie de biblioteca Borrowed Reference | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-008` | Superficie de biblioteca Byte Buffer | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-009` | Superficie de biblioteca Console | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-010` | Superficie de biblioteca CString | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-011` | Superficie de biblioteca Date Time | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-012` | Superficie de biblioteca Deterministic Math | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-013` | Superficie de biblioteca Environment | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-014` | Superficie de biblioteca Exception | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-015` | Superficie intrínseca Float32x2 | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-016` | Superficie intrínseca Float32x3 | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-017` | Superficie de biblioteca Float32x3x3 | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-018` | Superficie intrínseca Float32x4 | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-019` | Superficie de biblioteca Float32x4x4 | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-020` | Superficie intrínseca Float32x8 | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-021` | Superficie intrínseca Float64x4 | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-022` | Superficie intrínseca Half | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-023` | Superficie intrínseca Hardware Intrinsics | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-024` | Superficie intrínseca Int32x4 | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-025` | Superficie intrínseca Int32x8 | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-026` | Superficie intrínseca Int64x2 | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-027` | Superficie intrínseca Int64x4 | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-028` | Superficie de biblioteca Lazy | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-029` | Superficie de biblioteca Math Util | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-030` | Superficie intrínseca Native Clock | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-031` | Proyecto distribuido Native Sharp.Core | `integration` | fuentes, referencias y metadata de biblioteca | grafo de proyecto, unidades, referencias y artefactos publicados |
| `C12B-COR-032` | Superficie intrínseca Number | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-033` | Superficie intrínseca Numeric Limits | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-034` | Superficie intrínseca Object | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-COR-035` | Superficie de biblioteca Optional | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-036` | Superficie de biblioteca Random | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-037` | Superficie de biblioteca Rgb10 A2 Unorm | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-038` | Superficie de biblioteca Rgba8 Unorm | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-039` | Superficie de biblioteca Span | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-040` | Superficie de biblioteca String | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-041` | Superficie de biblioteca String Builder | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-042` | Superficie de biblioteca String View | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-043` | Superficie de biblioteca Time Span | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COR-044` | Superficie de biblioteca Utf8 | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 220.
- Casos NIGHTLY/weekly: 44.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.