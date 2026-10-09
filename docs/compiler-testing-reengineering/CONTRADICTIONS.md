# Registro de contradicciones

| ID | Estado | Fuente A | Fuente B | Discrepancia | Impacto | Acción necesaria |
|---|---|---|---|---|---|---|

| `CON-0001` | ABIERTA | `Language3Surface`, `CompatibilitySemantics`, `IntegrationQuality` y `ReleaseSemantics` | Decisión posterior del usuario: el compilador no debe rechazar por edición | La implementación vigente conserva etiquetas, gates y metadata ligados a 3.x | Puede convertir historia interna en rechazo semántico | Separar en una fase funcional autorizada; Fase 1 sólo preserva evidencia |
| `CON-0002` | ABIERTA | `AbiSemantics`, `ErrorSemantics`, `ExpressionErgonomics`, `SumTypeSemantics` | Decisión posterior del usuario de no forzar `unsafe` y revisar casts explícitos | Los helpers vigentes conservan contexto y operaciones `unsafe` | Afecta superficie y compatibilidad del lenguaje | Diseñar cambio sólo bajo SCOPE LOCK funcional posterior |
| `CON-0003` | ABIERTA | `ModuleSemantics::moduleGraphResolvable` | Lineamiento general que prohíbe recursión salvo necesidad demostrada | El grafo usa DFS recursivo proporcional a la profundidad | Riesgo de stack y divergencia normativa | Reemplazo iterativo requiere autorización funcional; conservar caso NIGHTLY |

Estas contradicciones se registran sin corregirlas porque la Fase 1 es exclusivamente documental.
