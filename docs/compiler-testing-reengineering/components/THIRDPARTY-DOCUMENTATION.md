# ThirdParty.Documentation — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 1 archivos, 14 líneas, 1 declaraciones/artefactos.
- **Disposición:** UPSTREAM=1.
- **Diseño futuro:** 0 contratos y 0 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Fijar procedencia, versión, hash y licencia de las dependencias vendorizadas.

**Dependencias:** archives upstream zlib/zstd.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000865 | `third_party/README.md` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream README; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| — | No se deriva contrato directo de NSC; la disposición por archivo es el cierre. | — | — | — |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 0.
- Casos NIGHTLY/weekly: 0.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva la razón completa de exclusión/upstream y su frontera de integración; no afirma reconstructibilidad interna del código ajeno.