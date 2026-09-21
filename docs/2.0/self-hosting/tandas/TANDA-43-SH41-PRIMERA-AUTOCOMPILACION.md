# NativeSharp 2.0 self-hosting — tanda 43: primera autocompilación real

## Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` — gate dirigido y baseline acumulado aprobados |
| Gate | `SH41` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `a3e986f1d6b8955502ec8bf5289a1e67f0418b8d` |
| Commit de cierre | este commit (`selfhost(tanda-43): complete the first source-derived self-compilation`) |

## Resultado

Esta tanda elimina la simulación histórica de SH11. `nsc1`, construido una vez por `nsc0`, consume
el proyecto fuente completo del compilador y emite con LLVM-C el objeto COFF de `nsc2`. El ejecutable
`nsc2` vuelve a consumir exactamente esas fuentes y produce el objeto de `nsc3`. Los objetos de ambas
generaciones son idénticos byte por byte en O0, O1, O2 y O3.

No se copia ni reproduce la imagen de entrada, no se invoca `GenerationBuilder` y no se ejecuta
`nsc0` durante ninguna generación. El auditor observa el árbol de procesos mientras cada productor
está vivo y registra comandos, descendientes, tiempos y hashes.

El gate creó y ejecutó 16 imágenes: dos generaciones, cuatro perfiles y dos enlazadores. Todas
reportaron la identidad contractual, aprobaron `--self-test-basic` con código 42 y analizaron las 26
unidades del proyecto del compilador con cero fallos.

## Diseño de la implementación

### Frontend y semántica

- `Parser.ParseType` y sus consumidores aceptan arrays fijos, punteros, genéricos anidados y formas
  de declaración usadas por el propio compilador.
- `SemanticAnalyzer` conserva propietarios genéricos, sustituye parámetros de tipo y resuelve
  miembros, indexers, constructores y overloads entre las 96 unidades del grafo bootstrap.
- `TypeSystem` y `Symbols` publican layout nominal, tamaño, offset y tipo cerrado para que el backend
  no reconstruya semántica a partir del texto.
- `SourceBuffer` materializa explícitamente su tabla de líneas; `StringBuilder` copia caracteres sin
  depender de una conversión `Span<T>` que el bootstrap todavía no podía bajar correctamente.

### IR y backend LLVM

`LlvmBackend` deja de ser una rebanada escalar y baja la superficie que el compilador usa realmente:

- funciones de instancia, receptores, constructores, campos y llamadas indirectas;
- reachability del grafo callable y selección determinista de overload;
- objetos nominales, arrays fijos y dinámicos con cabecera de longitud;
- indexers genéricos, boxing/unboxing del borrado genérico y conversiones de enum;
- punteros y aritmética por tamaño de elemento;
- strings nativos `{ i64 length; byte data[] }`, literales, igualdad, concatenación y substring;
- intrínsecos de bootstrap para `String`, `Environment`, `Console`, `File`, `Directory` y `Path`;
- caracteres escapados, incluidos `\v`, `\f`, `\xHH` y `\0`;
- condiciones booleanas/puntero y salidas fatales con bloque `unreachable` válido.

`LLVMApi.ns` añade únicamente las entradas LLVM-C necesarias para esas formas:
`LLVMStructCreateNamed`, `LLVMStructSetBody`, `LLVMConstStringInContext2`,
`LLVMConstNamedStruct`, `LLVMBuildSelect`, `LLVMGetNamedGlobal` y `LLVMSetLinkage`. Esta última
publica `_fltused` como COMDAT para que los fixtures autónomos lo satisfagan sin duplicarlo cuando
el producto final también enlaza `NativeSharp.Runtime.Win32.lib`.

### Política de vida durante promoción

El borrado genérico actual no conserva información suficiente para distinguir todas las instancias
cerradas que comparten una representación borrada. Liberarlas como si fueran buffers homogéneos
provocaba corrupción del heap durante `nsc2 -> nsc3`.

La política acotada de SH41 trata esas asignaciones como memoria de generación: viven hasta que
termina el proceso compilador. Los buffers concretos comprobados de `Replace` y `Asegurar` liberan
mediante el hook de runtime `__nativesharp_free`, y el fixture autónomo `Main` prueba directamente
`HeapFree`. No es una afirmación de ownership final; SH46/SH48 deben
sustituir esta frontera por destrucción tipada antes de la certificación SH54. El gate evita tanto
la corrupción como una liberación genérica inventada.

## Archivos y responsabilidades

