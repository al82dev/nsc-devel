# NS304-T05-F05 — Verificar borrows, moves, pinning, self-references y escapes entre suspensiones

- **Estado**: HECHO (borrow de local a través de suspensión re-localizado; pin y borrow de parámetro rechazados causalmente)
- **Rama**: development

## Resultado (re-localización de borrows — 2026-10-14)

- **Borrow de local**: `ProjectOopMirModel` arrastra el ORIGEN de un borrow vivo a través de la
  suspensión al frame (borrow-aware liveness); el borrow y su origen sobreviven. El puntero del
  borrow de tipo referencia es estable (heap), así que se guarda/restaura como local normal.
- **Pin**: sigue rechazado (identidad de dirección no preservada). **Borrow de parámetro/campo**:
  rechazado (sin slot de frame).
- **Prueba O0** (`TL-LANG-374`, exit 10): `borrowed Box view = box; yield; return view.Value;`
  sobrevive. `TL-LANG-434` (borrow de parámetro) se rechaza. Guards `TL-LANG-373/433` PASS.

## Nota

La variante de tipo valor (`borrowed int b = &y`) no existe: la fuente debe ser RAII y los tipos
valor no pueden ser RAII; por tanto solo aplica el borrow de tipo referencia (puntero estable).
