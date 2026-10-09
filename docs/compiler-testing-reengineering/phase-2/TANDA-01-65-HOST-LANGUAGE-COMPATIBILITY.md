# Tanda 01.65 — Compatibilidad y migración de Host.Language

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\\nsc-devel` |
| Contrato | `CL-LANG-004`, casos `TL-LANG-040..049` |
| Producto | Observar `CompatibilitySemantics`; no modificar |
| Excluido | Coroutines, cambios de política, compilador completo y suite histórica |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- prioridad de rechazo por estructura, target y metadata, seguida por fingerprint;
- planes de compilación vacíos, unitarios, múltiples, incompletos, duplicados y con imports ausentes;
- clausura incremental sin cambios, hoja, raíz, diamante, módulo eliminado y módulo nuevo;
- códigos `MIG3001..MIG3007` y fallback externo `MIG3000`;
- matriz completa de reemplazo y equivalencia para arreglos automáticos;
- las 16 combinaciones de cambio de firma, layout, efecto y privacidad;
- guías vacías, válidas, incompletas, desordenadas y duplicadas, más cuatro justificaciones de retiro.

Los recorridos de grafos y matrices son iterativos. No se añadió recursión.

## Implementación de pruebas

- `LanguageCompatibilitySemanticsModule.cpp` registra diez casos parametrizados.
- `run_host_language_compatibility_semantics_gate.py` exige `TL-LANG-040..049` completos y ordenados.
- `cases.jsonl` conserva la trazabilidad `case_id -> CL-LANG-004 -> Host.Language` en Debug y Release.
- `CompatibilitySemantics.cpp` se enlaza sin modificaciones al ejecutable consolidado.
- El enum externo de `MigrationIssue` tiene una supresión local y documentada del analizador.

## Evidencia

- Gate `CL-LANG-004`: 10/10 en Debug (3.92 s) y Release (0.54 s).
- Suite nueva completa: Debug 64/64 en 62.48 s; Release 64/64 en 10.82 s.
- Controlador Python: 51/51 en 0.750 s; fuentes Python: 85/85 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; una supresión local intencional.
- Manifiesto: 491 casos, 0 duplicados y 10 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservan los SHA-256 fijados en el cursor.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TL-LANG-040` | rechazo y prioridad de compatibilidad modular |
| `TL-LANG-041` | fingerprint directo o adapter |
| `TL-LANG-042` | planes válidos y cardinalidad |
| `TL-LANG-043` | invariantes y primer error del plan |
| `TL-LANG-044` | clausura incremental transitiva |
| `TL-LANG-045` | módulos retirados y nuevos |
| `TL-LANG-046` | códigos de migración y fallback |
| `TL-LANG-047` | matriz de arreglo automático |
| `TL-LANG-048` | clasificación completa de cambio API |
| `TL-LANG-049` | guía de migración y retiro 3.x |

Diagramas: no aplica; el contrato se expresa mediante tablas y grafos pequeños declarativos.
