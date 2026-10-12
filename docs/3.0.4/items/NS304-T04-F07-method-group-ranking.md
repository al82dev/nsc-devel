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
- **Falta la conversión a valor delegado**: `bindMethodGroupToDelegate` convierte a delegate
  NOMINAL; para el `fn*` estructural (tipo natural) no produce un local invocable (`f(21)` da
  "unknown identifier 'f'"). Se necesita bajar un cell/function pointer real para el `fn*`
  estructural (como hace `bindMethodGroupToDelegate` con el target nominal).

## Gap F07

1. Inferir el `fn*`/`fn&` natural de un method group único (hecho, ver arriba).
2. Convertir el method group a un valor `fn*` invocable (cell/function pointer) sin target nominal.
3. Congelar el ranking: overload con menos defaults > overload sin defaults > fallo ambiguo.

## Plan

1. En `analyzeVariable`, cuando el initializer es method group único, inferir `fn*` y convertirlo
   a valor delegado (replicar `bindMethodGroupToDelegate` para `fn*` estructural).
2. Prueba `TL-LANG-319` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-319`: `var f = Double; return f(21);` con `static int Double(int x)` — exit 42.
