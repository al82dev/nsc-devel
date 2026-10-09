# Library.NativeSharp.Core.Diagnostics — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `93094d49210999de5018765df1c7551e942f43f4`.
- **Universo:** 5 archivos, 474 líneas, 48 declaraciones/artefactos.
- **Disposición:** INDIRECTA=5.
- **Diseño futuro:** 5 contratos y 30 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Exponer depuración, procesos, cronómetros e información del sistema sobre la frontera runtime Win32.

**Dependencias:** NativeSharp.Core;runtime Win32.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000390 | `lib/NativeSharp.Core.Diagnostics/Debug.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-DIA-001` | Superficie de biblioteca Debug |
| PF-000391 | `lib/NativeSharp.Core.Diagnostics/NativeSharp.Core.Diagnostics.nsproj` | `LIBRARY_PROJECT` | `INDIRECTA` | `C12B-DIA-002` | Proyecto distribuido Native Sharp.Core.Diagnostics |
| PF-000392 | `lib/NativeSharp.Core.Diagnostics/Process.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-DIA-003` | Superficie de biblioteca Process |
| PF-000393 | `lib/NativeSharp.Core.Diagnostics/Stopwatch.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-DIA-004` | Superficie de biblioteca Stopwatch |
| PF-000394 | `lib/NativeSharp.Core.Diagnostics/SystemInfo.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-DIA-005` | Superficie de biblioteca System Info |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-DIA-001` | Superficie de biblioteca Debug | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-DIA-002` | Proyecto distribuido Native Sharp.Core.Diagnostics | `integration` | fuentes, referencias y metadata de biblioteca | grafo de proyecto, unidades, referencias y artefactos publicados |
| `C12B-DIA-003` | Superficie de biblioteca Process | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-DIA-004` | Superficie de biblioteca Stopwatch | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-DIA-005` | Superficie de biblioteca System Info | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 25.
- Casos NIGHTLY/weekly: 5.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.