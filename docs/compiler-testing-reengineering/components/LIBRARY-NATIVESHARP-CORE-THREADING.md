# Library.NativeSharp.Core.Threading — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `93094d49210999de5018765df1c7551e942f43f4`.
- **Universo:** 12 archivos, 2124 líneas, 186 declaraciones/artefactos.
- **Disposición:** EXCLUIDA=1, INDIRECTA=11.
- **Diseño futuro:** 11 contratos y 66 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Exponer atomics, sincronización, hilos, jobs y pool con ownership y estados concurrentes explícitos.

**Dependencias:** NativeSharp.Core;runtime Win32;modelo de memoria.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000400 | `lib/NativeSharp.Core.Threading/Atomics.ns` | `INTRINSIC_LOWERING` | `INDIRECTA` | `C12B-THR-001` | Superficie intrínseca Atomics |
| PF-000401 | `lib/NativeSharp.Core.Threading/Event.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-THR-002` | Superficie de biblioteca Event |
| PF-000402 | `lib/NativeSharp.Core.Threading/Job.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-THR-003` | Superficie de biblioteca Job |
| PF-000403 | `lib/NativeSharp.Core.Threading/LANGUAGE-3-MIGRATION.md` | `LIBRARY_DOCUMENT` | `EXCLUIDA` | `` | Documento histórico o de migración LANGUAGE 3 MIGRATION; aporta procedencia/contradicciones, no un contrato ejecutable del compilador. |
| PF-000404 | `lib/NativeSharp.Core.Threading/Monitor.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-THR-004` | Superficie de biblioteca Monitor |
| PF-000405 | `lib/NativeSharp.Core.Threading/Mutex.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-THR-005` | Superficie de biblioteca Mutex |
| PF-000406 | `lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj` | `LIBRARY_PROJECT` | `INDIRECTA` | `C12B-THR-006` | Proyecto distribuido Native Sharp.Core.Threading |
| PF-000407 | `lib/NativeSharp.Core.Threading/ReferenceCounting.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-THR-007` | Superficie de biblioteca Reference Counting |
| PF-000408 | `lib/NativeSharp.Core.Threading/ScopedLock.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-THR-008` | Superficie de biblioteca Scoped Lock |
| PF-000409 | `lib/NativeSharp.Core.Threading/Semaphore.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-THR-009` | Superficie de biblioteca Semaphore |
| PF-000410 | `lib/NativeSharp.Core.Threading/Thread.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-THR-010` | Superficie de biblioteca Thread |
| PF-000411 | `lib/NativeSharp.Core.Threading/ThreadPool.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-THR-011` | Superficie de biblioteca Thread Pool |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-THR-001` | Superficie intrínseca Atomics | `integration` | valores escalares/vectoriales y operación pública | IR/objeto y resultado ejecutable para vectores y bordes conocidos |
| `C12B-THR-002` | Superficie de biblioteca Event | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-THR-003` | Superficie de biblioteca Job | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-THR-004` | Superficie de biblioteca Monitor | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-THR-005` | Superficie de biblioteca Mutex | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-THR-006` | Proyecto distribuido Native Sharp.Core.Threading | `integration` | fuentes, referencias y metadata de biblioteca | grafo de proyecto, unidades, referencias y artefactos publicados |
| `C12B-THR-007` | Superficie de biblioteca Reference Counting | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-THR-008` | Superficie de biblioteca Scoped Lock | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-THR-009` | Superficie de biblioteca Semaphore | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-THR-010` | Superficie de biblioteca Thread | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-THR-011` | Superficie de biblioteca Thread Pool | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 55.
- Casos NIGHTLY/weekly: 11.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.