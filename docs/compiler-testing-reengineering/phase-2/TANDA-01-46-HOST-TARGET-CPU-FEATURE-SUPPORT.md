# Tanda 01.46 — Soporte jerárquico de features de CPU de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-027`, casos `TF-TGT-209..216` |
| Producto | Observar `TargetInfo::supportsCpuFeature`; no modificar |
| Excluido | Selección de nivel, strings de máquina, registros e instrucciones de ensamblador |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- baseline x64 acepta el nombre vacío y las features mínimas hasta `sse2`;
- x86-64-v3 acepta features de niveles anteriores y propios, pero rechaza las de v4;
- el factory x64 predeterminado expone baseline sin configuración externa;
- features desconocidas o con capitalización no canónica se rechazan en x64 y ARM64;
- los límites cubren vacío, `x87`, `sse3`, `avx512vl`, `neon`, `fp-armv8` y `avx` en ARM64;
- la tabla completa cruza cuatro niveles x64 con 26 nombres y verifica seis nombres ARM64;
- repetición y recorrido inverso producen exactamente los mismos veredictos;
- 65,536 consultas mixtas recorren iterativamente ambas arquitecturas.

El oráculo usa una tabla independiente de nivel mínimo por feature y una regla independiente para ARM64. Las
expectativas no se derivan del resultado del producto. Los recorridos son iterativos, con costo lineal respecto al
número de consultas y memoria constante fuera de los arreglos fijos; no existe recursión.

## Implementación de pruebas

- `TargetCpuFeatureSupportModule.cpp` registra ocho casos cohesionados y sólo consulta la API pública observada.
- `run_host_target_cpu_feature_support_gate.py` exige el conjunto canónico `TF-TGT-209..216` y ocho aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CF-TGT-027 -> Host.Target` para Debug y Release.
- CMake registra el módulo en el ejecutable consolidado y añade un gate dirigido por configuración.

No se añadió estado global mutable, fallback, dependencia de configuración externa ni abstracción de producto. El
runner usa un directorio temporal aislado y verifica tanto identidad como conteos del conjunto ejecutado.

## Evidencia

- Gate `CF-TGT-027`: 8/8 en Debug (3.57 s) y Release (0.51 s).
- Suite nueva completa: Debug 45/45 en 20.05 s; Release 45/45 en 8.33 s.
- Controlador Python: 51/51 en 0.738 s; fuentes Python: 66/66 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 346 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-209` | features mínimas de baseline x64 |
| `TF-TGT-210` | inclusión jerárquica hasta x86-64-v3 |
| `TF-TGT-211` | factory predeterminado sin prerrequisito externo |
| `TF-TGT-212` | nombres desconocidos y no canónicos |
| `TF-TGT-213` | límites contractuales de x64 y ARM64 |
| `TF-TGT-214` | matriz completa de niveles y features |
| `TF-TGT-215` | determinismo, repetición y orden inverso |
| `TF-TGT-216` | escala iterativa mixta de 65,536 consultas |

Diagramas: no aplica; el contrato observado es una función booleana pura sobre arquitectura, nivel y nombre de
feature, y su matriz completa queda expresada por los fixtures tabulares.
