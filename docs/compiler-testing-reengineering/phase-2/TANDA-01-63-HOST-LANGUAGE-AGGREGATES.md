# Tanda 01.63 — Agregados de Host.Language

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\\nsc-devel` |
| Contrato | `CL-LANG-002`, casos `TL-LANG-015..024` |
| Producto | Observar `AggregateSemantics`; no modificar |
| Excluido | Callables, cambios de ABI, compilador completo y suite histórica |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura implementada

- identidad de tuplas vacías, unitarias y múltiples, con exclusión exacta de nombres;
- política estable de nombres y matriz de conversiones por aridad y tipo;
- layout Windows x64 vacío y con padding, alignment y offsets mixtos;
- clasificación de retorno para tamaños `0..16` y `size_t::max`;
- bindings de deconstrucción, descartes, nombres vacíos y duplicados;
- conjunto inmutable de derives, unión y consulta de las cuatro banderas;
- campos promovidos y aggregate spread con errores y prioridad exactos.

Los recorridos son iterativos y lineales en sus entradas. No se añadió recursión.

## Implementación de pruebas

- `LanguageAggregateSemanticsModule.cpp` registra nueve casos parametrizados en el ejecutable consolidado.
- `run_host_language_aggregate_semantics_gate.py` exige exactamente los nueve casos implementables.
- `cases.jsonl` conserva la trazabilidad `case_id -> CL-LANG-002 -> Host.Language` en Debug y Release.
- `AggregateSemantics.cpp` se enlaza sin modificaciones al ejecutable de pruebas.

## Bloqueo confirmado de `TL-LANG-019`

El caso diseñado exige `nullopt` para tamaño cero, alignment inválido y overflow. Los primeros dos grupos se
pueden observar, pero no se registró un caso parcial bajo el mismo ID porque eso afirmaría falsamente que la
matriz completa quedó cubierta.

El probe aislado con campos `{size_t::max, 1}` y `{1, 1}` produjo exactamente:

```text
has_value=1 size=0 alignment=1 offsets=0,18446744073709551615
```

La causa está en las sumas no comprobadas de `alignUp` y `result.size += field.size`. El resultado envuelve a
cero y la función devuelve un layout utilizable en vez de `nullopt`. Corregirlo modificaría producto y está fuera
del SCOPE LOCK de Fase 2. Por ello `CL-LANG-002` queda parcial: nueve casos verificados y uno bloqueado por
producto, sin `SKIP`, fallback ni oráculo reducido.

## Evidencia

- Gate implementable `CL-LANG-002`: 9/9 en Debug (3.36 s) y Release (0.54 s).
- Suite nueva completa: Debug 62/62 en 56.16 s; Release 62/62 en 10.01 s.
- Controlador Python: 51/51 en 0.665 s; fuentes Python: 83/83 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos del código de usuario.
- Manifiesto: 468 casos, 0 duplicados y 9 casos implementados del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Estado | Propósito |
|---|---|---|
| `TL-LANG-015` | VERIFICADO | identidad exacta de tuplas |
| `TL-LANG-016` | VERIFICADO | irrelevancia estable de nombres |
| `TL-LANG-017` | VERIFICADO | matriz de conversiones |
| `TL-LANG-018` | VERIFICADO | layouts válidos y padding |
| `TL-LANG-019` | BLOQUEADO_POR_PRODUCTO | overflow no rechazado por `AggregateSemantics` |
| `TL-LANG-020` | VERIFICADO | clasificación ABI de retorno |
| `TL-LANG-021` | VERIFICADO | bindings de deconstrucción |
| `TL-LANG-022` | VERIFICADO | bitset inmutable de derives |
| `TL-LANG-023` | VERIFICADO | campos promovidos |
| `TL-LANG-024` | VERIFICADO | fuente y unicidad de spread |

Diagramas: no aplica; el contrato consiste en funciones puras y matrices tabulares.
