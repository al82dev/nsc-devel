# ThirdParty.zstd — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 66 archivos, 45769 líneas, 66 declaraciones/artefactos.
- **Disposición:** UPSTREAM=66.
- **Diseño futuro:** 0 contratos y 0 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Aportar zstd 1.5.7 vendorizado para el enlace estático de componentes LLVM.

**Dependencias:** upstream zstd 1.5.7.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000894 | `third_party/zstd/CHANGELOG` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream CHANGELOG; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000895 | `third_party/zstd/COPYING` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream COPYING; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000896 | `third_party/zstd/lib/common/allocations.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream allocations; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000897 | `third_party/zstd/lib/common/bits.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream bits; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000898 | `third_party/zstd/lib/common/bitstream.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream bitstream; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000899 | `third_party/zstd/lib/common/compiler.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream compiler; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000900 | `third_party/zstd/lib/common/cpu.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream cpu; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000901 | `third_party/zstd/lib/common/debug.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream debug; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000902 | `third_party/zstd/lib/common/debug.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream debug; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000903 | `third_party/zstd/lib/common/entropy_common.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream entropy common; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000904 | `third_party/zstd/lib/common/error_private.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream error private; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000905 | `third_party/zstd/lib/common/error_private.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream error private; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000906 | `third_party/zstd/lib/common/fse_decompress.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream fse decompress; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000907 | `third_party/zstd/lib/common/fse.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream fse; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000908 | `third_party/zstd/lib/common/huf.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream huf; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000909 | `third_party/zstd/lib/common/mem.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream mem; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000910 | `third_party/zstd/lib/common/pool.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream pool; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000911 | `third_party/zstd/lib/common/pool.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream pool; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000912 | `third_party/zstd/lib/common/portability_macros.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream portability macros; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000913 | `third_party/zstd/lib/common/threading.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream threading; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000914 | `third_party/zstd/lib/common/threading.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream threading; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000915 | `third_party/zstd/lib/common/xxhash.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream xxhash; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000916 | `third_party/zstd/lib/common/xxhash.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream xxhash; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000917 | `third_party/zstd/lib/common/zstd_common.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd common; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000918 | `third_party/zstd/lib/common/zstd_deps.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd deps; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000919 | `third_party/zstd/lib/common/zstd_internal.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd internal; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000920 | `third_party/zstd/lib/common/zstd_trace.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd trace; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000921 | `third_party/zstd/lib/compress/clevels.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream clevels; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000922 | `third_party/zstd/lib/compress/fse_compress.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream fse compress; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000923 | `third_party/zstd/lib/compress/hist.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream hist; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000924 | `third_party/zstd/lib/compress/hist.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream hist; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000925 | `third_party/zstd/lib/compress/huf_compress.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream huf compress; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000926 | `third_party/zstd/lib/compress/zstd_compress_internal.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd compress internal; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000927 | `third_party/zstd/lib/compress/zstd_compress_literals.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd compress literals; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000928 | `third_party/zstd/lib/compress/zstd_compress_literals.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd compress literals; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000929 | `third_party/zstd/lib/compress/zstd_compress_sequences.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd compress sequences; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000930 | `third_party/zstd/lib/compress/zstd_compress_sequences.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd compress sequences; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000931 | `third_party/zstd/lib/compress/zstd_compress_superblock.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd compress superblock; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000932 | `third_party/zstd/lib/compress/zstd_compress_superblock.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd compress superblock; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000933 | `third_party/zstd/lib/compress/zstd_compress.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd compress; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000934 | `third_party/zstd/lib/compress/zstd_cwksp.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd cwksp; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000935 | `third_party/zstd/lib/compress/zstd_double_fast.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd double fast; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000936 | `third_party/zstd/lib/compress/zstd_double_fast.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd double fast; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000937 | `third_party/zstd/lib/compress/zstd_fast.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd fast; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000938 | `third_party/zstd/lib/compress/zstd_fast.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd fast; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000939 | `third_party/zstd/lib/compress/zstd_lazy.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd lazy; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000940 | `third_party/zstd/lib/compress/zstd_lazy.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd lazy; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000941 | `third_party/zstd/lib/compress/zstd_ldm_geartab.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd ldm geartab; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000942 | `third_party/zstd/lib/compress/zstd_ldm.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd ldm; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000943 | `third_party/zstd/lib/compress/zstd_ldm.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd ldm; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000944 | `third_party/zstd/lib/compress/zstd_opt.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd opt; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000945 | `third_party/zstd/lib/compress/zstd_opt.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd opt; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000946 | `third_party/zstd/lib/compress/zstd_preSplit.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd pre Split; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000947 | `third_party/zstd/lib/compress/zstd_preSplit.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd pre Split; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000948 | `third_party/zstd/lib/decompress/huf_decompress.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream huf decompress; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000949 | `third_party/zstd/lib/decompress/zstd_ddict.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd ddict; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000950 | `third_party/zstd/lib/decompress/zstd_ddict.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd ddict; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000951 | `third_party/zstd/lib/decompress/zstd_decompress_block.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd decompress block; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000952 | `third_party/zstd/lib/decompress/zstd_decompress_block.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd decompress block; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000953 | `third_party/zstd/lib/decompress/zstd_decompress_internal.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd decompress internal; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000954 | `third_party/zstd/lib/decompress/zstd_decompress.c` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd decompress; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000955 | `third_party/zstd/lib/zstd_errors.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd errors; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000956 | `third_party/zstd/lib/zstd.h` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream zstd; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000957 | `third_party/zstd/LICENSE` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream LICENSE; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000958 | `third_party/zstd/README.md` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream README; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |
| PF-000959 | `third_party/zstd/SECURITY.md` | `UPSTREAM_UNIT` | `UPSTREAM` | `` | Unidad vendorizada upstream SECURITY; se conserva para el build estático y se valida por procedencia más integración, no por reescritura de sus tests internos. |

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