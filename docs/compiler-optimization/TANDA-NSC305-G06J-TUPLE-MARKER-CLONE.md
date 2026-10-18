# NSC 3.0.5 - G06J: conservación de `isTuple` en el clon genérico iterativo

## Alcance

Esta corrección cierra una regresión puntual del clon iterativo de `TypeSyntax`. No cambia sintaxis,
AST público, resolución nominal, diagnóstico, ABI, NSMETA ni contratos del lenguaje. Tampoco forma
parte de R01H ni de M02A.

## Síntoma reproducible

El gate focal `nsc.ns3.t02.native-tuples` quedaba en 22 de 23 comprobaciones. La compilación de la
fuente positiva fallaba sobre el argumento tupla de `Box<(int, int)>`:

```text
type '$tuple' does not name a unique visible generic class/struct template
```

El texto del diagnóstico cubre tanto ausencia como ambigüedad. La auditoría no encontró dos
plantillas nominales `$tuple`: el nodo estructural había perdido su marca antes de llegar a
`ProjectGenericModel::processTypeSyntax`.

## Causa

`Parser::parseTupleType` construye correctamente `$tuple`, llama `setTuple()` y agrega sus
elementos. El primer punto que perdía esa propiedad era
`ProjectGenericModel::cloneTypeGenericShallow`.

Antes del recorrido iterativo, la rama de tuplas llamaba `addTupleElement` al clonar el primer
elemento. Esa operación vuelve a establecer internamente `isTuple_`. El commit `4adc53e27`
(`perf(3.0.5): make cloneType type traversal iterative`) cambió el recorrido para decidir entre
`addTupleElement` y `addTypeArgument` consultando primero `item.parent->isTuple()`.

El padre superficial nacía con la marca en falso. Por ello el recorrido elegía `addTypeArgument`
para todos los hijos, nunca recuperaba la marca y presentaba `$tuple` como un tipo genérico nominal.
El defecto también alcanzaba a la tupla vacía, que no tiene un primer elemento capaz de restaurar
la propiedad de forma incidental.

## Corrección

El clon superficial copia explícitamente la propiedad estructural junto con las demás propiedades
escalares:

```cpp
result->setTuple(source->isTuple());
```

La modificación es local y conserva el algoritmo iterativo. Los elementos siguen agregándose por
la ruta existente; ahora esa ruta selecciona `addTupleElement` desde el primer hijo.

## Cobertura focal existente

`NativeSharp3T02NativeTupleTests.cpp` ya contiene la cobertura necesaria sin crear un contrato
nuevo:

- aridades de tupla 0 a 16;
- una tupla grande de aridad 32;
- una tupla anidada;
- una tupla como argumento de `Box<T>`;
- compilación O0 y ejecución del producto generado.

No se modificó la fixture ni su diagnóstico esperado.

## Evidencia

- Build focal de `nsc` y `nsc_ns3_t02_native_tuple_tests`: PASS.
  - `A:\temp\g06-tuple-marker-build.log`
- CTest exacto `^nsc\.ns3\.t02\.native-tuples$`: PASS 1/1, 0.74 s.
  - `A:\temp\g06-tuple-marker-t02-ctest.log`
- Ejecución directa del target: PASS 24 checks, exit 0.
  - `A:\temp\g06-tuple-marker-t02-direct.log`

No se ejecutó baseline ni suite completa.
