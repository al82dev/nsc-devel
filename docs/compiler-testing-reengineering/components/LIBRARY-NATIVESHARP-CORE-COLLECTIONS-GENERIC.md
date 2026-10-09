# Library.NativeSharp.Core.Collections.Generic — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 30 archivos, 4485 líneas, 330 declaraciones/artefactos.
- **Disposición:** EXCLUIDA=4, INDIRECTA=26.
- **Diseño futuro:** 26 contratos y 156 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Proveer algoritmos, comparadores y colecciones genéricas con ownership y enumeración observables.

**Dependencias:** NativeSharp.Core;genéricos y metadata NSMETA.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000360 | `lib/NativeSharp.Core.Collections.Generic/Algorithm.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-001` | Superficie de biblioteca Algorithm |
| PF-000361 | `lib/NativeSharp.Core.Collections.Generic/Comparer.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-002` | Superficie de biblioteca Comparer |
| PF-000362 | `lib/NativeSharp.Core.Collections.Generic/Delegates.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-003` | Superficie de biblioteca Delegates |
| PF-000363 | `lib/NativeSharp.Core.Collections.Generic/DICTIONARY-INDEXER-OFFICIAL-NSC.md` | `LIBRARY_DOCUMENT` | `EXCLUIDA` | `` | Documento histórico o de migración DICTIONARY INDEXER OFFICIAL NSC; aporta procedencia/contradicciones, no un contrato ejecutable del compilador. |
| PF-000364 | `lib/NativeSharp.Core.Collections.Generic/Dictionary.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-004` | Superficie de biblioteca Dictionary |
| PF-000365 | `lib/NativeSharp.Core.Collections.Generic/DictionaryEnumerator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-005` | Superficie de biblioteca Dictionary Enumerator |
| PF-000366 | `lib/NativeSharp.Core.Collections.Generic/Hashing.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-006` | Superficie de biblioteca Hashing |
| PF-000367 | `lib/NativeSharp.Core.Collections.Generic/HashSet.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-007` | Superficie de biblioteca Hash Set |
| PF-000368 | `lib/NativeSharp.Core.Collections.Generic/HashSetEnumerator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-008` | Superficie de biblioteca Hash Set Enumerator |
| PF-000369 | `lib/NativeSharp.Core.Collections.Generic/KeyValuePair.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-009` | Superficie de biblioteca Key Value Pair |
| PF-000370 | `lib/NativeSharp.Core.Collections.Generic/LANGUAGE-3-MIGRATION.md` | `LIBRARY_DOCUMENT` | `EXCLUIDA` | `` | Documento histórico o de migración LANGUAGE 3 MIGRATION; aporta procedencia/contradicciones, no un contrato ejecutable del compilador. |
| PF-000371 | `lib/NativeSharp.Core.Collections.Generic/LinkedList.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-010` | Superficie de biblioteca Linked List |
| PF-000372 | `lib/NativeSharp.Core.Collections.Generic/LinkedListEnumerator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-011` | Superficie de biblioteca Linked List Enumerator |
| PF-000373 | `lib/NativeSharp.Core.Collections.Generic/LinkedListNode.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-012` | Superficie de biblioteca Linked List Node |
| PF-000374 | `lib/NativeSharp.Core.Collections.Generic/List.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-013` | Superficie de biblioteca List |
| PF-000375 | `lib/NativeSharp.Core.Collections.Generic/ListEnumerator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-014` | Superficie de biblioteca List Enumerator |
| PF-000376 | `lib/NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj` | `LIBRARY_PROJECT` | `INDIRECTA` | `C12B-COL-015` | Proyecto distribuido Native Sharp.Core.Collections.Generic |
| PF-000377 | `lib/NativeSharp.Core.Collections.Generic/OwnedStringCollections.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-016` | Superficie de biblioteca Owned String Collections |
| PF-000378 | `lib/NativeSharp.Core.Collections.Generic/PriorityQueue.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-017` | Superficie de biblioteca Priority Queue |
| PF-000379 | `lib/NativeSharp.Core.Collections.Generic/PriorityQueueEnumerator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-018` | Superficie de biblioteca Priority Queue Enumerator |
| PF-000380 | `lib/NativeSharp.Core.Collections.Generic/Queue.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-019` | Superficie de biblioteca Queue |
| PF-000381 | `lib/NativeSharp.Core.Collections.Generic/QueueEnumerator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-020` | Superficie de biblioteca Queue Enumerator |
| PF-000382 | `lib/NativeSharp.Core.Collections.Generic/SORTED-DICTIONARY-INDEXER-OFFICIAL-NSC.md` | `LIBRARY_DOCUMENT` | `EXCLUIDA` | `` | Documento histórico o de migración SORTED DICTIONARY INDEXER OFFICIAL NSC; aporta procedencia/contradicciones, no un contrato ejecutable del compilador. |
| PF-000383 | `lib/NativeSharp.Core.Collections.Generic/SORTED-LIST-INDEXER-OFFICIAL-NSC.md` | `LIBRARY_DOCUMENT` | `EXCLUIDA` | `` | Documento histórico o de migración SORTED LIST INDEXER OFFICIAL NSC; aporta procedencia/contradicciones, no un contrato ejecutable del compilador. |
| PF-000384 | `lib/NativeSharp.Core.Collections.Generic/SortedDictionary.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-021` | Superficie de biblioteca Sorted Dictionary |
| PF-000385 | `lib/NativeSharp.Core.Collections.Generic/SortedList.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-022` | Superficie de biblioteca Sorted List |
| PF-000386 | `lib/NativeSharp.Core.Collections.Generic/SortedSet.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-023` | Superficie de biblioteca Sorted Set |
| PF-000387 | `lib/NativeSharp.Core.Collections.Generic/SortedSetEnumerator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-024` | Superficie de biblioteca Sorted Set Enumerator |
| PF-000388 | `lib/NativeSharp.Core.Collections.Generic/Stack.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-025` | Superficie de biblioteca Stack |
| PF-000389 | `lib/NativeSharp.Core.Collections.Generic/StackEnumerator.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-COL-026` | Superficie de biblioteca Stack Enumerator |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-COL-001` | Superficie de biblioteca Algorithm | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-002` | Superficie de biblioteca Comparer | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-003` | Superficie de biblioteca Delegates | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-004` | Superficie de biblioteca Dictionary | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-005` | Superficie de biblioteca Dictionary Enumerator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-006` | Superficie de biblioteca Hashing | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-007` | Superficie de biblioteca Hash Set | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-008` | Superficie de biblioteca Hash Set Enumerator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-009` | Superficie de biblioteca Key Value Pair | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-010` | Superficie de biblioteca Linked List | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-011` | Superficie de biblioteca Linked List Enumerator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-012` | Superficie de biblioteca Linked List Node | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-013` | Superficie de biblioteca List | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-014` | Superficie de biblioteca List Enumerator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-015` | Proyecto distribuido Native Sharp.Core.Collections.Generic | `integration` | fuentes, referencias y metadata de biblioteca | grafo de proyecto, unidades, referencias y artefactos publicados |
| `C12B-COL-016` | Superficie de biblioteca Owned String Collections | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-017` | Superficie de biblioteca Priority Queue | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-018` | Superficie de biblioteca Priority Queue Enumerator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-019` | Superficie de biblioteca Queue | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-020` | Superficie de biblioteca Queue Enumerator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-021` | Superficie de biblioteca Sorted Dictionary | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-022` | Superficie de biblioteca Sorted List | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-023` | Superficie de biblioteca Sorted Set | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-024` | Superficie de biblioteca Sorted Set Enumerator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-025` | Superficie de biblioteca Stack | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-COL-026` | Superficie de biblioteca Stack Enumerator | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 130.
- Casos NIGHTLY/weekly: 26.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.