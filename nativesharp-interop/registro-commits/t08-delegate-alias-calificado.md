# T08-002: delegates con nombre declarados con alias C de puntero calificado

Fecha: 2026-11-08. Pendiente que se cierra: «Llevar la calificación `volatile` a la firma de un `delegate`
con nombre declarado con alias C» (ampliado a `const`).

## Reproducción

Sonda `claude/sondas/delegate1`, encabezado directo, O0. El encabezado define
`typedef volatile Payload* VPayload;` y `typedef const Payload* CPayload;`. Los delegates
NativeSharp son `delegate int VHandler(VPayload p);`, `delegate int CHandler(CPayload p);`,
`delegate int PHandler(Payload* p);` y `delegate VPayload VGetter();`.

| Caso | Antes | Después |
|---|---|---|
| `VHandler h = take_v; h(get_v())` | rechazado («would discard … volatile») | correcto (42) |
| `VHandler h = (p) => p->value;` | rechazado al invocar | correcto; la lambda lee con `load volatile` |
| `CHandler h = (p) => { p->value = 1; … }` | aceptado | `left side of assignment is not assignable` |
| `VHandler h = take_p;`, `CHandler h = take_p;`, `PHandler h = take_v;` | aceptados | `no method in the method group exactly matches delegate signature` |
| `PHandler h = take_p; h(get_c())` | aceptado (se perdía `const`) | `would discard pointer pointee const qualification` |
| `VGetter g = get_v; Payload* p = g();` | aceptado | `initialization would discard pointer pointee volatile qualification` |
| `VGetter g = get_v; var p = g(); p->value` | lectura sin `volatile` | `load volatile` |

Causa: los parámetros y el retorno de un delegate con nombre solo conservaban la clave de tipo
(`Payload*`). Al expandir el alias C, la sintaxis sí registra los calificadores (`isPointeeConst`,
`isPointeeVolatile`), pero ninguna regla de delegates los consultaba.

## Cambio

- Nuevo `ProjectMethodBodyModel::delegatePointeeQualifiers(typeKey)`:
  - Para una identidad estructural `fn:`, devuelve la identidad.
  - Para un delegate con nombre declarado en código fuente, construye un `FunctionType` con las marcas
    tomadas de la sintaxis de sus parámetros y de su retorno.
  - Devuelve `nullopt` en los demás casos.
- Lo usan las mismas reglas que ya aplicaban a los callbacks estructurales:
  - conversión de grupos de métodos (`bindMethodGroupToDelegate`, `methodGroupExactlyMatchesDelegate`);
  - argumentos de invocación (`bindDelegateInvocationArguments`);
  - resultado de la invocación;
  - parámetros de lambdas.
  La comprobación `unsafe` sigue usando solo la identidad estructural, como antes.
- Un delegate materializado desde metadata (`isPrecompiled`) devuelve `nullopt`. La metadata conserva el
  tipo puntero pero no el alias C, así que sus calificadores son desconocidos, no ausentes. Rigen las
  reglas anteriores: no se introduce un rechazo nuevo ni una regresión en consumidores existentes.

## Prueba

Nueva: `pruebas-tanda-08-identidad-c/typedef-calificadores/validar-delegate-alias-calificado.py`.
Resultado: aprobada.

- Control C con Clang:
  - los mismos contratos escritos con punteros a función dan 42, 41, 40 y 43;
  - las tres conversiones con calificación distinta son incompatibles.
- IR: hay lecturas volatile en la lambda de `VHandler` y en el resultado de `VGetter`.
- Siete rechazos en el camino directo. `RejectPlainVolatileArgument` también se rechaza en el consumidor
  precompilado sin encabezado.
- Positivos en O0–O3:
  - camino directo: seis casos (métodos y lambdas `VHandler`/`CHandler`, getter volatile y el control
    sin calificar);
  - camino precompilado: los tres casos que no dependen de los calificadores del delegate materializado.

Regresión dirigida: `regresion-callbacks.cmd`. Todas las pruebas se aprobaron salvo T08-017-02a, que ya
fallaba antes. Las pruebas de
`nativesharp-interop` no declaran delegates con nombre; el conjunto general del compilador queda para la
validación integral.

## Pendiente

La firma pública materializada desde metadata pierde el `const`/`volatile` que el alias C aporta al
dato apuntado. Ocurre con métodos y con delegates con nombre. Por eso los consumidores precompilados
conservan las reglas anteriores para esos delegates. Corregirlo exige un campo en NSMeta: se suma a la
decisión pendiente registrada en
[t08-lambda-callback-calificado](t08-lambda-callback-calificado.md).
