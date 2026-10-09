# Library.NativeSharp.LLVM.C — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 4 archivos, 343 líneas, 144 declaraciones/artefactos.
- **Disposición:** INDIRECTA=4.
- **Diseño futuro:** 4 contratos y 24 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Congelar la frontera NativeSharp hacia la API C estática de LLVM 23 y sus enums ABI.

**Dependencias:** LLVM 23 C API;NativeSharp.Core.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000460 | `lib/NativeSharp.LLVM.C/LLVMApi.ns` | `NATIVE_ABI` | `INDIRECTA` | `C12B-LVC-001` | Frontera ABI LLVMApi |
| PF-000461 | `lib/NativeSharp.LLVM.C/LLVMEnums.ns` | `NATIVE_ABI` | `INDIRECTA` | `C12B-LVC-002` | Frontera ABI LLVMEnums |
| PF-000462 | `lib/NativeSharp.LLVM.C/LLVMVersionPolicy.ns` | `NATIVE_ABI` | `INDIRECTA` | `C12B-LVC-003` | Frontera ABI LLVMVersion Policy |
| PF-000463 | `lib/NativeSharp.LLVM.C/NativeSharp.LLVM.C.nsproj` | `LIBRARY_PROJECT` | `INDIRECTA` | `C12B-LVC-004` | Proyecto distribuido Native Sharp.LLVM.C |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-LVC-001` | Frontera ABI LLVMApi | `integration` | firmas, enums, handles, paths o response files | firma física, símbolos/imports, argumentos, recursos y código de salida |
| `C12B-LVC-002` | Frontera ABI LLVMEnums | `integration` | firmas, enums, handles, paths o response files | firma física, símbolos/imports, argumentos, recursos y código de salida |
| `C12B-LVC-003` | Frontera ABI LLVMVersion Policy | `integration` | firmas, enums, handles, paths o response files | firma física, símbolos/imports, argumentos, recursos y código de salida |
| `C12B-LVC-004` | Proyecto distribuido Native Sharp.LLVM.C | `integration` | fuentes, referencias y metadata de biblioteca | grafo de proyecto, unidades, referencias y artefactos publicados |

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