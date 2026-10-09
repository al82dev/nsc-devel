# Library.NativeSharp.Core.IO — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `93094d49210999de5018765df1c7551e942f43f4`.
- **Universo:** 5 archivos, 759 líneas, 63 declaraciones/artefactos.
- **Disposición:** INDIRECTA=5.
- **Diseño futuro:** 5 contratos y 30 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Exponer archivos, directorios, paths y escritores de texto sobre la frontera runtime Win32.

**Dependencias:** NativeSharp.Core;runtime Win32.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000395 | `lib/NativeSharp.Core.IO/Directory.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-CIO-001` | Superficie de biblioteca Directory |
| PF-000396 | `lib/NativeSharp.Core.IO/File.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-CIO-002` | Superficie de biblioteca File |
| PF-000397 | `lib/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj` | `LIBRARY_PROJECT` | `INDIRECTA` | `C12B-CIO-003` | Proyecto distribuido Native Sharp.Core.IO |
| PF-000398 | `lib/NativeSharp.Core.IO/Path.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-CIO-004` | Superficie de biblioteca Path |
| PF-000399 | `lib/NativeSharp.Core.IO/TextWriter.ns` | `LIBRARY_ALGORITHM` | `INDIRECTA` | `C12B-CIO-005` | Superficie de biblioteca Text Writer |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-CIO-001` | Superficie de biblioteca Directory | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-CIO-002` | Superficie de biblioteca File | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-CIO-003` | Proyecto distribuido Native Sharp.Core.IO | `integration` | fuentes, referencias y metadata de biblioteca | grafo de proyecto, unidades, referencias y artefactos publicados |
| `C12B-CIO-004` | Superficie de biblioteca Path | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |
| `C12B-CIO-005` | Superficie de biblioteca Text Writer | `integration` | valores públicos, estado de colección/memoria o recursos Win32 | API observable, contenido, estado, diagnóstico y liberación de recursos |

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