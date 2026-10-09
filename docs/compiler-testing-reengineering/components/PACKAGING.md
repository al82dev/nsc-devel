# Packaging — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 3 archivos, 561 líneas, 32 declaraciones/artefactos.
- **Disposición:** DIRECTA=2, INDIRECTA=1.
- **Diseño futuro:** 3 contratos y 18 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Instalar y empaquetar una distribución NativeSharp identificada, verificable y sin sobrescrituras implícitas.

**Dependencias:** CMake install;7-Zip;nsc --version;filesystem.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000077 | `deploy.bat` | `PACKAGING` | `INDIRECTA` | `C12B-PKG-001` | Instalación o empaquetado deploy |
| PF-000078 | `deploy.py` | `PACKAGING` | `DIRECTA` | `C12B-PKG-002` | Instalación o empaquetado deploy |
| PF-000468 | `package_native_sharp.py` | `PACKAGING` | `DIRECTA` | `C12B-PKG-003` | Instalación o empaquetado package native sharp |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-PKG-001` | Instalación o empaquetado deploy | `packaging` | build/prefix o instalación y herramienta de archivo | comando, identidad, árbol instalado/archive y código de salida |
| `C12B-PKG-002` | Instalación o empaquetado deploy | `packaging` | build/prefix o instalación y herramienta de archivo | comando, identidad, árbol instalado/archive y código de salida |
| `C12B-PKG-003` | Instalación o empaquetado package native sharp | `packaging` | build/prefix o instalación y herramienta de archivo | comando, identidad, árbol instalado/archive y código de salida |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 15.
- Casos NIGHTLY/weekly: 3.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.