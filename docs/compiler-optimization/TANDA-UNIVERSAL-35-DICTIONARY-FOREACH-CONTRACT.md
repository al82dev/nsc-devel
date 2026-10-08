# Tanda universal 35: contrato foreach de Dictionary

## Problema confirmado

`Dictionary<TKey,TValue>` publica actualmente `GetEnumerator()` y documenta un snapshot cuyo
arreglo pertenece al enumerador y cuyos pares son aliases no propietarios. La prueba 481 todavía
exigía que el compilador rechazara `foreach` directo porque conservaba el límite histórico de ABI
de structs pequeños.

Ese límite ya no describe al compilador ni a la biblioteca actuales. El fallo era una expectativa
negativa obsoleta, no una aceptación indebida del compilador.

## Cambio

Se conserva el mismo CTest y el mismo fixture, pero sus dos comprobaciones verifican ahora que:

- `foreach (var kv in dict)` resuelve el enumerador snapshot;
- no aparece el diagnóstico retirado de `GetEnumerator` ausente.

La prueba existente de `Dictionary.Keys()` y `Dictionary.Values()` permanece intacta. No se
debilita el protocolo foreach ni se agrega un caso especial al compilador.

## Archivos

- Modificado: `tests/unit/Stage52_6ForeachCollectionsEnumeratorsTests.cpp`.
- Nuevo: este documento.

## Validación requerida

1. Reconstruir el ejecutable de la prueba 481.
2. Ejecutar `nsc.stage52-6.foreach-collections-enumerators` en O0.
3. Incluir el cambio en la siguiente batería CTest completa.

