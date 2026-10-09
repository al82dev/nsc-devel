# Tanda 01.56 — Merge canónico de Host.Diagnostics

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Contrato | `CD-DIA-007`, casos `TD-DIA-032..042` |
| Producto | Observar `DiagnosticEngine::mergeDeterministically`; no modificar |
| Excluido | Append aislado, clear, renderizado y productores de diagnósticos |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- claves distintas producen la misma secuencia canónica aunque cambie el orden de llegada de los shards;
- una ubicación válida precede a una inválida;
- `FileId`, inicio y fin forman los tres campos de ubicación del orden lexicográfico;
- la severidad usa explícitamente `Error`, `Warning`, `Lint`, `Note` y no el valor subyacente del enum;
- el ID entero y el mensaje completan la clave cuando los campos anteriores empatan;
- un diagnóstico sin rango usa el offset de su ubicación como clave final;
- los diagnósticos existentes del receptor conservan su prefijo y el bloque mezclado se agrega después;
- el presupuesto del receptor se aplica una sola vez sobre la secuencia canónica;
- los suprimidos del receptor se suman con los suprimidos acumulados por cada shard;
- un empate total que difiere sólo en datos secundarios conserva ambos payloads sin afirmar orden relativo.

La recolección es `O(D)`, el ordenamiento es `O(D log D)` y la copia final es lineal en diagnósticos y contenido.
La memoria auxiliar es `O(D)`. Los casos y helpers son iterativos; no existe recursión.

## Implementación de pruebas

- `DiagnosticEngineMergeModule.cpp` registra once casos dentro del ejecutable consolidado.
- `run_host_diagnostics_engine_merge_gate.py` exige exactamente `TD-DIA-032..042` y once aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CD-DIA-007 -> Host.Diagnostics` en Debug y Release.
- Diez casos pertenecen al perfil PR; el límite de orden no total `TD-DIA-042` pertenece a NIGHTLY.
- El gate dirigido usa NIGHTLY, cuyo modelo acumulativo incluye los diez casos PR y el caso NIGHTLY.
- El caso del empate compara el multiconjunto de payloads, no inventa estabilidad ausente del contrato.

## Evidencia

- Gate `CD-DIA-007`: 11/11 en Debug (3.23 s) y Release (0.54 s).
- Suite nueva completa: Debug 55/55 en 39.48 s; Release 55/55 en 8.63 s.
- Controlador Python: 51/51 en 0.653 s; fuentes Python: 76/76 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos propios.
- Manifiesto: 412 casos, 0 duplicados y 11 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- El código y la configuración legible cumplen 120 caracteres; el JSONL conserva una fila canónica por caso.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TD-DIA-032` | independencia del orden de llegada para claves distintas |
| `TD-DIA-033` | ubicación válida antes que inválida |
| `TD-DIA-034` | orden por FileId, inicio y fin |
| `TD-DIA-035` | orden explícito de severidades |
| `TD-DIA-036` | orden por valor entero del ID |
| `TD-DIA-037` | orden lexicográfico del mensaje |
| `TD-DIA-038` | clave final de un diagnóstico sin rango |
| `TD-DIA-039` | conservación del prefijo del receptor |
| `TD-DIA-040` | presupuesto único aplicado por el receptor |
| `TD-DIA-041` | suma de supresiones de shards y receptor |
| `TD-DIA-042` | límite declarado para empates que sólo difieren en datos secundarios |

Diagramas: no aplica; la tabla sigue en orden los siete campos de la clave y los tres efectos posteriores.
