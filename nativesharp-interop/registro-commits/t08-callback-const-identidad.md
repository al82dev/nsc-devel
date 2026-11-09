# T08-002: `const` del dato apuntado en la identidad de callbacks C

Fecha: 2026-11-08. Rama `development-cpp-clang`. Pendiente que se cierra: «Conservar `const` del dato
apuntado en la identidad de callbacks C (hoy no se codifica)», registrado en
[t08-callback-volatile-identidad](t08-callback-volatile-identidad.md).

## Reproducción

Sonda con un encabezado directo en O0 (`claude/sondas/const1`). Antes del cambio, NSC aceptaba seis
programas que Clang rechaza o en los que la calificación se perdía:

| Caso | Antes | Después |
|---|---|---|
| `PlainFn f = take_plain; f(get_const())` | aceptado | `indirect call argument would discard pointer pointee const qualification` |
| `PlainFn f = take_const;` | aceptado | `no method in the method group exactly matches delegate signature` |
| `TakeFn t = take_plain;` | aceptado | igual que el anterior |
| `GetFn g = get_const; Payload* p = g();` | aceptado | `initialization of mutable-pointee pointer 'p' would discard pointee const` |
| `GetFn g = get_const; var p = g(); p->value = 3;` | aceptado | `left side of assignment is not assignable` |
| `TakeFn t = take_const; PlainFn f = t;` | aceptado | `cannot initialize local 'f' …` |

La llamada directa ya se comportaba bien: `take_plain(get_const())` se rechazaba y la escritura mediante
`var p = get_const()` también. Solo el camino indirecto perdía `const`. La causa era la misma que en el
caso de `volatile`: la identidad estructural `fn:` no registraba el calificador del dato apuntado. Por
eso `TakeFn` (`const Payload*`) y `PlainFn` (`Payload*`) tenían la misma identidad.

## Cambio

- `FunctionType` ahora tiene `parameterPointeeReadonly` y `resultPointeeReadonly`.
- `CallableSemantics.cpp` generaliza el sufijo de calificadores. Cada calificador tiene un descriptor
  (`PointeeQualifier`: etiqueta y miembros) y se escribe como sufijo opcional `<etiqueta><máscara>`:
  primero `:readonly-pointee:` y después `:volatile-pointee:`. Solo se escribe si hay alguna marca, así
  que las identidades sin calificadores conservan sus bytes.
- El análisis exige el orden indicado y el final exacto de la cadena. Rechaza las máscaras de longitud
  incorrecta, con caracteres distintos de 0/1 o sin ninguna marca.
- `functionTypesAssignable` exige que ambos calificadores coincidan exactamente.
- `canonicalFunctionTypeKey` y `functionTypeSpelling` toman `isPointeeConst()` de la sintaxis, ya sea
  del typedef C o de `T const*` escrito en NativeSharp.
- El tipo natural de `var` copia `pointerPointeeReadonly` de los parámetros y del retorno del método.
- La conversión de grupos de métodos a callbacks estructurales (`methodPointeeVolatileMatches`) exige
  también el mismo `const`.
- La invocación indirecta rechaza un argumento `const` en un parámetro puntero sin `const` del callback
  estructural. Una posición variádica lo acepta, como en C y como en la llamada directa. El resultado
  conserva `pointerPointeeReadonly`.
- `MetadataSourceMapping`: a diferencia de `volatile`, NativeSharp sí puede escribir `T const*`. Por
  eso la firma materializada desde metadata escribe el parámetro o el resultado como `T const*` y
  conserva la marca, en lugar de rechazarla.

No cambian NSMeta (ni su formato ni sus campos), NSLink ni LLVM/Clang. `CppSemanticFunction` sigue
rechazando los punteros a función sin typedef con parámetros `const`; ese pendiente ya está registrado
para `volatile` y ahora cubre también `const`.

## Pruebas

Nueva: `pruebas-tanda-08-identidad-c/typedef-calificadores/validar-callback-const.py`. Resultado:
aprobada.

- Control C con Clang:
  - Los seis casos positivos devuelven los valores esperados.
  - Las tres conversiones incompatibles son errores.
  - `Payload* p = g()` informa «discards qualifiers».
  - La escritura mediante `const Payload*` es un error («const-qualified»).
- Seis rechazos, cada uno en dos caminos: directo con encabezado y precompilado consumido sin
  encabezado. En todos los casos el rechazo se produce en el consumidor.
- Positivos en O0–O3, directos y precompilados sin encabezado: invocación, resultado, `var`, variádica,
  añadir `const` a un argumento sin `const` y el control sin calificadores.
- Superficie pública en O0–O3: los métodos públicos `Callback()`, `Getter()` y
  `Apply(TakeFn, GetFn)` se publican desde la biblioteca precompilada. El consumidor, sin encabezado,
  los usa y obtiene 41. Escribir mediante el resultado de `Getter()` se rechaza («not assignable»), lo
  que demuestra que la firma materializada conserva `const`.

Regresión dirigida:

- `typedef-calificadores/validar-callback-volatile.py`: aprobada (control C, IR, seis rechazos,
  positivos O0–O3).
- `regresion-callbacks.cmd`: aprobadas todas las pruebas salvo
  `pruebas-tanda-08-callback/validar-record-precompilado.cmd`. Esa prueba ya fallaba antes de este
  cambio con el mismo error, `conflicting C/C++ type alias 'RecordCallback'`, y está registrada como
  T08-017-02a.
- `invocacion-matriz.cmd` (`validar-invocacion.py`, O0–O3 × {volatile, const volatile}): aprobada, 8/8.

## Pendiente que queda abierto

- Una lambda convertida a `TakeFn` compila. No se verificó si su parámetro recibe `const` (ni
  `volatile`) dentro del cuerpo. Se amplía el pendiente existente de lambdas para cubrir ambos
  calificadores.
