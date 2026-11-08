# T08-003: validación de objeto nativo inválido en sidecar

Antes de la corrección, un sidecar v3 con encabezado COFF alterado y SHA-256
recalculado se aceptaba y fallaba más tarde al construir el archivo de enlace
(`unknown machine: 0`).

Con el build incremental `2f215d987e8a1268`, la validación agrupada terminó con
código 0: 41 comprobaciones de ThinLTO, matriz inline directa y precompilada
O0–O3, corrupción de uno y dos sidecars, objeto COFF inválido autenticado y
referencias de inline C a NativeSharp. El caso inválido mantuvo una biblioteca
ThinLTO y enlazó la otra desde su archivo nativo; el ejecutable devolvió 42.

La comprobación usa el lector de LLVM y exige COFF AMD64 con secciones. No
garantiza que un objeto válido sea semánticamente equivalente al de la
biblioteca ni cubre todas las corrupciones. La tanda 8 sigue abierta.
