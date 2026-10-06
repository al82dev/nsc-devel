# SEM-02.01: construction consume indices semanticos

**Requisitos:** `SEM-PERF-001`, `SEM-PERF-002`

**Tags:** `NSC.SEMANTICS.INDEX`, `NSC.SEMANTICS.CONSTRUCTION`

## Problemas cerrados

Por cada argumento de construccion, `ProjectConstructionModel::expressionType` recorria desde el inicio todos los
bindings de expresion del proyecto y devolvia la primera coincidencia syntax-only. Ademas, cuatro rutas filtraban
todos los miembros de un tipo para recuperar constructores.

El primer comportamiento era costoso y semanticamente debil: ignoraba el callable y no conservaba la regla de la
anotacion mas reciente.

## Cambio

- `expressionType` recibe `callerMemberOrdinal` y delega en el indice O(1) de `ProjectMethodBodyModel`;
- inicializadores `this`/`base` usan el ordinal del constructor caller;
- expresiones `new` usan su caller real;
- defaults, bases, inicializadores y `new` consumen `constructorsOfType`;
- los vectores finales de candidatos conservan el mismo orden estable de miembros.

## Complejidad

- tipo de cada argumento: O(1) promedio, antes O(E);
- catalogo de constructores: O(1) para obtenerlo y O(C) para evaluarlo, antes O(M + C);
- no cambia el algoritmo de overload resolution ni su orden de desempate.

## Validacion puntual O0

Se construye y ejecuta directamente `nsc_project_construction_model_tests` con temporales y evidencia en `A:`. La
auditoria estatica exige cero llamadas a `membersOfType` cuyo fin sea filtrar constructores y cero recorridos de
`methodBodyModel_->expressionTypes()`.

No se ejecuta baseline, minibaseline, CTest general, O1, O2 ni O3.

## Evidencia

- build dirigido: correcto;
- ejecucion directa: `PASS: 64 checks`;
- recorridos de `methodBodyModel_->expressionTypes()`: cero;
- filtros de constructor sobre `membersOfType`: cero;
- el recorrido residual de `membersOfType` verifica storage de campos precompilados y no busca constructores;
- la compilacion con warnings como errores no produjo hallazgos nuevos.
