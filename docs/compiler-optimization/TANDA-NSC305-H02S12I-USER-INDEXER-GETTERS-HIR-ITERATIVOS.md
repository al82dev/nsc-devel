# TANDA NSC305 H02S12I - getters de indexador de usuario HIR iterativos

## Alcance

Esta tanda elimina las aristas recursivas AST a HIR del receptor y de los índices explícitos de
un getter de indexador de usuario. Cubre tanto `receiver[i, ...]` como
`receiver?[i, ...]`. No cambia sintaxis, resolución, ABI, NSMETA, forma pública del HIR ni
contratos observables del lenguaje.

## Ruta histórica auditada

Los dos caminos históricos convergían en `lowerResolvedCall` y `lowerBoundCall`:

1. el receptor se resolvía nuevamente mediante `resolveBoundCallTarget`;
2. `lowerReceiver` bajaba el receptor de forma recursiva y materializaba temporales struct;
3. cada índice se bajaba con una llamada recursiva a `lowerExpression`, en orden fuente;
4. se aplicaban conversiones implícitas, borrows y argumentos por omisión;
5. se construía el `InstanceCall`, incluido su modo interface/base/null-conditional;
6. se fijaba `referenceResult`, se insertaba cleanup de expresión completa y, cuando correspondía,
   se cargaba el resultado de un getter `ref` usado como valor.

Una cadena `node[i, j][i, j]...` consumía por lo menos un frame del proceso por cada receptor y
podía sumar recursión adicional por cada índice.

## Diseño implementado

El clasificador reconoce sólo getters de indexador completamente resueltos. Las rutas inválidas,
un receptor `base`, una dirección prohibida y cualquier binding incompleto continúan por el camino
histórico, conservando sus diagnósticos literales.

El frame `UserIndexerGetter` conserva:

- binding del getter;
- destino efectivo obtenido por `resolveBoundCallTarget`;
- receptor y tipo efectivos, incluida desvirtualización ya existente;
- lista de sintaxis de índices en orden fuente;
- operandos HIR ya bajados;
- modo address, materialización struct y marca null-conditional.

El worklist visita primero el receptor y después cada índice. Al desenrollar, el helper
`lowerIndexerGetterWithOperands` entrega esos nodos a `lowerBoundCall`. La misma implementación
histórica sigue siendo la única dueña de conversiones, borrows, defaults, cleanup, dispatch y
ref-return. La ampliación `preloweredArguments` sólo sustituye la recursión; no reconstruye la
llamada por separado.

## Invariantes preservadas

- El receptor se evalúa antes que cualquier índice.
- Los índices se evalúan exactamente una vez y en orden fuente.
- Los índices múltiples no se reducen al último operando.
- Un receptor struct naturalmente direccionable conserva su dirección; un temporal struct se
  materializa una sola vez.
- La desvirtualización de un receiver concreto bajo interfaz permanece en
  `resolveBoundCallTarget`.
- `receiver?[...]` conserva `nullConditionalCall=true` y nunca se acepta en contexto address.
- Un getter ordinario sólo es direccionable cuando retorna por referencia.
- Los cuatro diagnósticos históricos de indexador inválido/no resuelto/no direccionable permanecen
  en el fallback original.
- Conversiones seleccionadas, borrows, ownership temporal, defaults y cleanup siguen en
  `lowerBoundCall`.
- Cada frame repite la validación noheap que antes ocurría al regresar de cada llamada recursiva.
- No cambian `memberOrdinal`, `baseReceiver`, `interfaceDispatch`, `referenceResult`, tipo de
  resultado ni la inserción de `PointerLoad` para un ref-return consumido como valor.

## Contrafactual profundo registrado

El foco `hir-user-indexer-getter-depth` construye primero un proyecto NativeSharp mínimo real con
un indexador `Node this[int row, int column]`. Del modelo compilado toma el ordinal real del getter,
el ordinal del caller y los tipos canónicos.

Después construye una cadena sintética de 32,768 getters. Cada nivel contiene dos índices, `1` y
`2`, y alterna entre indexador ordinario y null-conditional. La verificación exige:

- terminación sin recursión de la pila del proceso;
- un `InstanceCall` por nivel con el ordinal y tipo reales;
- receptor como primer hijo y ambos índices después, sin reordenamiento;
- valores y tipos canónicos de ambos índices preservados;
- alternancia exacta de `nullConditionalCall`;
- ausencia de cambios accidentales en dispatch, base, ref-result y campos históricos por omisión;
- hoja final `ThisValue` después de los 32,768 niveles.

## Infraestructura

La prueba vive en `tests/unit/ProjectOopLoweringTests.cpp`, archivo ya registrado en el CMake raíz
como target `nsc_project_oop_lowering_tests` y caso CTest `nsc.project-oop-lowering`. El selector
focal y la ruta predeterminada llaman al mismo caso. No se añade fuente, target, manifiesto ni alta
de NSC Builder, y no queda fixture huérfano.

## Gates estáticos

- Auditoría de los caminos ordinario y null-conditional históricos.
- Auditoría de materialización de receivers, interface dispatch, ref-return, conversiones,
  borrows, defaults, cleanup y noheap.
- Revisión de todos los call sites de `lowerBoundCall` tras añadir el argumento opcional final.
- Conteo conservador de funciones nuevas: máximo 45 líneas ejecutables aproximadas, por debajo
  del límite de 60.
- `git diff --check` sin errores; sólo avisos informativos LF/CRLF del checkout Windows.
- Confirmación de target raíz, selector focal y registro predeterminado.

## Validación diferida

Por instrucción de la tanda no se compiló ni se ejecutó ninguna prueba. El caso focal queda listo
para la corrida consolidada; este expediente acredita diseño, integración estática e
infraestructura, no éxito dinámico.
