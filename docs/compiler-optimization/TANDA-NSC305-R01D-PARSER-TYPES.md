# NSC 3.0.5 - R01D: extraccion fisica del parser de tipos

## Proposito

R01D separa fisicamente de `Parser.cpp` la familia que reconoce, anticipa y construye tipos.
El producto conserva `Parser` como fachada y mantiene sin cambios observables la gramatica, el
AST, el cursor, el recovery, los diagnosticos, ABI, NSMETA y los contratos congelados de 3.0.4.

La nueva unidad es `src/Parser/TypeParser.cpp`. `Parser.cpp` conserva la entrada privada
`parseType()` y delega en `parseTypeImpl()`. Esta tanda no agrega sintaxis, no cambia reglas de
aceptacion y no convierte a iterativos los recorridos recursivos existentes; R02 conserva esa
responsabilidad independiente.

## SCOPE LOCK

- Objetivo: extraer fisicamente el parser de tipos mediante movimiento y descomposicion interna.
- Naturaleza: refactor no funcional.
- Revision de trabajo: rama `development`, despues de R01C.
- Archivos funcionales autorizados: `Parser.h`, `Parser.cpp`, `TypeParser.cpp`, CMake raiz y shim
  privado de compiler-suite.
- Comportamiento protegido: tokens consumidos, precedencia, recovery, rangos, diagnosticos, AST,
  API, ABI, NSMETA y artefactos emitidos.
- Pruebas protegidas: todos los casos Host.Parser existentes; no se modifican para hacer pasar el
  refactor.
- Acciones prohibidas: baseline, suite completa, sintaxis nueva, correcciones semanticas, cambios
  HIR/MIR, cambios de metadata y cambios en consumidores.
- Archivo ajeno protegido: `tests/unit/ProjectMethodBodyModelTests.cpp`.
- Criterio: ambos targets focales compilan, los 68 casos de tipos pasan, la infraestructura queda
  completa, no hay duplicados y ninguna funcion R01D supera 60 statements.

## Inventario extraido

La unidad contiene 50 definiciones de `Parser`. Se agrupan en:

1. anticipacion de tipos y ownership;
2. anticipacion de funciones, tuplas, nombres, genericos y sufijos;
3. nombre calificado;
4. calificadores ownership, referencia, `dyn`, `some` y prefijos diferidos;
5. tipos de funcion y sus parametros, efectos, error y convencion;
6. tuplas;
7. tipos nombrados y argumentos genericos;
8. modificadores de puntero y sus calificadores;
9. arreglos flexibles, dinamicos, por rango y fijos;
10. lifetime opcional y tipos de elemento de asignacion.

`parseTypeImpl()` quedo como dispatcher. El monolito original se descompuso por fronteras ya
presentes en el algoritmo. `TypeParseState` conserva los calificadores del tipo y
`FunctionTypeParseState` conserva carrier, convencion, nullability y lifetime de una funcion.
Los estados son privados y no alteran layout publico ni metadata.

## Conservacion algoritmica

La extraccion conserva:

- el orden ownership, `ref`, `dyn`, `some` y builtin diferido;
- la decision entre funcion, tupla y tipo nombrado;
- la normalizacion de alias enteros;
- el orden de argumentos genericos y expansion de packs;
- nullability, alignment, address space y provenance de punteros;
- arreglos flexibles, heap arrays, rango rectangular y dimensiones fijas;
- mensajes, identificadores y rangos de cada diagnostico;
- el cursor final tanto en aceptacion como en recovery.

La division de `scanTypeAt` conserva el comportamiento historico ante un calificador de puntero
incompleto: el cursor llega al final y el lookahead termina igual que antes. No se introdujo una
validacion nueva ni un rechazo adicional.

La unidad mantiene la recursion preexistente mediante `parseType()` para tipos anidados. R01D no
afirma eliminarla ni modificar su profundidad. La migracion iterativa pertenece a R02.

## Frontera de compilacion

### Producto

- `src/Parser/TypeParser.cpp` aparece exactamente una vez en `nsc_frontend`.
- `Parser.cpp` conserva la fachada `parseType()`.
- No se agrega un target nuevo.

### Compiler suite

- El CMake de compiler-suite no compila `TypeParser.cpp` directamente.
- `ParserProductTestShim.cpp` incluye `Parser.cpp`, `ParserContext.cpp`,
  `DeclarationParser.cpp`, `MemberParser.cpp` y `TypeParser.cpp` bajo una sola vista privada.
- El helper local de Language 3 se llama `reportDeferredTypeLanguage3` para evitar una colision de
  enlace interno cuando el shim combina unidades que produccion compila por separado.
- `validate_source_registration.py` reconoce la inclusion y termino con codigo cero.

R01D no agrega una prueba, fixture, caso, target ni `add_test`; por eso no requiere alta nueva en
design, allocation, manifest, catalogo o Builder. La cobertura existente se reutiliza sin cambios.

## Cobertura focal existente

Se ejecutaron 68 casos exactos:

- `ParserLookaheadModule`: TP-PAR-018..035;
- `ParserTypeFoundationModule`: TP-PAR-036..050;
- `ParserFunctionTypeContinuationModule`: TP-PAR-051..059;
- `ParserTypeModifierContinuationModule`: TP-PAR-060..069;
- `ParserTypeArgumentContinuationModule`: TP-PAR-094..100;
- `ParserGenericLookaheadContinuationModule`: TP-PAR-101..109.

El conjunto cubre lookahead, builtins, nombres, ownership, traits, tuplas, funciones, calling
conventions, efectos, genericos, punteros, calificadores, arreglos y recovery. Resultado: 68/68
PASS, exit 0.

## Evidencia

- Build `nsc_frontend`: PASS con warnings tratados como errores.
- Build y enlace `nsc_component_tests`: PASS.
- Host.Parser dirigido: 68/68 PASS.
- `validate_source_registration.py`: PASS, exit 0.
- `CanonicalCatalog`: 5,995 casos, 30 componentes y 682 contratos.
- `clang-tidy readability-function-size`: cero avisos en `TypeParser.cpp` con threshold 60.
- Aviso ajeno conservado: `diagnosticIdName` tiene 77 statements en `DiagnosticId.h`.
- Definiciones duplicadas entre `Parser.cpp` y `TypeParser.cpp`: cero.
- Lineas nuevas por encima de 120 caracteres: cero.
- `git diff --check`: PASS.

Logs persistentes:

- `A:\temp\r01d-nsc-frontend-build.log`;
- `A:\temp\r01d-nsc-component-tests-build.log`;
- `A:\temp\r01d-nsc-component-tests-build-retry.log`;
- `A:\temp\r01d-host-parser-types-68.log`;
- `A:\temp\r01d-source-registration.log`;
- `A:\temp\r01d-canonical-catalog.log`;
- `A:\temp\r01d-function-size-audit-with-flags.log`.

No se ejecuto baseline ni suite completa.

## Criterio de cierre

R01D queda listo para integracion cuando el indice contiene exclusivamente sus archivos, el hunk
ajeno de MIR no entra al commit, los registros producto/shim aparecen una sola vez y la evidencia
anterior permanece verde. El tracking se reduce de 63 a 62 hojas solo despues del commit local.
