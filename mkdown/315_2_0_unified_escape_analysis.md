# 315 — Analizador unificado de escapes / Unified escape analysis

## Resultado / Outcome

Los retornos `ref` ya no deciden seguridad mirando sólo la expresión final. Cada local `ref`
conserva el origen real de la dirección y el análisis lo propaga por las formas que pueden ocultar
storage del frame.

```nativesharp
private static ref int Invalid()
{
    int local = 1;
    ref int first = ref local;
    ref int second = ref first;
    return ref second; // rechazado: el origen sigue siendo local
}
```

Reference safety is now transitive: renaming a stack address or forwarding it through another
ref-returning callable does not erase its origin.

## Modelo / Model

`MethodEscapeOrigin` clasifica una dirección como:

- storage externo o perteneciente al llamador;
- local del frame actual;
- parámetro por valor/`in` del frame actual;
- storage VLA del frame.

`MethodLocalSymbol::referenceEscapeOrigin` guarda el resultado al declarar un alias. La consulta
única `referenceEscapeOriginOfExpression` sigue:

- aliases `ref` encadenados;
- fields de structs por valor;
- elementos de arrays inline/flexibles y procedencia VLA;
- receptores struct de indexadores `ref`;
- argumentos `ref`/`out` de una llamada que devuelve `ref`.

El análisis es deliberadamente conservador en llamadas: si un callable `ref` recibe storage del
frame por una posición `ref/out`, el resultado puede reenviarlo y no puede escapar. Un `T*` local,
en cambio, es sólo un slot que contiene una dirección; desreferenciarlo no atribuye al pointee el
lifetime del slot. La procedencia VLA explícita sí se conserva y se rechaza.

## Cobertura / Coverage

`tests/unit/Item315UnifiedEscapeAnalysisTests.cpp` valida:

- reenvío seguro de parámetros `ref`, storage estático y pointees crudos;
- ejecución O0 de aliases y llamadas válidas;
- rechazo de alias local directo y encadenado;
- rechazo de llamada `ref` que recibe un local del frame;
- rechazo de alias de parámetro por valor, field de struct local y elemento de array fijo local;
- exposición del origen en el dump del modelo.

Las suites de retornos/indexadores `ref`, VLA, borrows y views forman la regresión adyacente. El
análisis regional preciso de duración entre scopes corresponde al punto 316.

## Mensaje de commit / Commit message

`Unifica el análisis de escapes ref / Unify ref escape analysis`

`Conserva procedencia de direcciones a través de aliases, agregados, índices y llamadas, y rechaza
escapes transitivos del frame. / Preserve address provenance across aliases, aggregates, indices,
and calls, and reject transitive current-frame escapes.`
