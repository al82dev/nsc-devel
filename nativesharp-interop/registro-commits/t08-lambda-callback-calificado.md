# T08-002: lambdas y métodos NativeSharp convertidos a callbacks C con datos apuntados calificados

Fecha: 2026-11-08. Pendiente que se cierra: «Verificar el parámetro `volatile T*` o `const T*` de una
lambda o método NativeSharp convertido a callback C».

## Reproducción

La sonda `claude/sondas/lambda1` usa un encabezado directo en O0 con `ConstFn` (`int (*)(const Payload*)`)
y `VolatileFn` (`int (*)(volatile Payload*)`):

| Caso | Antes | Esperado (contrato C) |
|---|---|---|
| `ConstFn f = (p) => { p->value = 1; return 0; };` | aceptado | rechazar la escritura mediante `const` |
| `VolatileFn f = (p) => p->value + 2;` | compila; IR sin ningún `load volatile` | leer con acceso volatile |
| método `int Read(Payload const* p)` → `ConstFn` | correcto | correcto |
| método `int Read(Payload* p)` → `ConstFn` o `VolatileFn` | rechazado (desde [t08-callback-const-identidad](t08-callback-const-identidad.md)) | rechazado; Clang: «incompatible» |

Causa: `bindLambdaToDelegate` creaba los parámetros de la lambda con el tipo del parámetro del callback.
Sin embargo, no les copiaba las marcas de la identidad estructural (`readonly`/`volatile` del dato
apuntado). Dentro del cuerpo, `p` se comportaba como un `Payload*` sin calificar.

## Cambio

`LambdaDelegateBinder.cpp`: después de `bindLambdaParameters`, si el destino es una identidad
estructural `fn:`, cada parámetro puntero por valor de la lambda recibe `pointerPointeeReadonly` y
`pointerPointeeVolatile` según las marcas del parámetro correspondiente. A partir de ahí se aplican las
reglas ya existentes para locales con esas marcas:

- se rechaza la asignación mediante `p->…`;
- las lecturas son volatile.

Una lambda con tipo explícito `(Payload* p)` también recibe la marca. El tipo escrito coincide con la
clave del tipo, y la calificación viene del destino, como en C, donde no puede descartarse.

## Prueba

Nueva: `pruebas-tanda-08-identidad-c/typedef-calificadores/validar-lambda-calificada.py`. Usa una DLL C
con `call_const(ConstFn)` y `call_volatile(VolatileFn)`. Resultado: aprobada.

- Control C con Clang:
  - las funciones equivalentes devuelven 41 y 42;
  - convertir `int (*)(Payload*)` a `ConstFn` o a `VolatileFn` es incompatible;
  - escribir mediante `const Payload*` es un error.
- IR: el cuerpo de la lambda convertida a `VolatileFn` contiene un `load volatile`.
- Cuatro rechazos, cada uno directo y precompilado sin encabezado:
  - escritura en una lambda con el tipo inferido;
  - escritura en una lambda con tipo explícito `Payload*`;
  - un método sin `const` convertido a `ConstFn`;
  - el mismo método convertido a `VolatileFn`.
- Positivos en O0–O3:
  - lambdas `ConstFn` y `VolatileFn`, y una lambda con tipo `Payload const*`, directas y precompiladas;
  - método `ReadConst(Payload const*)`, solo en el camino directo (ver límite).

Regresión dirigida: `regresion-callbacks.cmd`. Todas las pruebas se aprobaron salvo
`validar-record-precompilado.cmd`, que es el fallo previo ya registrado como T08-017-02a.

## Límites encontrados (pendientes registrados)

1. Un método NativeSharp público con un parámetro `T const*`, publicado en una biblioteca precompilada,
   pierde ese `const` en la firma que el consumidor materializa desde metadata. Por eso el consumidor no
   puede convertir `ReadConst` a `ConstFn` (`method group does not match a delegate parameter`). Los
   parámetros de miembro en NSMeta no tienen un campo para el `const` del dato apuntado. Corregirlo exige
   cambiar NSMeta, así que queda como decisión pendiente.
2. Una lambda sin tipos pasada directamente como argumento de una función C cuyo parámetro es un typedef
   de función (`call_const((p) => …)`) se rechaza con `an unconverted lambda requires an explicit type
   for every parameter`. Funciona asignándola antes a una variable `ConstFn`. Es un modo de consumo
   pendiente, independiente de los calificadores.

## Clasificación posterior del límite 2

La sonda `claude/sondas/lambda1/inline.py` / `typed.py` (O0) muestra que no es un caso de interop:

- `call_const((p) => …)`, `CallNs((p) => …)` con parámetro `ConstFn` y `CallH((p) => …)` con un delegate
  con nombre se rechazan igual: `an unconverted lambda requires an explicit type for every parameter`.
- Con tipos explícitos (`(Payload const* p) => …`, `(Payload* p) => …`), las tres formas, incluida una
  función solo NativeSharp con delegate con nombre, dan `no viable overload … accepts the argument types`.

Una lambda en línea no es un argumento convertible para ninguna función: es una regla general del
lenguaje, no una diferencia de la interoperabilidad C. Asignarla antes a una variable del tipo destino
funciona, y está cubierto por las pruebas de esta casilla. La casilla se cierra como fuera de alcance.
