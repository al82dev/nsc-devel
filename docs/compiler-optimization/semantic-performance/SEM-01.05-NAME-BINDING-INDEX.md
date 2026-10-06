# SEM-01.05: indice de nombres integrado

**Requisito:** `SEM-PERF-001`

**Tag:** `NSC.SEMANTICS.INDEX`

## Cambio

Las veintitres rutas que registraban `MethodNameBinding` ahora pasan por `recordNameBinding`. La funcion conserva el
append al vector ordenado y publica su ordinal bajo `(syntax, methodMemberOrdinal)`. La consulta publica
`nameBinding` devuelve el binding mas reciente de la clave exacta en O(1) promedio.

Este commit integra y prueba el productor del indice. La migracion de consumidores lineales queda separada en el
siguiente commit para que una regresion pueda atribuirse al registro o al reemplazo de las consultas, no a ambos.

## Invariantes

- todo binding, incluidos errores parciales y referencias sinteticas existentes, sigue entrando al vector;
- el orden y los ordinales de `nameBindings_` no cambian;
- el indice guarda ordinales, no direcciones de elementos;
- una clave repetida reemplaza solo la vista indexada;
- `build` limpia el indice al preparar el modelo;
- no se introduce recursion.

## Validacion puntual O0

Se construye y ejecuta directamente `nsc_project_method_body_model_tests` con temporales y evidencia en `A:`. La
prueba recupera el ultimo nombre por sintaxis y callable. La auditoria estatica exige una unica insercion directa en
`nameBindings_`, encapsulada dentro de `recordNameBinding`.

No se ejecuta baseline, minibaseline, CTest general, O1, O2 ni O3.

## Evidencia

- build dirigido: correcto;
- ejecucion directa: `PASS: 64 checks`;
- inserciones directas en `nameBindings_`: una, encapsulada en `recordNameBinding`;
- la compilacion con warnings como errores no produjo hallazgos nuevos;
- la limitacion conocida de `clang-tidy` permanece registrada y no se presenta como pasada limpia.
