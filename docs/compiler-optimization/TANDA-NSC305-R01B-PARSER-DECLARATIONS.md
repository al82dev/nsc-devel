# NSC 3.0.5 — R01B: separación física del parser de declaraciones

## Estado

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y EJECUCIÓN PENDIENTE DEL USUARIO; SIN COMMIT PARA INTEGRACIÓN COORDINADA`.

R01B separa físicamente el parsing de declaraciones en `DeclarationParser.cpp`. No cambia la
gramática, la sintaxis aceptada, la forma del AST, el orden de consumo, los rangos, los textos o
identidades de diagnóstico, el recovery, ABI, NSMETA ni contratos públicos. Por instrucción
expresa del usuario no se compiló ni se ejecutaron pruebas; toda la evidencia de esta hoja es
estática.

## SCOPE LOCK

| Dimensión | Alcance vinculante |
|---|---|
| Objetivo | Extraer de `Parser.cpp` las familias concretas de declaraciones. |
| Naturaleza | Refactor no funcional: movimiento físico y helperización acotada para dejar cada función en 60 líneas ejecutables o menos. |
| Repositorio | Únicamente `A:\nsc-devel`. |
| Rama | `development`. |
| Fachada preservada | `Parser::parseGlobalDeclaration` permanece en `Parser.cpp` como dispatcher central. |
| Unidad nueva | `src/Parser/DeclarationParser.cpp`. |
| Tipos internos | `KnownGenericTypeScope.h` y `ParsedVariantCase.h`; no forman API instalada. |
| Prueba focal | Caso existente `TP-PAR-124`, contrato `CP-PAR-013`, en `ParserImportNamespaceModule.cpp`. |
| Contratos congelados | Sintaxis, AST, rangos, cursor, recovery, diagnósticos, API, ABI, NSMETA y CLI. |
| Fuera de alcance | Reescribir parsing, introducir sintaxis, cambiar algoritmos, paralelismo, caché o metadata. |
| Acciones prohibidas | Build, ejecución de pruebas, baseline, deploy, instalación y red. |

## Frontera física

`Parser.cpp` conserva la fachada y los dispatchers que conectan declaraciones con tipos,
miembros, statements, patrones y expresiones. En particular, `parseGlobalDeclaration` no se movió:
es la frontera estable que selecciona una declaración concreta y aplica sus modificadores comunes.

`DeclarationParser.cpp` contiene las diecisiete entradas concretas extraídas:

1. `parseCompilationUnit`;
2. `parseNamespaceMember`;
3. `parseImportDirective`;
4. `parseExtensionDeclaration`;
5. `parseNamespaceDeclaration`;
6. `parseUsingDirective`;
7. `parseInterfaceDeclaration`;
8. `parseTraitImplementationDeclaration`;
9. `parseDelegateDeclaration`;
10. `parseVariantDeclaration`;
11. `parseOpaqueTypeDeclaration`;
12. `parseClassDeclaration`;
13. `parseStructDeclaration`;
14. `parseStructBody`;
15. `parseEnumDeclaration`;
16. `parseExternCDeclaration`;
17. `parseFunctionOrGlobalVariable`.

Las entradas grandes se dividieron sólo por responsabilidades ya presentes: lectura de cabecera,
construcción del mismo nodo, cuerpo, validaciones y generación de miembros sintéticos. Los helpers
siguen siendo privados de `Parser`; no se añadió un servicio público ni estado mutable global.

## Estado compartido interno

`KnownGenericTypeScope` era un tipo local de `Parser.cpp`, pero las declaraciones genéricas lo
necesitan en ambas unidades. Se trasladó sin cambio de conducta a
`src/Parser/KnownGenericTypeScope.h`. El guard sigue instalando únicamente parámetros de tipo y
eliminando exactamente los nombres que él insertó.

`ParsedVariantCase` se movió a `src/Parser/ParsedVariantCase.h` para que `Parser.h` pueda declarar
los helpers privados de variant mediante una declaración adelantada. El tipo conserva exactamente
nombre, rango y puntero de payload. Ninguno de estos encabezados está en `include/`, se instala o
forma parte de ABI.

Tres wrappers privados permanecen definidos en `Parser.cpp`:

- `cloneDeclarationNode` delega al clonador existente `cloneParsedNode`;
- `cloneDeclarationType` delega al clonador existente `cloneParsedType`;
- `isAccessibilityModifier` delega al predicado existente `isAccessibilityToken`.

Los wrappers permiten reutilizar las implementaciones internas existentes sin duplicarlas ni
ampliar su linkage. No cambian ownership, orden de copia ni reglas de accesibilidad.

## Invariantes de equivalencia

- Cada una de las 70 definiciones de `Parser::` presentes en la unidad nueva tiene una declaración
  privada correspondiente y una sola definición entre `Parser.cpp` y `DeclarationParser.cpp`.
- Las diecisiete entradas extraídas ya no tienen una segunda definición en `Parser.cpp`.
- Se preservan condiciones, orden de tokens, llamadas a `expect`, puntos de sincronización y
  retornos tempranos.
- Se preservan `DiagnosticId`, severidad, rango y texto de todos los diagnósticos movidos.
- Se preservan kind, campos, modifiers, children, type syntax, rangos y markers de cada nodo AST.
- Se preserva el scope RAII de parámetros genéricos en alias, interfaces, traits, delegates,
  variants, opaque types, clases y structs.
- No se añadió recursión, concurrencia, caché, I/O, allocation externa ni estado global.
- No se cambió ninguna representación o condición de lectura/escritura de NSMETA.

## Límite de tamaño

El conteo estático conservador encontró 70 métodos en `DeclarationParser.cpp`. Ninguno supera 60
líneas ejecutables. Las funciones más grandes son:

| Símbolo | Líneas ejecutables conservadoras |
|---|---:|
| `parsePromotedStructParameter` | 59 |
| `parseStructBody` | 58 |
| `parseFunctionOrGlobalVariable` | 57 |
| `parseRecordDerives` | 52 |
| `parseOpaqueTypeDeclaration` | 50 |
| `parsePromotedClassParameter` | 49 |
| `parseVariantDeclaration` | 49 |
| `addVariantFactory` | 48 |
| `parseNamespaceMember` | 47 |
| `addVariantStorage` | 43 |

El resto queda por debajo de 40 líneas ejecutables en el mismo conteo. Los dos encabezados internos
tampoco contienen funciones que excedan el límite.

## Infraestructura de producto

La unidad física está registrada explícitamente en el target correcto:

- `CMakeLists.txt`: `src/Parser/DeclarationParser.cpp` pertenece a `nsc_frontend` junto a
  `Parser.cpp` y `ParserContext.cpp`.
- El archivo existe en el árbol y no depende de una inclusión textual desde otro `.cpp`.

Esto evita que el producto dependa incidentalmente del shim de pruebas.

## Infraestructura de prueba

La prueba focal reutiliza el caso existente `TP-PAR-124`; no se creó un ID nuevo y no se alteraron
catálogos. El caso conserva su contrato `CP-PAR-013` y amplía el contrafactual de alias para cubrir
también `using Buffer<T> = T*;`: debe preservar nombre, parámetro genérico, target `T` y modificador
de puntero.

La cadena de registro está completa:

1. `ParserImportNamespaceModule.cpp` define `registerUsingAliasCase` y lo invoca desde
   `registerParserImportNamespaceModule`;
2. `TestRegistry.h` declara `registerParserImportNamespaceModule`;
3. `host/src/main.cpp` lo registra incondicionalmente;
4. `tests/compiler-suite/CMakeLists.txt` incluye `ParserImportNamespaceModule.cpp` en
   `nsc_component_tests`;
5. el mismo target incluye explícitamente `ParserContext.cpp` y `DeclarationParser.cpp`;
6. `cases.jsonl`, `test-case-design.csv`, `suite-case-allocation.csv` y
   `HOST-PARSER-TEST-PLAN.md` conservan `TP-PAR-124`;
7. sin filtro de caso o componente, `main.cpp` usa `registry.all()`, por lo que el caso Required
   permanece en la ejecución predeterminada.

## Validación estática realizada

- `validate_source_registration.py`: `PASS`, exit code 0.
- Paridad declaración/definición: 70 definiciones, 70 nombres únicos, ninguna ausente o duplicada.
- Tamaño conservador: máximo 59 líneas ejecutables; ninguna función por encima de 60.
- `git diff --check` sobre los archivos versionados de R01B: exit code 0.
- Encabezados y unidad nuevos: cero trailing whitespace y cero líneas por encima de 120 columnas.
- Búsqueda del caso: catálogo, plan, asignación, definición, registro y llamada presentes.
- Ambos registros CMake están dentro de sus targets efectivos, no en variables huérfanas.

No se ejecutó compilador, linker, CTest, compiler-suite, baseline ni consumidor externo. Por lo
tanto, esta hoja no afirma que R01B compile o pase pruebas; afirma únicamente que la implementación,
la infraestructura y la auditoría estática están completas y listas para integración coordinada.

## Archivos de R01B

- `include/nativesharp/Parser/Parser.h`;
- `src/Parser/Parser.cpp`;
- `src/Parser/DeclarationParser.cpp`;
- `src/Parser/KnownGenericTypeScope.h`;
- `src/Parser/ParsedVariantCase.h`;
- `CMakeLists.txt`, sólo la línea de `DeclarationParser.cpp` dentro de `nsc_frontend`;
- `tests/compiler-suite/CMakeLists.txt`, sólo la línea de `DeclarationParser.cpp` dentro de
  `nsc_component_tests`;
- `tests/compiler-suite/host/modules/ParserImportNamespaceModule.cpp`;
- este expediente.

Los cambios concurrentes de HIR, MIR, driver y pruebas unitarias visibles en el worktree no forman
parte de R01B y no fueron editados, staged ni incluidos en un commit por esta tarea.
