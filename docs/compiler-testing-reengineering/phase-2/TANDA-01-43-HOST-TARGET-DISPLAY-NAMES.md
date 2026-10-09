# Tanda 01.43 — Nombres públicos de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-024`, casos `TF-TGT-185..192` |
| Producto | Observar nombres públicos de `TargetInfo`; no modificar |
| Excluido | LLVM triple, ABI, CPU name, features y registros de ensamblador |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- x64 publica exactamente `x86-64`, `windows` y `COFF`;
- ARM64 publica exactamente `arm64`, `windows` y `COFF`;
- una instancia fresca no requiere metadatos ni estado externo;
- el fallback de nivel CPU no altera los nombres de identidad;
- contenido, no vaciedad y longitudes exactas se comprueban en ambas arquitecturas;
- las 16 combinaciones de niveles x64 y la variante ARM64 conservan sus strings;
- repetición y cambio del orden de construcción no alteran el contenido;
- 65,536 consultas alternan ambas arquitecturas sin desviaciones.

El oráculo compara directamente el contenido y las longitudes contractuales. No depende de direcciones de memoria,
de otros strings derivados ni de recursión.

## Evidencia

- Gate `CF-TGT-024`: 8/8 en Debug (2.44 s) y Release (0.84 s).
- Suite nueva completa: Debug 42/42 en 16.47 s; Release 42/42 en 7.17 s.
- Controlador Python: 51/51; fuentes Python: 63/63 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 322 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-185` | nombres x64 mínimos |
| `TF-TGT-186` | nombres ARM64 representativos |
| `TF-TGT-187` | instancia fresca sin prerrequisito |
| `TF-TGT-188` | invariancia frente al fallback CPU |
| `TF-TGT-189` | no vaciedad y longitudes exactas |
| `TF-TGT-190` | matriz de factories y niveles |
| `TF-TGT-191` | determinismo y orden de construcción |
| `TF-TGT-192` | escala alternando arquitecturas |

Diagramas: no aplica; el contrato contiene tres strings inmutables por target.
