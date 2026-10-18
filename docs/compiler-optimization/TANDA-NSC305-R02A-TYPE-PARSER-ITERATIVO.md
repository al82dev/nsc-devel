# Tanda NSC 3.0.5 R02A — tipos iterativos y lookahead lineal

## Estado

IMPLEMENTACIÓN, INFRAESTRUCTURA Y VALIDACIÓN FOCAL COMPLETAS.

La primera compilación compartida compiló todos los fuentes Parser modificados y enlazó `nsc_frontend.lib`.
El cierre posterior construyó `nsc` y los tres targets focales de compiler-suite, ejecutó el contrafactual y
los 68 casos históricos, y revalidó catálogo y registro de fuentes.

## SCOPE LOCK

- Objetivo autorizado: sustituir la recursión de parseo y escaneo de tipos por máquinas iterativas, y retirar
  el reescaneo cuadrático de argumentos genéricos nominales.
- Naturaleza: refactor interno y optimización algorítmica de NSC 3.0.5.
- Contratos preservados: tokens consumidos, cursor, AST, rangos, ownership, referencias, lifetimes, `dyn`,
  `some`, tipos función, tuplas, genéricos, constantes genéricas, packs, modificadores, diagnostics y recovery.
- Revisión base: `0dd1f6301d5d`, rama `development`.
- Archivos productivos autorizados: `Parser.h`, `TypeParser.cpp`, `GenericSyntaxParser.cpp`.
- Archivo de prueba autorizado: `ParserTypeIterationCounterfactualModule.cpp`.
- Protegidos: sintaxis, semántica, ABI, NSMETA, Driver, HIR, MIR, LLVM, NSLink, pruebas y expectativas ajenas.
- Prohibido: baseline, batería completa, cambios de fixtures para ocultar fallos, recursión nueva y publicación
  parcial de una prueba sin su infraestructura final.
- Terminación: cero aristas recursivas en el parse/scan de tipos; funciones tocadas bajo el umbral de 60
  sentencias; contrafactual aislado de profundidad 32,768; build y pruebas focales PASS; infraestructura,
  tracking y commit aislado completos.

## Causa y frontera

La ruta anterior tenía dos familias de recursión sobre la pila nativa:

1. `scanTypeAt` se llamaba directa o indirectamente desde calificadores dinámicos, parámetros y resultados de
   función, errores tipados y elementos de tupla.
2. `parseType` se llamaba indirectamente desde parámetros, resultado y error de función, elementos de tupla y
   argumentos genéricos de un tipo nominal.

Además, `parseTypeArgument` evaluaba primero `canStartTypeAt(index_)` incluso cuando el token actual era un
identificador. En `Box<Box<...>>`, cada nivel volvía a escanear el sufijo completo: suma
`n + (n-1) + ... + 1`, por lo tanto tiempo cuadrático. La condición era lógicamente equivalente a comprobar
primero el identificador; invertir el orden del cortocircuito conserva el resultado y evita el escaneo.

## Diseño implementado

### Scanner

`scanTypeAt` mantiene un cursor único y una pila explícita de tareas:

- `Begin`;
- `FunctionParameters`;
- `FunctionAfterParameter`;
- `FunctionAfterResult`;
- `FunctionAfterError`;
- `TupleElements`;
- `TupleAfterElement`.

Los helpers existentes de identidad nominal, cierre genérico balanceado, carriers, effects y suffixes siguen
siendo los productores canónicos. Ninguna tarea llama nuevamente a `scanTypeAt`; añade continuaciones y deja
que el bucle exterior las consuma en orden LIFO.

### Parser

`parseTypeImpl` usa frames con estas fases:

- `Begin`;
- `FunctionParameters`, `FunctionParameterReady`, `FunctionResultReady`, `FunctionErrorReady`;
- `TupleElements`, `TupleElementReady`;
- `NamedArguments`, `NamedArgumentReady`.

El resultado de un hijo viaja como un evento que puede contener un puntero nulo. La distinción es necesaria:
función y tupla propagan el fallo del hijo, mientras un argumento genérico fallido conserva el comportamiento
histórico de descartar la lista incompleta y devolver el tipo nominal exterior con sus diagnostics.

