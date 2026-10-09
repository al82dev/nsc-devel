# SDK — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 1 archivos, 95 líneas, 79 declaraciones/artefactos.
- **Disposición:** INDIRECTA=1.
- **Diseño futuro:** 1 contratos y 6 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Declarar el cierre exacto de imports Win64 requerido por NativeSharp.Runtime.Win32.

**Dependencias:** KERNEL32;KERNELBASE;SHELL32;ucrtbase.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000469 | `sdk/win64/nativesharp-runtime.nsimports` | `RUNTIME_IMPORTS` | `INDIRECTA` | `C12B-SDK-001` | Cierre de imports del runtime Win64 |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-SDK-001` | Cierre de imports del runtime Win64 | `integration` | archivo nsimports y ABI de runtime | conjunto exacto DLL:símbolo y smoke de enlace/ejecución |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 5.
- Casos NIGHTLY/weekly: 1.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.