# Library.NativeSharp.Win32.Compiler — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 4 archivos, 224 líneas, 32 declaraciones/artefactos.
- **Disposición:** INDIRECTA=4.
- **Diseño futuro:** 4 contratos y 24 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Adaptar filesystem, procesos, archivos mapeados y response files requeridos por el compilador self-host.

**Dependencias:** NativeSharp.Core;Core.IO;Core.Diagnostics;kernel32.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000464 | `lib/NativeSharp.Win32.Compiler/CompilerMappedFile.ns` | `NATIVE_ABI` | `INDIRECTA` | `C12B-W32-001` | Frontera ABI Compiler Mapped File |
| PF-000465 | `lib/NativeSharp.Win32.Compiler/CompilerServices.ns` | `NATIVE_ABI` | `INDIRECTA` | `C12B-W32-002` | Frontera ABI Compiler Services |
| PF-000466 | `lib/NativeSharp.Win32.Compiler/LinkerAdapters.ns` | `NATIVE_ABI` | `INDIRECTA` | `C12B-W32-003` | Frontera ABI Linker Adapters |
| PF-000467 | `lib/NativeSharp.Win32.Compiler/NativeSharp.Win32.Compiler.nsproj` | `LIBRARY_PROJECT` | `INDIRECTA` | `C12B-W32-004` | Proyecto distribuido Native Sharp.Win32.Compiler |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-W32-001` | Frontera ABI Compiler Mapped File | `integration` | firmas, enums, handles, paths o response files | firma física, símbolos/imports, argumentos, recursos y código de salida |
| `C12B-W32-002` | Frontera ABI Compiler Services | `integration` | firmas, enums, handles, paths o response files | firma física, símbolos/imports, argumentos, recursos y código de salida |
| `C12B-W32-003` | Frontera ABI Linker Adapters | `integration` | firmas, enums, handles, paths o response files | firma física, símbolos/imports, argumentos, recursos y código de salida |
| `C12B-W32-004` | Proyecto distribuido Native Sharp.Win32.Compiler | `integration` | fuentes, referencias y metadata de biblioteca | grafo de proyecto, unidades, referencias y artefactos publicados |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 20.
- Casos NIGHTLY/weekly: 4.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.