# NS304-T04-F07 — Ranking de method groups, overload priority y conversiones a fn*/fn&

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development
- **Alcance (SCOPE LOCK)**: `ProjectMethodBodyModel::analyzeExpression` (method group),
  `bindMethodGroupToDelegate`, y la prueba `TL-LANG-319`. NO tocar CMake, toolchain, Builder,
  foreach, rango T01-F04.

## Auditoría (hallazgo validado)

- `bindMethodGroupToDelegate` (:22974) YA convierte un method group a delegate **con target
  explícito** (p.ej. argumento de delegate), y el ranking de overloads ya existe
  (`defaultedParameterCount` :24145, `methodGroupExactlyMatchesDelegate` :15446).
- **Falta el TIPO NATURAL**: `var f = Double;` falla con "method group 'Double' can only be used
  as a call target" (`analyzeVariable` no infiere el `fn*` de un method group único). Es el mismo
  gap que el residual de T04-F01 (method group natural).

## Gap F07

1. Inferir el `fn*`/`fn&` natural de un method group con UN único overload (o el único aplicable
   por ranking) cuando no hay target explícito.
2. Congelar el ranking: overload con menos defaults > overload sin defaults > fallo ambiguo.

## Plan

1. En `analyzeVariable`/`analyzeExpression`, cuando el initializer es un method group único,
   construir su `fn*` (`functionType`) y devolverlo como `ExpressionResult`.
2. Prueba `TL-LANG-319` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-319`: `var f = Double; return f(21);` con `static int Double(int x)` — exit 42.
