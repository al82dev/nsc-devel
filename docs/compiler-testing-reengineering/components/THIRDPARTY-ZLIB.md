# ThirdParty.zlib — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 28 archivos, 23903 líneas, 28 declaraciones/artefactos.
- **Disposición:** UPSTREAM=28.
- **Diseño futuro:** 0 contratos y 0 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Aportar zlib 1.3.2 vendorizado para el enlace estático de componentes LLVM.

**Dependencias:** upstream zlib 1.3.2.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000866 | `third_party/zlib/adler32.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream adler32; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000867 | `third_party/zlib/ChangeLog` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream Change Log; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000868 | `third_party/zlib/compress.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream compress; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000869 | `third_party/zlib/crc32.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream crc32; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000870 | `third_party/zlib/crc32.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream crc32; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000871 | `third_party/zlib/deflate.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream deflate; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000872 | `third_party/zlib/deflate.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream deflate; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000873 | `third_party/zlib/FAQ` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream FAQ; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000874 | `third_party/zlib/gzguts.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream gzguts; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000875 | `third_party/zlib/INDEX` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream INDEX; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000876 | `third_party/zlib/infback.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream infback; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000877 | `third_party/zlib/inffast.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream inffast; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000878 | `third_party/zlib/inffast.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream inffast; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000879 | `third_party/zlib/inffixed.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream inffixed; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000880 | `third_party/zlib/inflate.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream inflate; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000881 | `third_party/zlib/inflate.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream inflate; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000882 | `third_party/zlib/inftrees.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream inftrees; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000883 | `third_party/zlib/inftrees.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream inftrees; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000884 | `third_party/zlib/LICENSE` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream LICENSE; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000885 | `third_party/zlib/README` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream README; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000886 | `third_party/zlib/trees.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream trees; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000887 | `third_party/zlib/trees.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream trees; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000888 | `third_party/zlib/uncompr.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream uncompr; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000889 | `third_party/zlib/zconf.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zconf; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000890 | `third_party/zlib/zconf.h.in` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zconf.h; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000891 | `third_party/zlib/zlib.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zlib; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000892 | `third_party/zlib/zutil.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zutil; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000893 | `third_party/zlib/zutil.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zutil; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |

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