# NS3-T06-F07 — Análisis de escape interprocedural / Interprocedural escape analysis

## Resultado

NativeSharp 3.0 publica y consume un summary estable por parámetro. El frontend deriva contratos
`NoEscape`, `Return`, `CallerOwned`, `Global` o `Unknown`, propaga llamadas hasta punto fijo y
rechaza en edición 3 una dirección del frame cuando el callee demuestra que la retiene. Los
contratos cruzan `LibraryReference` mediante NSMETA 1.35 y forman parte de la huella pública.

## Cambios principales

- El modelo semántico conserva un summary por callable y por parámetro.
- Retornos de referencia, punteros/vistas, stores externos y captures alimentan el contrato.
- Las llamadas locales y de funciones libres propagan el efecto hasta punto fijo, incluidos
  wrappers que sólo reenvían a otro callable.
- Callables genéricos sin contrato cerrado, imports externos y metadata antigua se hidratan como
  `Unknown`; nunca se inventa `NoEscape`.
- NSMETA 1.35 serializa, valida y rehidrata el catálogo de miembros y funciones de namespace.
- La huella ABI/API incluye el summary, por lo que un cambio invalida consumidores incrementales.
- Edición 2 conserva el comportamiento histórico de punteros crudos, pero produce summaries para
  consumidores 3.0.

## Archivos nuevos

- `tests/unit/NativeSharp3T06EscapeSummaryTests.cpp`: única prueba dirigida del ítem.
- `docs/3.0/language/interprocedural-escape-analysis.md`: contrato normativo bilingüe.
- Este documento: mapa del commit y evidencia de validación.

## Archivos modificados

- Modelos frontend de miembros, funciones libres, cuerpos y compilación precompilada.
- Codec/merger NSMETA y versión menor 1.35.
- Driver de materialización de `LibraryReference`.
- `CMakeLists.txt`, expectativas de versión, documento de tanda y tracking maestro.

## Mejora observable

Una API separada puede prometer que sólo observa una dirección, devuelve una referencia o retiene
el argumento. El consumidor recibe exactamente el mismo contrato sin fuentes. Un wrapper local
hereda el efecto del callee, y una dirección de stack no puede cruzar una frontera que la guarda.

## Validación dirigida

- `nsc` compiló correctamente.
- Objetivo exacto `nsc_ns3_t06_escape_summary_tests` compiló correctamente.
- `nsc.ns3.t06.escape-summaries`: 1/1 test, 8/8 checks.
- Cubrió `NoEscape`, `Return`, `Global`, forwarding, closure, NSMETA 1.35, fingerprint, lectura
  1.34, catálogo corrupto, diagnóstico causal y edición 2.
- Baseline, suites, matrices y regresiones agrupadas: no ejecutados.
