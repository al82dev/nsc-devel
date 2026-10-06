# SEM-01.03: migracion de consultas calientes de expresion

**Requisito:** `SEM-PERF-001`

**Tag:** `NSC.SEMANTICS.INDEX`

## Problema cerrado

Seis rutas de `ProjectMethodBodyModel` buscaban tipos o constantes recorriendo `expressionTypes_` hacia atras. El
costo crecia con todas las expresiones ya analizadas y se repetia dentro de operaciones de escape, resolucion de
storage places, protocolos de slicing, setters de indexador y evaluacion constante.

## Cambio

Las siguientes rutas usan ahora `expressionTypeBinding(syntax, callableOrdinal)`:

- origen de escape de referencias;
- tipo y constante de un storage place;
- endpoints del protocolo `Length`/`Slice`;
- argumentos conservados para setters de indexador;
- evaluacion de expresiones constantes.

Cuando una funcion recibe un scope, el callable se obtiene del `MethodScope` ya construido. Las rutas que ya reciben
`methodMemberOrdinal` lo usan directamente. No existe fallback a un barrido syntax-only: una anotacion de otro
callable no es una respuesta valida.

## Complejidad

Cada consulta migrada pasa de O(E), donde E es el total acumulado de bindings de expresion, a O(1) promedio. El
orden del vector no cambia y la clave conserva la regla del binding mas reciente.

## Riesgos controlados

- scope invalido: produce ausencia, nunca captura una anotacion ajena;
- binding sin constante: conserva el resultado vacio anterior;
- setter o slicing sin anotacion: conserva `ExpressionResult` invalido;
- punteros devueltos: solo se consumen despues de terminar el registro correspondiente;
- no se introduce recursion nueva.

## Validacion

Se construye y ejecuta solamente `nsc_project_method_body_model_tests` en O0 con temporales bajo `A:`. Ademas se
comprueba estaticamente que `ProjectMethodBodyModel.cpp` ya no contiene recorridos sobre
`expressionTypes_.rbegin()`.

No se ejecuta baseline, minibaseline, CTest general, O1, O2 ni O3.

## Evidencia

- build incremental del target dirigido: correcto;
- ejecucion directa: `PASS: 62 checks`;
- busqueda residual `expressionTypes_.rbegin` en `ProjectMethodBodyModel.cpp`: cero coincidencias;
- warnings como errores: sin hallazgos nuevos durante la compilacion del archivo modificado;
- la limitacion de `clang-tidy` permanece clasificada en SEM-00.01 y SEM-01.02; no se presenta como una pasada
  limpia.
