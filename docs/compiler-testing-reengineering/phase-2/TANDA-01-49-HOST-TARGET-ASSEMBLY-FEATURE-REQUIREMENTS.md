# Tanda 01.49 — Requisitos de características assembly de Host.Target

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CF-TGT-030`, casos `TF-TGT-233..240` |
| Producto | Observar `TargetInfo::requiredAssemblyFeature`; no modificar |
| Excluido | Validación del parser de assembly, CPU host y selección de backend |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- `popcnt` demuestra el requisito mínimo exacto `popcnt` en Windows x64;
- las familias AVX, AES, PCLMUL, SSE4.2 y BMI cubren prefijos y nombres exactos representativos;
- el factory Windows x64 clasifica mnemónicos sin metadata ni configuración externa;
- capitalización, espacios, sufijos indebidos y nombres desconocidos producen una característica vacía;
- los límites distinguen prefijos válidos de nombres exactos y prefijos incompletos;
- la tabla independiente contiene 26 pares mnemónico-característica y 10 entradas desconocidas;
- Windows ARM64 devuelve siempre una característica vacía para el mismo corpus;
- recorrido directo, recorrido inverso y repetición conservan cada resultado;
- 65,536 consultas iterativas alternan mapeos x64, desconocidos x64 y entradas ARM64.

El oráculo enumera independientemente los resultados esperados y no los obtiene de la implementación. La matriz
completa cuesta tiempo lineal respecto al corpus; la prueba de escala usa tiempo lineal y memoria auxiliar
acotada. No existe recursión.

## Implementación de pruebas

- `TargetAssemblyFeatureRequirementModule.cpp` registra los ocho casos en el ejecutable consolidado.
- `run_host_target_assembly_feature_requirement_gate.py` exige exactamente `TF-TGT-233..240`.
- `cases.jsonl` conserva la trazabilidad `case_id -> CF-TGT-030 -> Host.Target` para Debug y Release.
- CMake registra el módulo y su gate dirigido sin crear un ejecutable por prueba.

El módulo sólo consulta la API pública. No altera estado de producto y no comparte estado mutable entre casos. El
controlador crea un directorio temporal aislado y verifica identidad, orden y conteos.

## Evidencia

- Gate `CF-TGT-030`: 8/8 en Debug (2.15 s) y Release (0.47 s).
- Suite nueva completa: Debug 48/48 en 22.01 s; Release 48/48 en 9.68 s.
- Controlador Python: 51/51 en 2.567 s; fuentes Python: 69/69 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos.
- Manifiesto: 370 casos, 0 duplicados y 8 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- El código y la configuración legible cumplen 120 caracteres; el JSONL conserva una fila canónica por caso.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TF-TGT-233` | requisito mínimo de `popcnt` |
| `TF-TGT-234` | familias representativas y resultados exactos |
| `TF-TGT-235` | clasificación disponible sin metadata externa |
| `TF-TGT-236` | capitalización, espacios, sufijos y nombres inválidos |
| `TF-TGT-237` | límites de prefijos y nombres exactos |
| `TF-TGT-238` | tabla x64 completa, desconocidos y regla ARM64 |
| `TF-TGT-239` | determinismo, repetición y orden inverso |
| `TF-TGT-240` | escala iterativa de 65,536 consultas |

Diagramas: no aplica; el contrato observado es una consulta sin estado y la tabla enumera completamente sus
familias y límites.