El cierre combinado `>>` y `>>=` continúa usando `pendingGenericGreater_` y
`pendingGenericEqualToken_`. Los argumentos sólo se adjuntan al AST cuando el cierre completo fue aceptado,
igual que en la implementación anterior.

## Complejidad

- Scanner de función/tupla: `O(tokens)` tiempo y `O(depth)` memoria explícita.
- Parser de función/tupla/genérico: `O(tokens + nodes)` tiempo y `O(depth)` frames.
- Cadena nominal `Box<Box<...>>`: cada identificador evita `canStartTypeAt`; el parseo es lineal en tokens y
  nodos.
- Cierre genérico combinado: cada token se consume una vez; los cierres pendientes son un contador.
- La memoria de profundidad reside en vectores del heap, no en frames de la pila nativa.

No se introduce recursión mutua alternativa ni un segundo árbol intermedio.

## Contrafactual profundo preparado

`TP-PAR-541` se diseñó como caso `NIGHTLY` aislado en un proceso nuevo. Construye y valida por separado:

1. 32,768 niveles de `Box<...>`;
2. 32,768 resultados anidados `fn*()->...`;
3. 32,768 tuplas unitarias escritas con coma `(T,)`.

Para cada familia verifica:

- que `scanTypeAt` termina y deja el cursor exactamente en EOF;
- que `parseType` termina y consume exactamente hasta EOF;
- que la forma AST conserva exactamente 32,768 niveles y termina en `int`;
- que no existe ningún diagnóstico.

El caso quedó registrado en la misma tanda mediante:

- declaración en `TestRegistry.h`;
- llamada default en `host/src/main.cpp`;
- fuente/shim aplicable en el CMake raíz y `tests/compiler-suite/CMakeLists.txt`;
- fila de diseño;
- asignación;
- `cases.jsonl`;
- validación de `CanonicalCatalog` y `validate_source_registration.py`.

`CanonicalCatalog` lo descubre como el caso 5,996 y Builder lo enruta al ejecutor
`nsc_component_tests`. La ruta default registra el módulo y el foco exacto es `--case TP-PAR-541`.

## Auditoría estática inicial

- `TypeParser.cpp` ya no contiene llamadas a `parseType()` en las rutas function/tuple/named-generic.
- `scanTypeAt` ya no se llama a sí misma ni desde continuaciones function/tuple.
- Los consumidores externos de `parseTypeArgumentList` se conservan para expresiones y allocations; entran al
  motor iterativo y no forman un ciclo de recursión.
- El orden del guard genérico cambió sin cambiar su tabla de verdad.
- `git diff --check` no reportó errores de whitespace en R02A.
- La infraestructura de prueba quedó completa y `validate_source_registration.py` terminó con exit `0`.

## Validación ejecutada

- Build `nsc`: PASS, exit `0`.
- Reconfiguración compiler-suite: PASS, exit `0`.
- Build `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS, 293/293, exit `0`.
- TP-PAR-541 aislado: PASS, exit `0`, 1.557 segundos.
- Regresión TP-PAR-018..069 y TP-PAR-094..109: 68/68 PASS, exit `0`.
- `validate_source_registration.py`: PASS, exit `0`.
- `CanonicalCatalog`: 5,996 casos; TP-PAR-541 descubierto como
  `Host.Parser/NIGHTLY/nsc_component_tests`.
- Auditoría conservadora de tamaño: máximo tocado de 51 líneas físicas y 20 semicolons; ninguna función R02A
  supera 60 sentencias ejecutables.
- Búsqueda estática: cero `parseType()` internas en `TypeParser.cpp` y cero autorrecursión de `scanTypeAt`.
- `git diff --check`: PASS.

## Evidencia

- `A:\temp\m02f-build-nsc.log`;
- `A:\temp\r02a-build-nsc.log`;
- `A:\temp\r02a-suite-configure.log`;
- `A:\temp\r02a-suite-three-targets-build.log`;
- `A:\temp\r02a-tp-par-541.log`;
- `A:\temp\r02a-host-parser-types-68.log`;
- `A:\temp\r02a-source-registration.log`;
- `A:\temp\r02a-canonical-catalog.log`.
