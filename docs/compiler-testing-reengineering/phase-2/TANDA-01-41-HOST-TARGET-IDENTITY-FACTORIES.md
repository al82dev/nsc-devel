# Tanda 01.41 — Factories e identidad de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-022`, casos `TF-TGT-169..176` |
| Producto | Observar factories e identidad de `TargetInfo`; no modificar |
| Excluido | Nombres, triples, ABI, tablas de features y registros de ensamblador |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- `WindowsX64()` produce exactamente x86-64, Windows, COFF y niveles baseline;
- el factory x64 personalizado conserva el nivel solicitado y el nivel seleccionado;
- los factories x64 y ARM64 son valores autocontenidos y no requieren estado externo;
- una reducción de capacidad de CPU no altera arquitectura, sistema operativo ni formato de objeto;
- los extremos baseline y x86-64-v4, además del factory ARM64, conservan la tupla completa;
- las 16 combinaciones x64 de nivel preferido y disponible más ARM64 conservan identidad exacta;
- repetición y cambio del orden de construcción no alteran ninguna propiedad de identidad;
- 65,536 construcciones recorren iterativamente las 16 combinaciones sin pérdida de identidad.

El oráculo compara campo por campo la identidad pública. La selección esperada se calcula de forma independiente
como el mínimo ordinal de los dos niveles x64. No usa strings derivados ni recursión.

## Evidencia

- Gate `CF-TGT-022`: 8/8 en Debug (1.33 s) y Release (0.34 s).
- Suite nueva completa: Debug 40/40 en 15.25 s; Release 40/40 en 7.53 s.
- Controlador Python: 51/51; fuentes Python: 61/61 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 306 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-169` | factory x64 predeterminado |
| `TF-TGT-170` | factory x64 personalizado representativo |
| `TF-TGT-171` | valores autocontenidos sin prerrequisito externo |
| `TF-TGT-172` | invariantes bajo fallback de CPU |
| `TF-TGT-173` | límites inferior, superior y ARM64 |
| `TF-TGT-174` | matriz completa de variantes |
| `TF-TGT-175` | determinismo y orden de construcción |
| `TF-TGT-176` | escala iterativa |

Diagramas: no aplica; el contrato es una tupla de identidad sin transición de estado.
