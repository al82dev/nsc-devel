# Registro de contradicciones

| ID | Estado | Fuente A | Fuente B | Discrepancia | Impacto | Acción necesaria |
|---|---|---|---|---|---|---|

| `CON-0001` | ABIERTA | `Language3Surface`, `CompatibilitySemantics`, `IntegrationQuality` y `ReleaseSemantics` | Decisión posterior del usuario: el compilador no debe rechazar por edición | La implementación vigente conserva etiquetas, gates y metadata ligados a 3.x | Puede convertir historia interna en rechazo semántico | Separar en una fase funcional autorizada; Fase 1 sólo preserva evidencia |
| `CON-0002` | ABIERTA | `AbiSemantics`, `ErrorSemantics`, `ExpressionErgonomics`, `SumTypeSemantics` | Decisión posterior del usuario de no forzar `unsafe` y revisar casts explícitos | Los helpers vigentes conservan contexto y operaciones `unsafe` | Afecta superficie y compatibilidad del lenguaje | Diseñar cambio sólo bajo SCOPE LOCK funcional posterior |
| `CON-0003` | ABIERTA | `ModuleSemantics::moduleGraphResolvable` | Lineamiento general que prohíbe recursión salvo necesidad demostrada | El grafo usa DFS recursivo proporcional a la profundidad | Riesgo de stack y divergencia normativa | Reemplazo iterativo requiere autorización funcional; conservar caso NIGHTLY |
| `CON-0004` | ABIERTA | `SemanticAnalyzer`, `ExpressionInfo` y diagnósticos de Sema | Decisión posterior del usuario: neutralidad de edición y no forzar features | La implementación vigente conserva aliases 2.0, textos `NativeSharp 2.0`, ownership y requisitos históricos | Las pruebas nuevas deben caracterizar el presente sin convertirlo en política futura | Resolver sólo en una fase funcional autorizada |
| `CON-0005` | ABIERTA | `EffectContext::allows`, `SemanticDumper`, `SymbolDumper`, `statementPreventsFallthrough` y `containsBreakTargetingLoop` | Lineamiento general que prohíbe recursión salvo necesidad demostrada | Cinco recorridos vigentes usan recursión sin límite o visited set | Profundidad hostil puede agotar stack o ciclar | Diseñar casos NIGHTLY; una reescritura iterativa queda fuera de Fase 1 |
| `CON-0006` | ABIERTA | `SemanticAnalyzer` single-file | Frontend/proyecto contienen soporte posterior para clases, enum underlying types, instance context, placement e inline assembly | La ruta auditada rechaza explícitamente esas capacidades | Un test sin frontera de pipeline confundiría rechazo local con ausencia global de feature | Mantener suites y contratos separados por ruta propietaria |

| `CON-0007` | ABIERTA | `GlobPattern::matches`, `GlobPattern::matchSegment`, `visitProject`, replacement DFS y `SimpleXmlParser::parseElement` | Lineamiento general que prohíbe recursión salvo necesidad demostrada | Compilation conserva matching, orden de grafo y XML recursivos; sólo XML limita profundidad a 128 | Globs o grafos hostiles pueden agotar stack y la implementación contradice la política vigente | Caracterizar con casos NIGHTLY; reemplazo iterativo requiere SCOPE LOCK funcional posterior |

| `CON-0008` | ABIERTA | `canMaterializeContractType`, `collectMaterializedContractTypes`, emisión de tipos anónimos, clausura de callees y `JsonParser::parseValue`/`parseObject`/`parseArray` | Lineamiento general que prohíbe recursión salvo necesidad demostrada | Driver conserva recorridos recursivos de metadata, materialización de fuente, clausura de llamadas y análisis JSON; algunos recorridos están acotados por el formato y otros no tienen una cota explícita | Entradas profundas o clausuras hostiles pueden agotar stack y contradicen la política vigente | Caracterizar con casos NIGHTLY y conservar el comportamiento; una reescritura iterativa requiere SCOPE LOCK funcional posterior |
| `CON-0009` | ABIERTA | Ayuda e identidad del Driver, verificadores de instalación, manifiestos de release/migración y artefactos por etapas | Decisión posterior del usuario: compatibilidad universal y neutralidad frente a ediciones internas | Driver conserva contratos operativos, textos y artefactos ligados a 2.0, 3.0, stages y versiones concretas | Una suite nueva podría congelar residuos históricos como gates futuros del lenguaje o del producto | Caracterizar los artefactos vigentes sin convertirlos en requisitos semánticos futuros; resolver la neutralidad sólo bajo una fase funcional autorizada |
| `CON-0010` | ABIERTA | `validRenameIdentifier` mantiene una tabla privada de keywords en `Host.LanguageService` | Lexer y parser son la autoridad de la superficie léxica del compilador | La lista editorial puede aceptar una keyword nueva o rechazar un identificador retirado aunque el compilador decida lo contrario | Rename puede divergir del lenguaje sin que falle el análisis semántico principal | Diseñar un gate contractual contra la autoridad léxica; eliminar la duplicación requiere SCOPE LOCK funcional posterior |

Estas contradicciones se registran sin corregirlas porque la Fase 1 es exclusivamente documental.

