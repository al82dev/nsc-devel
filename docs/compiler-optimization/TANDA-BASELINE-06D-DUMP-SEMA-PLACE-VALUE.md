# Tanda baseline 06D: categorías semánticas place/value

## Objetivo

Restaurar la equivalencia funcional SH42 entre el compilador nativo y el
compilador autoalojado sin relajar ni normalizar artificialmente sus dumps.

## Causa

El modelo semántico nativo de NativeSharp 3.0 publica las categorías
`place` y `value`. El dumper autoalojado conservaba las etiquetas históricas
`lvalue` y `rvalue`, aunque ambas implementaciones clasificaban las mismas
expresiones de la misma forma.

## Cambios

- El dumper semántico autoalojado publica `place` para `ValueCategory.LValue`.
- El dumper semántico autoalojado publica `value` para `ValueCategory.RValue`.
- El comentario del caso especial de declaraciones usa la terminología 3.0.
- La comparación byte a byte de SH42 permanece estricta.

## Archivos modificados

- `selfhost/nsc/Sema/SemanticDumper.ns`

## Archivos nuevos

- Este documento de trazabilidad de la tanda.

## Mejora obtenida

El compilador autoalojado vuelve a exponer el mismo contrato semántico público
que `nsc0`, sin ocultar diferencias reales mediante filtros del verificador.

## Validación dirigida

- Se generó un `nsc1` temporal desde las 98 unidades actuales.
- Se emitió únicamente el candidato autoalojado O0 y se enlazó con NSLink.
- `--dump-sema tests/integration/valid/cfg_features.ns` terminó con código 0
  tanto en `nsc0` como en el candidato O0.
- Las salidas fueron idénticas; el candidato publicó 7 categorías `place`,
  10 categorías `value` y ninguna etiqueta `lvalue/rvalue`.
- Los artefactos temporales se eliminaron después de la comprobación.
- No se ejecutó el baseline completo ni se modificó `C:`.
