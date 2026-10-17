# Tanda correctiva NSC 3.0.5: balance causal del presupuesto en ondas G09

## SCOPE LOCK

Esta tanda corrige exclusivamente el balance de `InstantiationBudget` durante la pre-reserva y la
ejecución de una onda de especializaciones de tipo. No agrega sintaxis, no cambia semántica, ABI,
NSMETA, diagnósticos contractuales ni formatos públicos. No inicia L05, no ejecuta baseline, no
instala y no modifica consumidores.

La referencia inmediata es `development` después del commit correctivo que aisló las arenas de los
workers G09. La corrección conserva el conteo monotónico y la profundidad lógica configurada; sólo
impide que demandas hermanas se representen falsamente como frames causales anidados.

## Defecto demostrado

`executeTypeDemandWaves` pre-reservaba toda la onda antes de ejecutar cualquier demanda. Cada
`reserveTypeDemand` llamaba `instantiationBudget_.enter`, por lo que una onda ordenada `A, B, C, D`
dejaba temporalmente `activeStack() == [A, B, C, D]`.

La ejecución ocurría en orden directo. Al terminar `A`, `executeTypeDemand` llamaba `leave()` y
desapilaba `D`, no `A`. Además, cualquier especialización síncrona iniciada mientras se procesaba
`A` veía la anchura completa de la onda como si fuera profundidad causal.

La reproducción focal añadida al caso existente `TF-FRONT-168` fija profundidad máxima 4,
pre-reserva cuatro tipos hermanos y hace que el primer tipo materialice un método genérico. Antes
de la corrección falló exactamente con:

```text
generic instantiation depth limit (4) exceeded; causal chain:
Suite::Suite.Box0`1<int> -> Suite::Suite.Box1`1<int> ->
Suite::Suite.Box2`1<int> -> Suite::Suite.Box3`1<int> ->
Suite::Suite.Algorithms.Identity``1<int>
```

La cadena es imposible como genealogía: `Box0` a `Box3` son campos hermanos del mismo programa.
El diagnóstico demuestra que la pre-reserva mezclaba orden determinista con profundidad causal.

## Corrección

`reserveTypeDemand` conserva la llamada a `InstantiationBudget::enter` para:

- validar la profundidad lógica transportada por la demanda;
- validar el límite total de materializaciones;
- incrementar el conteo monotónico;
- actualizar la profundidad máxima observada.

Inmediatamente después de reservar el slot `Building`, libera ese frame con `leave()`. La reserva
no es una llamada recursiva y no debe sobrevivir hasta la ejecución. El slot ya publicado en
`SpecializationPlanner` impide que la misma clave vuelva a reservarse durante una dependencia
cíclica.

`executeTypeDemand` deja de llamar `leave()` en éxito o error porque ya no posee un frame causal.
Así se mantiene la correspondencia exacta `enter/leave` por demanda y ninguna ejecución desapila a
otra demanda hermana.

## Prueba focal

Se extendió el caso existente `TF-FRONT-168`; no se añadió una fila al manifiesto. El fixture:

- contiene cuatro tipos genéricos hermanos;
- materializa un método genérico desde el cuerpo clonado del primero;
- fija `GenericInstantiationDepth=4` mediante `ProjectLoadOptions`;
- solicita cuatro workers;
- exige completar `generics` sin convertir la anchura de la onda en profundidad;
- comprueba que el límite publicado siga siendo 4 y que la profundidad observada no lo exceda.

Resultado contrafactual:

- antes de la corrección: `TF-FRONT-168` FAIL con la cadena falsa mostrada arriba;
- después de la corrección: `TF-FRONT-168` PASS, exit 0.

Validación adicional después del cambio:

- build focal de `nsc_component_tests`: PASS;
- build focal de `nsc`: PASS y contrato de runtime estático PASS;
- `TL-LANG-122`: PASS;
- `TL-LANG-123`: PASS;
- `generics-32 --check-project-generics -j 16 --progress`: PASS, 32/32;
- repetición `generics-32 -j 16`: PASS 100/100;
- build hash local: `48d59f86b9093944`.

## Evidencia que esta tanda no afirma

El proyecto externo que terminó con `0xC00000FD` no existe en esta máquina. Esta corrección elimina
una segunda regresión demostrada de G09, pero no permite afirmar que esa ejecución externa quedó
reparada. El gate final continúa siendo repetir el mismo proyecto con un `nsc.exe` que muestre el
build hash anterior o uno posterior derivado de este commit y confirmar `generics finished`.

La auditoría estática también localizó recursión previa en inferencia, expansión de alias,
representación de tipos y algunos caminos especiales de clonación. Ninguna de esas rutas fue
modificada aquí porque todavía no existe evidencia que la relacione con el escenario externo. Se
mantienen como información no verificada, no como causa atribuida ni como autorización para abrir
otro refactor dentro de esta tanda.
