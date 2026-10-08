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

## Evidencia dirigida posterior al commit

- Una primera invocación directa de CTest no exportó `NS_LIB_ROOT` y se detuvo al resolver
  `NativeSharp.Core.Collections.Generic`; no alcanzó el protocolo foreach.
- Con `NSC_LINKER`/`NSC_LLD_LINK` en `A:\nslink-devel\bin\release\nslink.exe` y
  `NS_LIB_ROOT=A:\native-sharp-3.0-baseline\lib`,
  `nsc.stage52-6.foreach-collections-enumerators` aprobó en `1.67 s`.
- El fallo 481 permanece abierto en el inventario integral hasta la batería completa O0 con NSLink.
