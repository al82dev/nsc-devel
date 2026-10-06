# SEM-01.04: indice de llamadas y recorrido iterativo de packs

**Requisito:** `SEM-PERF-001`

**Tag:** `NSC.SEMANTICS.INDEX`

## Problema cerrado

Los bindings de metodo se agregaban desde diecinueve salidas distintas y cinco consultas recorrian `calls_` en
sentido inverso. Una de ellas, la deteccion de parameter packs expandidos, recorria ademas el arbol mediante llamadas
recursivas.

## Cambio

`recordCallBinding` centraliza toda insercion. Conserva el append al vector determinista y publica el ordinal bajo
`(syntax, callerMethodMemberOrdinal)`. `callBinding` devuelve la anotacion mas reciente de la clave exacta.

Las consultas indexadas cubren:

- deteccion de parameter packs expandidos;
- origen de escape de una llamada resuelta;
- storage place de retornos por referencia;
- validacion de `must_tail`;
- recuperacion del getter antes de analizar el RHS de una asignacion.

La deteccion de packs ahora usa una cola respaldada por `std::vector`. Elimina la recursion y conserva la semantica
booleana: termina en cuanto encuentra una llamada resuelta que usa expansion.

## Seguridad y determinismo

- el indice almacena ordinales y no punteros realocables;
- el getter se convierte inmediatamente a ordinal antes de que el RHS pueda ampliar el vector;
- el vector conserva el orden de dumps, demanda generica y propagacion de escape;
- una llamada fallida tambien queda indexada, igual que antes quedaba en el vector;
- las consultas validan resolucion y miembro seleccionado despues del lookup;
- no queda ningun recorrido inverso de `calls_` en `ProjectMethodBodyModel`.

## Validacion puntual O0

Se construye y ejecuta directamente `nsc_project_method_body_model_tests` con temporales y evidencia en `A:`. La
prueba comprueba que la ultima llamada de una clave se recupera por sintaxis y callable. Una busqueda estatica debe
confirmar cero usos de `calls_.rbegin` y que la unica insercion directa en `calls_` vive dentro de
`recordCallBinding`.

No se ejecuta baseline, minibaseline, CTest general, O1, O2 ni O3.

## Evidencia

- build dirigido: correcto;
- ejecucion directa: `PASS: 63 checks`;
- `calls_.rbegin`: cero coincidencias;
- inserciones directas en `calls_`: una, encapsulada en `recordCallBinding`;
- la compilacion con warnings como errores no produjo hallazgos nuevos;
- la limitacion conocida de `clang-tidy` permanece registrada y no se presenta como pasada limpia.
