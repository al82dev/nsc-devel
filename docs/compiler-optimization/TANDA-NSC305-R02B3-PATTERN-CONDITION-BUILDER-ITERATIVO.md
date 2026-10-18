# NSC 3.0.5 — R02B3 condition builder de patterns iterativo

## SCOPE LOCK

- Objetivo: eliminar la recursión directa, indirecta y de closures del builder de condiciones de
  patterns.
- Naturaleza: refactor algorítmico interno.
- Símbolos autorizados: `makePatternCondition`, sus seis helpers recursivos históricos y el helper
  privado que los sustituye.
- Contratos inmutables: AST exacto, asociación izquierda, orden fuente, cantidad y orden de
  invocaciones de `PatternExpressionFactory`, auxiliares, rangos, operadores, índices de listas,
  tratamiento de rest y payload de Variant.
- Archivos productivos: `include/nativesharp/Parser/Parser.h` y
  `src/Parser/PatternParser.cpp`.
- Protegido: gramática R02B1, clasificación/expansión R02B2, lowering R02B4, emisión de bindings y
  rutas match/conditional R02B5, diagnósticos, recovery, AST público, sintaxis, ABI, NSMETA, HIR,
  MIR y Driver.
- Criterio de cierre: cero ciclo recursivo del builder y de sus fuentes derivadas; funciones bajo
  60 statements; TP-PAR-544 y regresión focal PASS; infraestructura, catálogo, diff y tracking
  consistentes.

## Causa

El call graph histórico era un ciclo indirecto:

```text
makePatternCondition
  -> makeStructPatternCondition   -> makePatternCondition(child)
  -> makeVariantPatternCondition  -> makePatternCondition(payload)
  -> makeLogicalPatternCondition  -> makePatternCondition(child)
  -> makeListPatternCondition     -> makePatternCondition(element)
  -> makeNotPatternCondition      -> makePatternCondition(child)
```

El recorrido visitaba `O(N)` patterns, pero trasladaba la profundidad completa a la pila nativa.
Existía una segunda recursión menos visible: los factories de member, payload e index capturaban
el factory padre, de modo que invocar el source de una hoja profunda volvía a recorrer la cadena
mediante llamadas C++ anidadas. Convertir únicamente el DFS no habría resuelto el desbordamiento.

Los seis helpers retirados no tenían consumidores independientes: cada uno aparecía solamente en
su definición, declaración privada y dispatch de `makePatternCondition`. La fachada permanece
porque la consumen ExpressionParser, lowering, emisión y las pruebas productivas.

## Diseño

`PatternConditionBuilder` usa tres estructuras heap-backed:

1. una worklist LIFO de fases `Build`, finishers y continuaciones Struct/List;
2. una pila de valores `AstNode*` terminados;
3. un catálogo persistente de source plans identificado por índices estables.

Cada source plan referencia su padre y una transformación:

- `Member`: agrega un acceso de campo;
- `VariantPayload`: agrega el extractor `__nsc_variant_extract_*` y su auxiliar;
- `Index`: agrega un índice clonando el prototype histórico.

`evaluateSource` recorre los índices hacia la raíz, invoca una vez el factory raíz y aplica los
steps en orden inverso. No existen closures que invoquen otra closure derivada. El costo es lineal
en la ruta materializada y proporcional al AST que el contrato exige.

## Equivalencia por kind

- Wildcard, Binding y Positional producen `true` sin visitar hijos.
- Tuple produce `false` sin visitar hijos.
- Literal conserva `source == value`.
- Range conserva `source >= lower && source <|<= upper` y dos invocaciones del source.
- Relational conserva el operador textual y una invocación del source.
- Struct inicia en `true`, visita campos en orden y construye una cadena `&&` asociada a la
  izquierda.
- Variant construye primero `IsCase()`, ignora payloads posteriores al primero como antes y combina
  el primer payload con `&&`.
- Or/And conservan orden fuente y asociación izquierda sin introducir un literal identidad.
- List conserva `Length == fixedCount` o `Length >= fixedCount`, omite rest, usa índices directos
  antes de rest e índices `Length - fromEnd` después de rest.
- Not conserva un único Unary `!` alrededor de la condición hija.

No se agregan guards para patrones estructuralmente imposibles que antes accedían `front()`; el
refactor no redefine recovery ni política de AST inválido.

## Contrafactual TP-PAR-544

El proceso aislado valida:

- 32,768 patterns Not anidados y una cadena Unary exacta;
- 32,768 Struct anidados, una cadena `true &&` exacta, un source plan Member de igual profundidad
  y una sola invocación del factory raíz;
- un And de anchura 32,768 y asociación izquierda exacta;
- `Some([1, .., 2])`, con cinco invocaciones source y la topología AST histórica exacta. El
  recorrido plano observa cinco extractores Variant y tres marcadores `list-pattern-length`
  porque `IndexExpression` expone el índice simultáneamente mediante `second()` y `children()`;
  esta doble arista ya formaba parte del producto anterior y no implica evaluaciones source
  adicionales.

Los patterns profundos se dejan al proceso aislado hasta su salida para no confundir el destructor
recursivo de `vector<Pattern>` con el algoritmo probado.

## Infraestructura

TP-PAR-544 se registra en el mismo cambio en:

- `tests/compiler-suite/CMakeLists.txt`;
- `tests/compiler-suite/host/include/nsc_suite/TestRegistry.h`;
- `tests/compiler-suite/host/src/main.cpp`;
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`;
- `docs/compiler-testing-reengineering/inventory/suite-case-allocation.csv`;
- `tests/compiler-suite/manifest/cases.jsonl`.

Usa el contrato existente CP-PAR-042, el componente `Host.Parser`, perfil `NIGHTLY` y target
`nsc_component_tests`. `PatternParser.cpp` continúa entrando a la suite sólo mediante su shim.

## Estado de validación

- Primera build focal `nsc` después de la migración productiva: PASS; log
  `A:\temp\r02b3-build-nsc-first.log`.
- Build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS; log
  `A:\temp\r02b3-build-suite-three-targets.log`.
- TP-PAR-544: PASS 1/1, exit `0`, 1,371,597 us; el hijo aislado verificó por separado las tres
  invariantes a profundidad/anchura 32,768; log `A:\temp\r02b3-tp-par-544-final.log`.
- Regresión TP-PAR-349..410 + TP-PAR-525..540: PASS 78/78, exit `0`; log
  `A:\temp\r02b3-pattern-regression-78.log`.
- `validate_source_registration.py`: PASS, exit `0`; log
  `A:\temp\r02b3-source-registration.log`.
- `CanonicalCatalog`: PASS, 5,999 casos, 30 componentes y 682 contratos; TP-PAR-544 se descubre
  como `Host.Parser/NIGHTLY/nsc_component_tests`; log
  `A:\temp\r02b3-canonical-catalog.log`.
- Gate conservador de tamaño: producción máximo 34 líneas físicas/27 ejecutables aproximadas en
  `continueList`; contrafactual máximo 24/17 en `testWideLogicalCondition`; todas las funciones
  quedan por debajo de 60.
- Los seis helpers recursivos históricos tienen cero declaraciones, definiciones o llamadas; el
  registro del módulo aparece exactamente una vez en CMake, TestRegistry y `main.cpp`.
- Tracking externo: 63→62 hojas pendientes; preserva completas M02N–M02V y las demás hojas
  concurrentes.
- Índice: auditado con exactamente las diez rutas R02B3; M02N y las pruebas ajenas quedaron
  fuera.
- Cierre técnico: completo en un commit local aislado después de todos los gates.
