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
  as a call target" (`analyzeVariable` no infiere el `fn*` de un method group único).

## Intento validado (incompleto)

- La inferencia del `fn*` desde un method group único SÍ se logró (build del `FunctionType` +
  `functionTypeIdentity`), y `var f = Double;` dejó de diagnosticar.
- **Causa raíz de `f(21)` fallido**: `bindMethodGroupToDelegate` (:23106) SIEMPRE crea un
  `MethodDelegateBinding{Create}` = **celda delegada** (code + receiver). Para un `fn*`
  ESTRUCTURAL (tipo natural de un method group ESTÁTICO) hace falta un **puntero de función crudo**
  (como el caso `externCTarget` :23200-23211, pero con calling convention nativa), no una celda.
  Por eso el local `f` (tipo `fn*(int)->int`) no queda invocable.

## Gap F07

1. Inferir el `fn*`/`fn&` natural de un method group único (hecho, ver arriba).
2. Bajar un method group ESTÁTICO con target `fn*` estructural a puntero de función crudo (no
   celda). El case `externCTarget` es el precedente; falta el camino nativo.
3. Congelar el ranking: overload con menos defaults > overload sin defaults > fallo ambiguo.

## Plan

1. En `bindMethodGroupToDelegate`, cuando el target es `fn*` estructural (no delegate nominal) y
   el method es estático, emitir un puntero de función crudo (replicando el camino externC sin C
   ABI). 
2. Prueba `TL-LANG-319` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-319`: `var f = Double; return f(21);` con `static int Double(int x)` — exit 42.