<!-- BEGIN TANDA-12A-SELFHOST-CONTRADICTIONS -->
## Contradicciones self-host de la Tanda 12A

| ID | Estado | Fuente A | Fuente B | Discrepancia | Impacto | Acción necesaria |
|---|---|---|---|---|---|---|
| `CON-0011` | ABIERTA | `README.md`, `ProjectSystem.ns`, `ModuleSet.ns`, `Program.ns` | Decisión posterior del usuario: neutralidad universal frente a ediciones internas | El self-host conserva `BootstrapProfile/2.0`, `NativeSharp/2.0`, NSMETA/1/2, `language=2.0` y texto `nsc-next 2.0.0` | Una suite nueva podría congelar residuos históricos como política futura | Caracterizar el comportamiento vigente sin convertirlo en requisito futuro; corrección funcional fuera de Fase 1 |
| `CON-0012` | ABIERTA | Recorridos en Parser, Sema, IR, SourceGraph, SyntaxDumper y LlvmBackend | Lineamiento general que prohíbe recursión salvo necesidad demostrada | Existen descensos y clausuras recursivas, algunas sin cota explícita | Profundidad hostil puede agotar stack; cambiar algoritmo alteraría producto | Diseñar casos NIGHTLY de profundidad; cualquier reescritura iterativa exige otra fase y SCOPE LOCK |
| `CON-0013` | ABIERTA | Comentario de `LlvmBackend.ns:139` | Toolchain actual exige componentes LLVM estáticos y no depender de `LLVM-C.dll` monolítico | El comentario histórico afirma que toda dependencia cruza `LLVM-C.dll` | Puede confundir documentación de bootstrap con el contrato de distribución vigente | Contrastar en fase funcional/toolchain; en esta fase conservar ambos hechos y no editar fuente |
<!-- END TANDA-12A-SELFHOST-CONTRADICTIONS -->

<!-- BEGIN TANDA-12B-CONTRADICTIONS -->
## CON-0014 — Ediciones 2.0/3.0 persisten en bootstrap y proyectos distribuidos

- **Estado:** `ABIERTA`.
- **Evidencia:** `bootstrap-profile.json`, `bootstrap.lock`, contratos self-host y tres `.nsproj` aún nombran Language/SDK 2.0 o 3.0.
- **Impacto:** son residuos históricos o metadata de la revisión, no autorización para rechazar fuente por edición.
- **Tratamiento:** casos futuros prueban reconocimiento sintáctico/semántico y registran el residuo; no lo convierten en precondición universal.

## CON-0015 — Documentación de enlace dual frente a matrices actuales de una sola ruta

- **Estado:** `ABIERTA`.
- **Evidencia:** `selfhost/contracts/README.md` describe LLD/NSLink, mientras los JSON vigentes publican arreglos `linkers`/`routes` con una sola entrada.
- **Impacto:** una prueba nueva no puede inferir una segunda ruta inexistente ni borrar la afirmación histórica.
- **Tratamiento:** conservar la contradicción hasta decidir el contrato vigente en Fase 2.

## CON-0016 — Bootstrap lock congela herramientas incidentales por hash y texto de versión

- **Estado:** `ABIERTA`.
- **Evidencia:** `selfhost/bootstrap/bootstrap.lock` fija Python, CMake y Ninja además de productores binarios.
- **Impacto:** esos hashes pueden fallar sin que el compilador esté incorrecto.
- **Tratamiento:** el plan nuevo separa identidad semántica del productor de versiones incidentales del orquestador.

## CON-0017 — Stubs de biblioteca representan intrínsecos, no algoritmos ejecutables

- **Estado:** `ABIERTA`.
- **Evidencia:** SIMD, atomics, bit operations e intrinsics contienen retornos placeholder que dependen del lowering de NSC.
- **Impacto:** una unitaria sobre el cuerpo fuente aceptaría resultados falsos.
- **Tratamiento:** validar IR/objeto y ejecución del programa `.ns`, nunca el stub como oráculo.
<!-- END TANDA-12B-CONTRADICTIONS -->

<!-- BEGIN PHASE-2-CONTRADICTIONS -->
## CON-0018 — Conteo incorrecto en el diseño de TS-SRC-017

- **Estado:** `RESUELTA EN LA SUITE; FUENTE CONSERVADA`.
- **Fuente A:** `test-plans/HOST-SOURCE-TEST-PLAN.md` afirma que `\n\r\r\n` produce cuatro saltos lógicos y cinco
  líneas.
- **Fuente B:** `SourceFile::buildLineStarts`, `components/HOST-SOURCE.md` y `CS-SRC-003` establecen que un par
  `CRLF` es un solo terminador lógico.
- **Discrepancia:** la secuencia contiene `LF`, `CR` y `CRLF`: tres terminadores lógicos y los comienzos
  `[0,1,2,4]`, por lo que produce cuatro líneas.
- **Impacto:** copiar literalmente el oráculo de Fase 1 congelaría una expectativa contraria al producto y al
  contrato consolidado.
- **Tratamiento:** no se reescribe la fuente histórica; `TS-SRC-017` usa el oráculo contractual de cuatro líneas
  y la Tanda 01.14 conserva la derivación byte por byte.
<!-- END PHASE-2-CONTRADICTIONS -->
