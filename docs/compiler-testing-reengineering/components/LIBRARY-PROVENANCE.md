# Library.Provenance — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `93094d49210999de5018765df1c7551e942f43f4`.
- **Universo:** 1 archivos, 1 líneas, 1 declaraciones/artefactos.
- **Disposición:** INDIRECTA=1.
- **Diseño futuro:** 1 contratos y 6 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Registrar la procedencia declarada del árbol de bibliotecas distribuido.

**Dependencias:** árbol lib congelado.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000359 | `lib/.source.txt` | `PROVENANCE` | `INDIRECTA` | `C12B-PRV-001` | Procedencia del árbol lib |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-PRV-001` | Procedencia del árbol lib | `packaging` | marcador de fuente distribuida | texto exacto y presencia en manifiesto de distribución |

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