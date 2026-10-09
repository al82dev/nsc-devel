# Tanda 01.45 — Strings de máquina de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-026`, casos `TF-TGT-201..208` |
| Producto | Observar baseline, CPU LLVM y features de `TargetInfo`; no modificar |
| Excluido | Soporte booleano de features, registros e instrucciones de ensamblador |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- baseline x64 publica `x86-64-v1`, `x86-64` y `+sse,+sse2`;
- x86-64-v3 valida la entrada representativa completa de la tabla;
- el factory x64 predeterminado no requiere configuración externa;
- un nivel x64 desconocido devuelve vacíos los tres strings;
- x86-64-v4 y ARM64 validan los extremos contractuales;
- la tabla completa cubre v1, v2, v3, v4, ARM64 y desconocido;
- repetición y cambio del orden de construcción conservan los resultados;
- 65,536 consultas recorren iterativamente las seis variantes.

El oráculo contiene una tabla independiente de strings exactos. El nivel desconocido se construye desde su
representación subyacente sin desactivar diagnósticos. No se usan resultados del producto para derivar expectativas
y no hay recursión.

## Evidencia

- Gate `CF-TGT-026`: 8/8 en Debug (2.20 s) y Release (0.73 s).
- Suite nueva completa: Debug 44/44 en 17.26 s; Release 44/44 en 6.44 s.
- Controlador Python: 51/51; fuentes Python: 65/65 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 338 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-201` | strings baseline x64 |
| `TF-TGT-202` | strings representativos x86-64-v3 |
| `TF-TGT-203` | factory predeterminado sin prerrequisito |
| `TF-TGT-204` | nivel x64 desconocido |
| `TF-TGT-205` | extremos x86-64-v4 y ARM64 |
| `TF-TGT-206` | tabla completa de variantes |
| `TF-TGT-207` | determinismo y orden de construcción |
| `TF-TGT-208` | escala sobre las seis variantes |

Diagramas: no aplica; el contrato es una tabla inmutable de tres strings por variante.
