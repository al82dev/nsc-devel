# Tanda 01.62 — ABI, FFI y SIMD de Host.Language

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\\nsc-devel` |
| Contrato | `CL-LANG-001`, casos `TL-LANG-001..014` |
| Producto | Observar `AbiSemantics`; no modificar |
| Excluido | Agregados, callables, cambios de política, compilador completo y suite histórica |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura contractual

- matrices válidas e inválidas de packing, alignment y representación transparente;
- máscaras de bitfield para storage 8/16/32/64 y límites inválidos;
- asignabilidad y necesidad de contexto para las cinco dimensiones del contrato de puntero;
- convenciones Windows x64, operaciones unsafe y umbral documentado de bloque amplio;
- parámetros FFI, callbacks escapables y política sin unwind;
- validación SIMD, construcción, identidad, parseo, shuffle, máscara y fallback escalar.

Los validadores son iterativos y acotados por las matrices de entrada. El parseo y la identidad son lineales en
el texto. No se añadió recursión.

## Implementación de pruebas

- `LanguageAbiSemanticsModule.cpp` registra catorce casos parametrizados en el ejecutable consolidado.
- `run_host_language_abi_semantics_gate.py` exige exactamente `TL-LANG-001..014` y catorce aprobaciones.
- `cases.jsonl` conserva la trazabilidad `case_id -> CL-LANG-001 -> Host.Language` en Debug y Release.
- `AbiSemantics.cpp` se enlaza sin modificaciones al ejecutable de pruebas.
- El valor enum externo de `UnsafeOperation` se caracteriza con una supresión local y documentada de
  `clang-analyzer-optin.core.EnumCastOutOfRange`.
- La auditoría ejecutable confirmó que un separador final de shuffle produce el error vigente de cardinalidad.

## Evidencia

- Gate `CL-LANG-001`: 14/14 en Debug (4.70 s) y Release (0.64 s).
- Suite nueva completa: Debug 61/61 en 53.61 s; Release 61/61 en 9.62 s.
- Controlador Python: 51/51 en 0.691 s; fuentes Python: 82/82 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; una supresión local intencional.
- Manifiesto: 459 casos, 0 duplicados y 14 casos del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron exactamente sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Caso | Propósito |
|---|---|
| `TL-LANG-001` | representaciones válidas |
| `TL-LANG-002` | representaciones inválidas y errores exactos |
| `TL-LANG-003` | máscaras y límites de bitfield |
| `TL-LANG-004` | matriz de asignabilidad de punteros |
| `TL-LANG-005` | matriz de conversión restringida de punteros |
| `TL-LANG-006` | convenciones y variadic Windows x64 |
| `TL-LANG-007` | operaciones unsafe y fallback externo |
| `TL-LANG-008` | umbral de bloque unsafe amplio |
| `TL-LANG-009` | matriz de parámetros FFI |
| `TL-LANG-010` | política FFI sin unwind repetida |
| `TL-LANG-011` | validación SIMD y límites de ancho |
| `TL-LANG-012` | identidad SIMD/mask y round-trip |
| `TL-LANG-013` | identidades SIMD malformadas |
| `TL-LANG-014` | shuffle, máscaras de lane y fallback escalar |

Diagramas: no aplica; el contrato consiste en validadores puros y matrices tabulares.
