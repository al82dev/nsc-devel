# TANDA NSC305 H02S12C - property getters HIR iterativos

## Estado de este expediente

Este documento registra el cambio aislado H02S12C. El cierre es de implementación
y auditoría estática: por instrucción de la tanda no se ejecutaron compilaciones ni
pruebas. La prueba focal quedó registrada en la infraestructura existente para la
corrida consolidada posterior.

## Scope lock

H02S12C migra únicamente la arista recursiva AST a HIR del receptor de un getter
de propiedad de instancia. Abarca las formas semánticamente resueltas escritas como
identificador, miembro y miembro null-conditional.

Quedan fuera de alcance:

- getters de indexador, que conservan su leaf propio;
- propiedades de extensión mediante funciones de namespace;
- setters, eventos, operadores, conversiones y llamadas ordinarias;
- sintaxis, parser, resolución semántica y selección de overload;
- representación pública o serializada de AST, HIR, MIR, ABI o NSMETA;
- Driver, Builder 3, publicación de artefactos y tracking de otras tandas;
- cambios de diagnósticos, evaluación, cleanup, efectos o contratos `noheap`.

## Ruta histórica auditada

La ruta histórica detectaba el accessor en `lowerExpressionImpl`, validaba el
contexto de dirección y delegaba a `lowerResolvedCall`. Éste llamaba a
`lowerBoundCall`, que resolvía el destino efectivo y finalmente pedía a
`lowerReceiver` bajar el receptor mediante otra llamada a `lowerExpression`.

En una cadena como `node.Next.Next...Next`, cada getter retenía una continuación
C++ mientras se bajaba el getter interior. El costo era lineal, pero la profundidad
de la pila nativa también era lineal y una entrada válida suficientemente profunda
podía desbordarla.

## Diseño H02S12C

La máquina de expresiones reconoce un `PropertyGetter` sólo cuando el binding:

1. está resuelto a un miembro válido;
2. corresponde a un getter de propiedad y no de indexador;
3. es de instancia;
4. tiene un receptor explícito distinto de `base`;
5. tiene tipo de receptor conocido;
6. admite el contexto de dirección solicitado.

Las formas estáticas, implícitas, `base` o inválidas no tienen la arista recursiva
que H02S12C debe eliminar, o necesitan conservar un diagnóstico histórico previo;
por ello continúan por el fallback existente.

Cada frame conserva:

- el binding semántico original del getter;
- el destino efectivo de llamada;
- el receptor y su tipo efectivos;
- el contexto value/address del resultado;
- si el receptor struct necesita `TemporaryAddress`;
- si la llamada es null-conditional;
- si el análisis de flujo ya probó el receptor no nulo.

La máquina desciende al receptor efectivo, lo baja una sola vez y, al desenrollar,
materializa el temporal struct si corresponde y completa la llamada con ese receptor
ya producido. No se repite ningún recorrido del AST.

## Destino efectivo y desvirtualización

`lowerBoundCall` ya contenía una desvirtualización deliberadamente estrecha: una
conversión a interfaz cuyo operando es un objeto concreto recién construido puede
resolverse al slot de implementación antes del lowering.

H02S12C extrae esa decisión a `resolveBoundCallTarget`. Tanto la ruta histórica
como el frame iterativo consultan el mismo helper. Así, el worklist visita el mismo
receptor que la llamada va a consumir y conserva:

- ordinal final del miembro;
- receptor después de retirar el cast estrecho cuando aplica;
- tipo concreto del receptor;
- indicador de dispatch de interfaz;
- `targetTypeOrdinal` cuando el dispatch sigue siendo dinámico.

La extracción es movimiento semánticamente neutro; las condiciones de
desvirtualización no cambiaron.

## Receptores struct y orden de evaluación

`lowerReceiver` distinguía dos casos para un receptor struct:

- almacenamiento naturalmente direccionable: bajar en contexto address;
- valor temporal: bajar en contexto value y envolver en `TemporaryAddress`.

El frame reproduce exactamente esa bifurcación con `isNaturallyAddressable`.
Esto también cubre una propiedad que devuelve un struct: un getter ordinario no es
almacenamiento y se materializa; un getter `ref` sí puede alimentar directamente el
siguiente receptor.

