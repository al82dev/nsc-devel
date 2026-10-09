# Tanda 01.44 — Triple LLVM y ABI de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-025`, casos `TF-TGT-193..200` |
| Producto | Observar triple LLVM y nombre ABI de `TargetInfo`; no modificar |
| Excluido | Display names, CPU name, features y registros de ensamblador |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- x64 publica exactamente `x86_64-pc-windows-msvc` y `Microsoft Windows x64 ABI`;
- ARM64 publica exactamente `aarch64-pc-windows-msvc` y `Microsoft Windows ARM64 ABI`;
- una instancia fresca produce ambos strings sin estado externo;
- el fallback de nivel CPU no altera triple ni ABI;
- no vaciedad y longitudes exactas se comprueban en ambas arquitecturas;
- las 16 combinaciones de niveles x64 y ARM64 conservan su identidad LLVM;
- repetición y cambio de orden no alteran los strings;
- 65,536 consultas alternan ambas arquitecturas sin desviaciones.

El oráculo compara contenido y longitud exactos. No deriva la expectativa a partir de otros métodos del producto,
no compara direcciones de memoria y no usa recursión.

## Evidencia

- Gate `CF-TGT-025`: 8/8 en Debug (4.06 s) y Release (0.68 s).
- Suite nueva completa: Debug 43/43 en 16.94 s; Release 43/43 en 5.67 s.
- Controlador Python: 51/51; fuentes Python: 64/64 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 330 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-193` | triple y ABI x64 mínimos |
| `TF-TGT-194` | triple y ABI ARM64 |
| `TF-TGT-195` | instancia fresca sin prerrequisito |
| `TF-TGT-196` | invariancia frente al fallback CPU |
| `TF-TGT-197` | no vaciedad y longitudes exactas |
| `TF-TGT-198` | matriz de factories y niveles |
| `TF-TGT-199` | determinismo y orden de construcción |
| `TF-TGT-200` | escala alternando arquitecturas |

Diagramas: no aplica; el contrato contiene dos strings inmutables por target.
