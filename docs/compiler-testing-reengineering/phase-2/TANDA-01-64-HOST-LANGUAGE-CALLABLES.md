# Tanda 01.64 — Callables y closures de Host.Language

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\\nsc-devel` |
| Contrato | `CL-LANG-003`, casos `TL-LANG-025..039` |
| Producto | Observar `CallableSemantics`; no modificar |
| Excluido | Compatibilidad, coroutines, compilador completo y suite histórica |
| Protegido | 684 pruebas históricas, seis cambios preexistentes, `C:` y remotos |

## Cobertura implementada

- identidad byte a byte, parseo, round-trip, predicado y gramática inválida de tipos función;
- matriz direccional completa de asignabilidad por campo contractual;
- spellings y fallbacks de carriers y calling conventions;
- identidad, parseo, predicado y nombre generado de lambdas anónimas en ordinales de borde;
- escape de captures por valor, move y referencias con lifetime;
- prioridad `FnOnce`, `FnMut`, `Fn`, conversión a puntero y dispatch genérico;
- layout de bound method Windows x64 y prioridad de errores `must_tail`.

Los parsers y matrices son iterativos y acotados por la entrada. No se añadió recursión.

## Implementación de pruebas

- `LanguageCallableSemanticsModule.cpp` registra trece casos parametrizados en el ejecutable consolidado.
- `run_host_language_callable_semantics_gate.py` exige exactamente los trece casos implementables.
- `cases.jsonl` conserva la trazabilidad `case_id -> CL-LANG-003 -> Host.Language` en Debug y Release.
- `CallableSemantics.cpp` se enlaza sin modificaciones al ejecutable de pruebas.
- Dos casts de enum externo tienen supresiones locales y documentadas para caracterizar fallbacks públicos.

## Bloqueos confirmados

### `TL-LANG-034`

El diseño pide offsets, size y alignment exactos. `ClosureLayout` sólo expone `size` y `alignment`; no existe un
offset observable ni una consulta pública equivalente. Registrar el caso con un oráculo reducido afirmaría una
cobertura que el producto no permite observar. El caso queda `BLOQUEADO_POR_PRODUCTO` completo.

### `TL-LANG-035`

El diseño exige rechazo exacto de nombre vacío, size cero, alignment inválido y overflow. El probe aislado con
captures `{size_t::max, 1}` y `{1, 1}` produjo:

```text
accepted=1 size=0 alignment=1 error=
```

`computeClosureLayout` alinea y suma sin comprobación, acepta el wrap a cero y no produce error. Corregirlo
modificaría producto y está fuera del SCOPE LOCK. El caso queda bloqueado completo; no se registró una versión
parcial que ocultara el vector de overflow.

## Evidencia

- Gate implementable `CL-LANG-003`: 13/13 en Debug (4.90 s) y Release (0.64 s).
- Suite nueva completa: Debug 63/63 en 65.74 s; Release 63/63 en 12.75 s.
- Controlador Python: 51/51 en 0.850 s; fuentes Python: 84/84 compiladas en memoria.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos; dos supresiones locales intencionales.
- Manifiesto: 481 casos, 0 duplicados y 13 casos implementados del contrato.
- Builds con warnings como errores; diferencial de producto: 0 archivos.
- Dependencias dinámicas: sólo `KERNEL32.dll`; CRT dinámico: 0.
- Los seis archivos preexistentes conservaron sus SHA-256 registrados.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Trazabilidad

| Casos | Estado | Propósito |
|---|---|---|
| `TL-LANG-025..030` | VERIFICADOS | funciones, identidades, parseo, asignabilidad y nombres |
| `TL-LANG-031..033` | VERIFICADOS | identidad, gramática y nombres de lambdas |
| `TL-LANG-034` | BLOQUEADO_POR_PRODUCTO | offsets no observables en `ClosureLayout` |
| `TL-LANG-035` | BLOQUEADO_POR_PRODUCTO | overflow aceptado con wrap a cero |
| `TL-LANG-036..039` | VERIFICADOS | escape, traits, conversión, bound method y `must_tail` |

Diagramas: no aplica; el contrato consiste en valores, parsers y matrices tabulares.

## Adenda de test plan — 2026-10-09

La expectativa de offsets de `TL-LANG-034` no pertenece a la superficie pública
del compilador y se retiró. El caso conserva la capacidad real
`computeClosureLayout` y comprobará únicamente `size` y `alignment`. La
clasificación `BLOQUEADO_POR_PRODUCTO` de la tabla histórica queda sustituida por
`PLAN CORREGIDO — IMPLEMENTABLE`; no se modifica el producto.

El caso fue implementado posteriormente y pasó su ejecución exclusiva. El
contrato queda en 14/15 casos implementados y verificados; `TL-LANG-035`
permanece como prueba por materializar para revelar el defecto de overflow.