El orden observable permanece de adentro hacia afuera. Primero se produce el
receptor completo, después se construye su getter exterior. No se adelantan calls,
conversiones, cleanup ni materialización.

## Contexto address y diagnósticos

Una propiedad ordinaria no-reference en contexto address conserva el error exacto:

`Stage 38 properties are semantic accessor calls and are not addressable`

Un acceso null-conditional en contexto address conserva:

`a null-conditional access produces a value, not addressable storage`

Esos casos no se clasifican como frame: entran al fallback histórico para que el
diagnóstico ocurra antes de visitar el receptor, igual que antes. Un getter `ref`
ordinario sí se clasifica y conserva el resultado address sin `PointerLoad`.

## Null-conditional y flujo

Para `receiver?.Property`, el frame conserva la consulta de
`flowProvenNonNull` sobre el receptor sintáctico original.

- Sin prueba de flujo, la llamada mantiene `nullConditionalCall = true`.
- Con prueba de flujo, la llamada directa mantiene `nullConditionalCall = false`
  y la marca `flow-nonnull` en `stringValue`.

No se cambia el tipo inferido ni la forma HIR establecida para ninguno de los dos
casos.

## Política noheap

La recursión histórica aplicaba las comprobaciones comunes al terminar cada getter
anidado. Si sólo se comprobara la raíz después del worklist, un getter interior
non-noheap podría quedar oculto por un getter exterior permitido.

`validateNoHeapDirectCall` reproduce el chequeo directo para cada frame completado:
usa los efectos del callable activo, el símbolo seleccionado original y conserva
el texto exacto del diagnóstico `NS3030`. El rango de error se fija en el getter
más interno que falla antes de vaciar la pila explícita.

## Prueba focal registrada

La infraestructura existente `ProjectOopLoweringTests` incorpora el selector:

`NSC_PROJECT_OOP_LOWERING_FOCUS=hir-property-getter-depth`

La prueba:

1. construye un proyecto mínimo con la propiedad `Node Next`;
2. obtiene del modelo real los ordinales del getter y del caller;
3. arma 32,768 accesos de propiedad semánticamente resueltos;
4. baja la raíz con el worklist HIR;
5. comprueba 32,768 nodos `InstanceCall` en orden;
6. comprueba ordinal, tipo, cardinalidad del receptor y flags de dispatch;
7. exige que el terminal continúe siendo `ThisValue`;
8. destruye el árbol resultante de forma iterativa.

No se creó ejecutable, source ni target nuevo. El caso vive en el harness ya
declarado, por lo que no requiere cambios de CMake ni de Builder 3.

## Complejidad y uso de pila

Para una cadena de profundidad N:

- tiempo: O(N);
- memoria explícita: O(N);
- pila nativa atribuible a la arista getter-receptor: O(1);
- cada receptor se baja exactamente una vez;
- cada getter se materializa exactamente una vez.

No se introduce recursión nueva ni búsqueda cuadrática.

## Archivos de la tanda

- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `src/Frontend/HIR/HirCallLowering.cpp`
- `src/Frontend/HIR/HirExpressionMachine.cpp`
- `tests/unit/ProjectOopLoweringTests.cpp`
- este expediente

## Gates estáticos

- arquitectura y commits H02S12A, H02S12B y H02S12D auditados;
- diff completo de los archivos autorizados inspeccionado;
- una producción y un consumo del frame `PropertyGetter`;
- getters de indexador y extensión excluidos explícitamente;
- condiciones históricas de desvirtualización preservadas en helper único;
- receptores struct, ref-return y contexto address auditados;
- null-conditional y `flow-nonnull` auditados;
- `noheap` validado por cada frame anidado;
- helpers nuevos por debajo de 60 líneas ejecutables;
- prueba focal registrada en el harness existente;
- ninguna fuente nueva que requiera alta de infraestructura;
- contratos públicos/serializados sin cambios;
- Driver, MIR, Builder 3 y tests concurrentes fuera del diff;
- `git diff --check` sin errores.

## Validación aplazada

No se compiló ni se ejecutó la prueba focal por instrucción expresa. Este expediente
demuestra implementación y revisión estática, no paridad dinámica. La validación
corresponde a la corrida consolidada posterior.
