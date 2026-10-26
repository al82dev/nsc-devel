# Congelamiento 3.0 P3 — un parámetro `in` acepta un lugar readonly

Fecha: 2026-10-26. Revisión de partida: `b0a03bd98`.

## Problema

Un parámetro `in` no se podía reenviar. Dentro de `A(in int y)`, la llamada `B(in y)` daba «no viable overload of
'B'» (sonda R18InForward). Lo mismo con un resultado `ref readonly` (R1Method) y con un campo readonly.

`in x` se analiza como la dirección de `x`. Si el lugar es readonly, el puntero lleva `pointerPointeeReadonly`.
El símbolo del parámetro `in` no marcaba su puntero como de solo lectura, así que la resolución de sobrecargas
veía «descartar const» y rechazaba la candidata. P3 lo necesita: la biblioteca pasa claves recibidas `in` a
otros métodos `in`.

## SCOPE LOCK

- **Cambia.** `CallAnalyzer.cpp`: en las dos comprobaciones de contrato de puntero, un parámetro `in` cuenta
  como puntero a solo lectura.
- **Idéntico.** `ref` y `out` siguen exigiendo un lugar asignable. El ABI de `in` no cambia.
- **Prohibido.** Cambiar el símbolo del parámetro ni NSMETA.

## Validación

| Sonda | Resultado |
|---|---|
| R18InForward (`B(in y)` con `y` un `in`) | 5 (antes no compilaba) |
| R1Method (`Use(in c.Get())` con `Get` `ref readonly`) | 7098 |
| R15InMut (`in` desde un `ref` mutable) | 14, sin cambios |

Casos TL-LANG de referencias, préstamos y vistas en Debug y Release: ver doc 23.