| Archivo | Responsabilidad en SH41 |
|---|---|
| `selfhost/nsc/Parser/Parser.ns` | formas sintácticas requeridas por las fuentes reales |
| `src/Parser/Parser.cpp` | progreso garantizado al recuperar bloques `extern C` inválidos |
| `selfhost/nsc/Sema/SemanticAnalyzer.ns` | sustitución genérica y resolución multimódulo |
| `selfhost/nsc/Types/TypeSystem.ns` | tipos cerrados, arrays y layout |
| `selfhost/nsc/Symbols/Symbols.ns` | propietarios y offsets nominales |
| `selfhost/nsc/IR/IntermediateRepresentation.ns` | representación completa del programa |
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | emisión fuente→LLVM→COFF autónoma |
| `selfhost/nsc/Driver/Program.ns` | producto `--emit-bootstrap-object` y diagnóstico de rechazo |
| `selfhost/nsc/Syntax/SyntaxTree.ns` | AST público compatible sin perder genéricos internos |
| `selfhost/nsc/Sema/SemanticDumper.ns` | dump compatible conservando hechos internos de lowering |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | frontera LLVM-C adicional |
| `lib/NativeSharp.Core/StringBuilder.ns` | buffer textual compatible con bootstrap |
| `selfhost/bootstrap/verify_self_hosted_generation.py` | auditoría y matriz generacional |
| `cmake/VerifyNativeSharpSelfHostedGeneration.cmake` | orquestación configurable y bibliotecas de importación |
| `selfhost/contracts/self-hosted-generation.contract.json` | contrato SH41 versionado |

## Gate dirigido SH41

La corrida dirigida `selfhost/out/tests-lld/selfhost-sh41` produjo este resumen:

| Perfil | SHA-256 objeto nsc2/nsc3 | nsc1→obj | nsc2→obj |
|---|---|---:|---:|
| O0 | `8c4d695e00ff8edfcaad07752a238bbd729987ed317ff57a6f0d65467fe4b27f` | 12.986 s | 10.647 s |
| O1 | `1675e571c937759b9d220360564c8803359cb75c408f7a83885a5162aec2075f` | 54.242 s | 43.005 s |
| O2 | `e0b5da15155d6d95c67c67d06af2e0b87399a68b7bb05c4c45fc36f8995c3647` | 61.897 s | 51.512 s |
| O3 | `8c1d6f9179ace3b8c649a3de0c2d34150d92bc16268b51f42e32a47d24933f04` | 72.296 s | 61.620 s |

- 101 fuentes/proyectos inventariados;
- digest canónico de fuente
  `90299cbc1899b23baf659900d8cccdd215245b23c606ab7d3fce1948f11ab9d0`;
- 0 invocaciones de `nsc0` durante generación;
- 4/4 perfiles con punto fijo de objeto;
- 16/16 imágenes ejecutadas con LLD y NSLink;
- 0 problemas en el reporte.

## Baseline acumulado

Corrida certificada:
`selfhost/evidence/runs/tanda-43-sh41-autocompilacion-r3-20260920/report.md`.

| Componente | Resultado | Tiempo |
|---|---:|---:|
| construcción e instalación `A:\native-sharp` | aprobada | 5.161 s |
| CTest LLD | 476/476 | 1134.187 s |
| fixtures LLD | 148/148 | 50.630 s |
| triage LLD | 18/18 | 8.179 s |
| CTest NSLink | 476/476 | 1090.929 s |
| fixtures NSLink | 148/148 | 46.416 s |
| triage NSLink | 18/18 | 7.942 s |
| certificación Gate 0 | aprobada | 8.944 s |
| total | 0 fallos | 2390.086 s |

La instalación final reporta
`nsc 2.0.0+build.19700101T000000Z.4300000000000002` y
`--verify-installation A:\native-sharp` termina correctamente. Los logs y productos de la corrida
permanecen ignorados; el commit conserva el Gate 0 certificado y el resumen compacto de SH41.

## Criterios de cierre

- [x] `nsc1` genera el objeto de `nsc2` exclusivamente desde fuentes NativeSharp.
- [x] la auditoría detectaría una invocación accidental de la semilla.
- [x] `nsc2` genera `nsc3` y alcanza identidad de objeto O0–O3.
- [x] LLD y NSLink producen imágenes ejecutables de ambas generaciones.
- [x] no hay image replay ni artefactos binarios ocultos en el commit.
- [x] baseline completo acumulado verde.
