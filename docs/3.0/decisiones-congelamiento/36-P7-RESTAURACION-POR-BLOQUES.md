# Congelamiento 3.0 P7 / DEC-16 — restauración por bloques de los archivos históricos

Fecha: 2026-10-26. Revisión de partida: `5cc4e0f9f`.

## Nota de trabajo

```text
HEAD: 5cc4e0f9f
Elemento: P7 (elementos 109-112 del plan de 142)
Contrato vigente: DEC-16 / P7: conservar sólo los bloques exigidos por contratos aprobados (C-01, C-05, C-08,
  C-11, C-13/P-1, C-14 y los posteriores con relación causal demostrada) y restaurar byte por byte el resto,
  bloque por bloque
Documento histórico que podría contradecirlo: ninguno vigente
Archivos exactos que se modificarán: los 34 archivos de la tabla «Bloques restaurados»
```

## Decisión aplicada

P7, opción 1, en palabras de Alberto: conserva únicamente los bloques exigidos por contratos aprobados y restaura
byte por byte todos los demás bloques, bloque por bloque, no por archivo completo. Validación: compilar sólo los
proyectos restaurados o modificados con `--check-project-oop-mir`. La matriz integral queda para su gate final:
P7 queda implementado y validado focalmente, no certificado.

## Inventario

`git diff 63aba0be4 HEAD -- tests book_examples examples selfhost` (`HEAD` = `605114b3b`), 246 archivos:

| Tipo | Archivos | Bloques (hunks) | Tratamiento |
|---|---|---|---|
| `.ns` de proyectos (1) | 101 | — | herramienta por bloques, oráculo de proyecto |
| `.ns` sueltos (`tests/cc1`, `tests/integration`) | 27 | — | herramienta por bloques, oráculo de archivo |
| `tests/cases` (`.ns` y manifiestos `.json`) | 49 | — | herramienta por bloques, oráculo: los casos del directorio |
| C++ y otros (`tests/unit`, `tests/compiler-suite`, README) | 68 | 269 | revisión manual de cada bloque |
| `tests/unit/Freeze30ContractTests.cpp` | 1 (nuevo) | — | archivo de la campaña, no histórico |

(1) `book_examples`, `examples`, `selfhost`, `tests/bug_triage` y `tests/interop_contract`.

Total: 591 hunks. Los commits que los tocan están en `A:\temp\nsc-dec\p7\filecommits.txt`; 120 archivos sólo
los tocó `ea0da601c` (C-01/C-02), 51 sólo `73b03a394` (P-1/P-3) y 36 sólo `2e154f127` (C-05).

## Método

Herramientas en `A:\temp\nsc-dec\p7`: `nsrev.py` (proyectos y archivos) y `casrev.py` (`tests/cases`).

1. **Bloques.** Diferencia por líneas entre el blob de la base y el contenido actual, ignorando el fin de línea.
   Cada bloque distinto se revierte a la base sobre los ya revertidos. Si el bloque no se puede revertir entero
   y es partible (mismo número de líneas, o inserción o eliminación pura), se intenta línea por línea.
2. **Oráculo.** Un bloque queda revertido sólo si el destino da exactamente lo mismo que con el contenido actual:
   - proyecto: resultado y diagnósticos de `--check-project-oop-mir`; si es ejecutable, también exit y stdout
     del `.exe` `-O0`; si el proyecto pasa el check pero no emite ejecutable, sólo el check;
   - archivo suelto: compilación `-O0`, diagnósticos, exit y stdout;
   - directorio de `tests/cases`: el resultado de cada manifiesto del directorio, ejecutado como lo arma el
     harness (fuentes, `working_files`, `{project}`, `{workspace}`).
3. **Guarda de contrato.** El check y la ejecución no ven una fuga ni una propiedad perdida. Por eso un bloque que
   contiene `raii`, `owned`, `borrowed`, `new`, `move`, `destroy`, `delete`, `throw`, `throws`, `try`, `catch`,
   `async` o `await` en su lado actual se conserva aunque el oráculo no lo distinga. Excepción: un bloque cuyo
   lado actual no tiene esas palabras y cuyo lado de la base sólo trae `delete`/`delete[]` (DEC-16 con DEC-06);
   el compilador sigue rechazando un `delete` que los contratos prohíben.
4. **JSON.** Un manifiesto se restaura al texto de la base y sólo conservan el valor actual los campos cuya forma
   de la base hace fallar un caso. Así vuelve el formato compacto de la base y quedan sólo las expectativas que
   el contrato cambió.
5. **Fin de línea.** El blob de git es LF; el árbol de trabajo (`core.autocrlf=true`) se escribe con el fin de
   línea que tenía el archivo.

## Resultado

### Bloques restaurados

| Archivo | Qué volvió de la base |
|---|---|
| `book_examples/01_rational_numbers/Program.ns` | línea en blanco |
| `book_examples/09_doubly_linked_list/DoublyLinkedList.ns` | `before.Next = node;` |
| `book_examples/10_circular_linked_list_josephus/CircularList.ns` | `tail.Next = node;` |
| `examples/project_stage36_recovery/App/InterfacesAndDispatch.ns` | línea en blanco |
| `integration/valid/full_frontend.ns` y 5 copias en `tests/cases` | los dos `delete[] heap;` y `return result;` |
| `integration/valid/llvm_features.ns` y 2 copias | línea inicial, `delete[] heap;`, `return temporary[0];` |
| `tests/integration/valid/hir_features.ns` y su copia en `tests/cases` | `delete[] heap;` y `delete null;` |
| `tests/integration/valid/memory_types.ns` y su copia en `tests/cases` | `delete[] heap;` |
| `tests/integration/valid/mir_features.ns` y su copia en `tests/cases` | línea en blanco final |
| `tests/integration/valid/pass_features.ns` y su copia en `tests/cases` | sangría original y líneas finales |
| 12 manifiestos `.json` de `tests/cases` | formato compacto de la base; valores de la base donde el caso pasa |
| `tests/cases/backend/nsc.dump-llvm-phi/case.json` | archivo completo (`"phi i32"` pasa) |

