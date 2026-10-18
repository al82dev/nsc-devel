# NSC 3.0.5 - R01C: extraccion fisica del parser de miembros

## Alcance

R01C separa fisicamente de `Parser.cpp` la familia que reconoce miembros de
interfaces, traits, clases, structs y tipos anidados. El producto conserva a
`Parser` como fachada y mantiene sin cambios observables la gramatica, el AST,
el avance del cursor, el recovery, los diagnosticos, ABI, NSMETA y contratos
publicos de NSC 3.0.4.

La implementacion nueva vive en `src/Parser/MemberParser.cpp`. Las entradas
historicas `parseInterfaceMember`, `parseNestedTypeDeclaration` y
`parseClassMember` permanecen en `Parser.cpp` y delegan en sus implementaciones
extraidas. La separacion no agrega sintaxis ni cambia el orden en que se
consumen tokens o se emiten diagnosticos.

## Descomposicion interna

La extraccion no traslado un monolito intacto. Los flujos grandes quedaron
divididos alrededor de estados locales explicitos:

- `InterfaceMemberState` conserva modificadores y despacho de miembros de
  interfaz o trait.
- `MemberParseState` conserva modificadores, layout, almacenamiento de frame y
  la declaracion terminada durante el despacho de miembros de clase o struct.
- `PropertyParseState` conserva accesores, backing field, inicializador y
  restricciones de propiedades.
- `IndexerParseState` conserva parametros, accesores y restricciones de
  indexadores.

Los helpers separan validacion, construccion de AST, aplicacion de metadata,
parseo de cuerpos y finalizacion. La auditoria con
`readability-function-size.StatementThreshold=60` no encontro ninguna funcion
de `MemberParser.cpp` por encima de 60 statements. El unico aviso de esa pasada
corresponde a `diagnosticIdName` en `DiagnosticId.h`, fuera del alcance de R01C.

## Frontera de compilacion

Produccion registra directamente y una sola vez:

- `src/Parser/Parser.cpp`
- `src/Parser/ParserContext.cpp`
- `src/Parser/DeclarationParser.cpp`
- `src/Parser/MemberParser.cpp`

El host `nsc_component_tests` no puede mezclar unidades compiladas con acceso
privado normal con la vista de acceso que usan sus pruebas dirigidas: en ABI
Microsoft el acceso forma parte del nombre decorado. Por eso
`ParserProductTestShim.cpp` incluye las cuatro unidades bajo la misma vista
privada de prueba y el CMake de la suite no vuelve a compilar directamente las
tres unidades extraidas. El validador de registro reconoce esas inclusiones
mediante `PRODUCT_INCLUDE_PATTERN`; no requiere una excepcion ni una lista
especial para R01C.

## Cobertura focal existente

No se creo una prueba nueva para un movimiento fisico. Las diez familias ya
existentes que ejercen el codigo movido contienen 115 casos:

- `ParserClassInterfaceModule`: TP-PAR-183..196.
- `ParserNestedDeclarationModule`: TP-PAR-197..200.
- `ParserNestedMemberDispatchModule`: TP-PAR-201..222 y TP-PAR-538.
- `ParserEventModule`: TP-PAR-223..228.
- `ParserPropertyIndexerModule`: TP-PAR-229..244.
- `ParserOperatorModule`: TP-PAR-245..250.
- `ParserOperatorConversionModule`: TP-PAR-251..256.
- `ParserConstructorDestructorModule`: TP-PAR-257..266.
- `ParserCallableDeclarationModule`: TP-PAR-267..282.
- `ParserCallableClauseModule`: TP-PAR-283..296.

La corrida focal agrego TP-PAR-537, TP-PAR-539 y TP-PAR-540 para volver a
comprobar el bloque de cuatro casos cuyo registro de infraestructura se reparo
separadamente en `6f57342e9`. El resultado conjunto fue 118/118 PASS.

## Evidencia

- `nsc_frontend`: PASS con Clang `/W4 /WX`.
- `nsc_component_tests`: compilacion y enlace PASS.
- Familias R01C y bloque TP-PAR-537..540: 118/118 PASS.
- `validate_source_registration.py`: PASS.
- `CanonicalCatalog`: 5,995 casos, 30 componentes y 682 contratos.
- Auditoria de funciones: cero cuerpos de R01C por encima de 60 statements.
- No se ejecuto baseline ni la suite completa.

Los logs persistentes estan en:

- `A:\temp\r01c-nsc-component-tests-build.log`
- `A:\temp\r01c-host-parser-118.log`
- `A:\temp\r01c-source-registration.log`
- `A:\temp\r01c-function-size-audit.log`

## Criterio de cierre

R01C queda cerrado cuando el diff final no contiene duplicados de definicion,
ambos registros de compilacion conservan su frontera correcta, las fachadas de
`Parser.cpp` siguen siendo las entradas canonicas y la evidencia focal anterior
permanece verde. Cualquier cambio posterior de algoritmos de tipos, patrones o
expresiones pertenece a R02/R03 y no forma parte de esta extraccion fisica.
