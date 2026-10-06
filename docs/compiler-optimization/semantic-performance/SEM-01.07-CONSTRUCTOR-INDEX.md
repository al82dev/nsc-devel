# SEM-01.07: catalogo de constructores por tipo

**Requisito:** `SEM-PERF-001`

**Tag:** `NSC.SEMANTICS.INDEX`

## Problema

`ProjectConstructionModel` obtiene candidatos recorriendo todos los miembros fisicos de un tipo y filtrando
`ClassMemberKind::Constructor`. Ese patron se repite para defaults, inicializadores, bases y expresiones `new`.

## Cambio

`ProjectMemberModel` mantiene `constructorsByType_` en el mismo momento en que publica cada miembro. La API
`constructorsOfType` devuelve un `std::span` ordenado y no asigna memoria durante la consulta.

El orden coincide con `membersOfType`: las declaraciones se agregan con el mismo ordinal estable. Los miembros
ordinarios, campos, destructores y promociones anonimas no entran al catalogo. Un ordinal de tipo invalido devuelve
un span vacio.

## Complejidad

- registro: O(1) amortizado por constructor;
- consulta del catalogo: O(1);
- iteracion de candidatos: O(C), no O(M), donde C es el numero de constructores y M todos los miembros del tipo.

## Validacion puntual O0

Se construye y ejecuta directamente `nsc_project_member_model_tests` con temporales y evidencia en `A:`. La fixture
declara dos constructores y un metodo, y comprueba orden, filtrado y lookup fuera de rango.

No se ejecuta baseline, minibaseline, CTest general, O1, O2 ni O3.

## Evidencia

- build dirigido: correcto;
- ejecucion directa: `PASS: 70 checks`;
- la fixture confirma dos constructores, exclusion del metodo y span vacio fuera de rango;
- la compilacion con warnings como errores no produjo hallazgos nuevos;
- la limitacion conocida de `clang-tidy` permanece registrada y no se presenta como pasada limpia.