Los `delete[]` que vuelven son los que DEC-16 manda restaurar (P-3 los había quitado; DEC-06 los hace válidos).

### Bloques conservados

| Motivo | Bloques (contando las partes de un bloque partido) |
|---|---|
| guarda de contrato (propiedad, excepciones, await) | 454 |
| el oráculo cambia al revertirlos (el check falla o cambia la ejecución) | 272 |

Los C++ de `tests/unit` y `tests/compiler-suite` se conservan completos. Cada bloque es del mismo tipo:
- `raii`/`owned`/`borrowed` en fuentes embebidas (C-01);
- `throws`, `try`/`catch` y `throw move(...)` (C-13/P-1);
- oráculos de C-05 (proyecto implícito, acciones de inspección), C-06 (manifiesto de superficie), DEC-02, DEC-15,
  C-17, DiagnosticId (`88982f3ab`), C-08 (`IAllocator`, P6) y Object ABI 3 (P8);
- fuentes nuevas en `tests/compiler-suite/CMakeLists.txt`.

Hay dos conservaciones sin contrato propio, por la verdad del documento o de la prueba:
- `tests/interop_contract/README.md`: la nota de que `c1_14` sale 14 y no 141;
- `BasicArtifactCommitModule.cpp` y la fila `TB-BAS-089` de `cases.jsonl` (`837325b27`): prueban la corrección
  de `-o nombre.exe` sin directorio. Quitarlas deja esa corrección sin prueba.

### Excepciones manuales

- `tests/interop_contract/c1_14_callback_exception_boundary/Main.ns`: la herramienta dejó partido el comentario
  «UPDATE (Freeze 3.0, checked throw)», porque sus líneas con `throw` se conservaron y las demás no. Se devolvió
  el archivo completo a `HEAD`: el comentario explica que el caso sale 14.
- `tests/cases/runtime-linker/nsc.invalid.llvm-reserved-runtime` y
  `tests/cases/diagnostics/nsc.invalid.unresolved-link`:
  quedan como en `HEAD`. Con la base ambos casos pasan, pero por otra razón: C-05 rechaza el `extern C` global, y
  el caso deja de probar la colisión con el símbolo reservado o el enlace no resuelto.

### Destinos no validables (quedan como en `HEAD`)

| Destino | Motivo |
|---|---|
| `25_drozdek_advanced_suite`, `project_prestage56_utilidades`, `CompilerHostWorkload` | falta la biblioteca (2) |
| `selfhost/tests/low-level-2.0` (proyecto raíz) | falla en `HEAD`: `stackalloc` de un `Cell` con destructor (C-11) |
| `tests/integration/debug_pdb_extreme/Producer.ns` | no compila suelto y la herramienta no resolvió su proyecto |
| `tests/cases/ir/stage35-recovery` | su único caso falla en `HEAD` (hallazgo 2) |

(2) `lib/NativeSharp.Core.IO/NativeSharpCore.lib`, precompilada, no existe en el árbol.

Los negativos (`project_foreach_negative_test`, `b9_enum_bitwise/MixedRejected`,
`integration/invalid/llvm_reserved_runtime.ns`)
se procesaron con el oráculo «mismos diagnósticos». Ningún bloque cambió.

## Validación

nsc Release de `5cc4e0f9f`. Compilación focal de lo restaurado (`A:\temp\nsc-dec\p7\finalchk.cmd`):

| Destino | Resultado |
|---|---|
| `Book01RationalNumbers`, `Book09DoublyLinkedList`, `Book10CircularJosephus`, `Stage36RecoveryApp` | MIR check ok |
| 17 `.ns` sueltos de `tests/integration/valid` y `tests/cases` (`--check`) | `project OOP LLVM verification: ok` |

La herramienta además comparó, bloque por bloque, el exit y el stdout de cada ejecutable y el resultado de cada
manifiesto de los directorios tocados. Bitácoras: `A:\temp\nsc-dec\p7\logF*.jsonl`.

## Hallazgos (no corregidos aquí)

1. Los ocho `object.case.json` de `tests/cases/backend/nsc.stage3[4-9]*`, `stage42` y `stage44` fallan en `HEAD`.
   Esperan `project OOP object emission: ok.*machine=0x8664` y `--emit-project-oop-obj` imprime `Build succeeded.`.
2. `tests/cases/ir/stage35-recovery/mir.case.json` falla en `HEAD`: el dump no contiene
   `destruction=dynamic-descriptor`.
3. `selfhost/tests/low-level-2.0` no compila en `HEAD` (ver arriba).

No verifiqué si 1 y 2 son previos a la campaña: el auditor de casos no corría los manifiestos de proyecto.
Quedan para el gate de la matriz.

## No verificado

- Unitarias (`tests/unit`, `nsc_component_tests`) y la matriz de 349 proyectos y 52 casos: gate final de Alberto.
