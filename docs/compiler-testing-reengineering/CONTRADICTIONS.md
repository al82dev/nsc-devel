# Registro de contradicciones

| ID | Estado | Fuente A | Fuente B | Discrepancia | Impacto | Acción necesaria |
|---|---|---|---|---|---|---|

| `CON-0001` | ABIERTA | `Language3Surface`, `CompatibilitySemantics`, `IntegrationQuality` y `ReleaseSemantics` | Decisión posterior del usuario: el compilador no debe rechazar por edición | La implementación vigente conserva etiquetas, gates y metadata ligados a 3.x | Puede convertir historia interna en rechazo semántico | Separar en una fase funcional autorizada; Fase 1 sólo preserva evidencia |
| `CON-0002` | ABIERTA | `AbiSemantics`, `ErrorSemantics`, `ExpressionErgonomics`, `SumTypeSemantics` | Decisión posterior del usuario de no forzar `unsafe` y revisar casts explícitos | Los helpers vigentes conservan contexto y operaciones `unsafe` | Afecta superficie y compatibilidad del lenguaje | Diseñar cambio sólo bajo SCOPE LOCK funcional posterior |
| `CON-0003` | ABIERTA | `ModuleSemantics::moduleGraphResolvable` | Lineamiento general que prohíbe recursión salvo necesidad demostrada | El grafo usa DFS recursivo proporcional a la profundidad | Riesgo de stack y divergencia normativa | Reemplazo iterativo requiere autorización funcional; conservar caso NIGHTLY |
| `CON-0004` | ABIERTA | `SemanticAnalyzer`, `ExpressionInfo` y diagnósticos de Sema | Decisión posterior del usuario: neutralidad de edición y no forzar features | La implementación vigente conserva aliases 2.0, textos `NativeSharp 2.0`, ownership y requisitos históricos | Las pruebas nuevas deben caracterizar el presente sin convertirlo en política futura | Resolver sólo en una fase funcional autorizada |
| `CON-0005` | ABIERTA | `EffectContext::allows`, `SemanticDumper`, `SymbolDumper`, `statementPreventsFallthrough` y `containsBreakTargetingLoop` | Lineamiento general que prohíbe recursión salvo necesidad demostrada | Cinco recorridos vigentes usan recursión sin límite o visited set | Profundidad hostil puede agotar stack o ciclar | Diseñar casos NIGHTLY; una reescritura iterativa queda fuera de Fase 1 |
| `CON-0006` | ABIERTA | `SemanticAnalyzer` single-file | Frontend/proyecto contienen soporte posterior para clases, enum underlying types, instance context, placement e inline assembly | La ruta auditada rechaza explícitamente esas capacidades | Un test sin frontera de pipeline confundiría rechazo local con ausencia global de feature | Mantener suites y contratos separados por ruta propietaria |

Estas contradicciones se registran sin corregirlas porque la Fase 1 es exclusivamente documental.
