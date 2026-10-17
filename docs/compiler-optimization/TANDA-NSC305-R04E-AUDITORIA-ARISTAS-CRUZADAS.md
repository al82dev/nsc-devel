# NSC 3.0.5 — R04E: auditoría final de aristas cruzadas de statements

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `9dc2ddebb`.
- **Objetivo exacto:** demostrar, clasificar y cerrar las reentradas que sobreviven al alternar
  familias de statements, sin confundir pruebas same-family con un parser globalmente iterativo.
- **Naturaleza inicial:** auditoría causal con contrafactuales directos; cualquier cambio de
  producto exige un subítem y una evidencia roja independiente.
- **Archivos de evidencia autorizados:** `tests/unit/ProjectMethodBodyModelTests.cpp`, este
  expediente y tracking externo.
- **Producto inicialmente protegido:** todo el compilador. Sólo una reproducción roja autoriza
  abrir un subítem acotado en `src/Parser/Parser.cpp`.
- **Familias obligatorias:** controles ordinarios, `foreach`, condition-pattern,
  `switch`/`match`, `try`/`using`/`defer` y bloques.
- **Invariantes:** sintaxis, AST, ranges, diagnostics, recovery, cleanup, temporales, semántica,
  API, ABI, NSMETA, CLI y artefactos 3.0.4.
- **Separación obligatoria:** un overflow que ocurra después de syntax-parse, por ejemplo en
  `members`, no se atribuye al parser y se rastrea como defecto independiente.
- **Prohibido:** aumentar stack, reducir profundidad, guards como solución, baseline, suite
  completa, deploy, instalación, push, pull o fetch.
- **Criterio de terminación:** todas las aristas cruzadas quedan verdes o transferidas con
  evidencia a un subítem explícito; ninguna familia se declara cerrada por inferencia.

## Matriz inicial de reentrada

| Productor con estado pendiente | Hijo que puede reentrar | Riesgo |
|---|---|---|
| `StructuredControlParser` | `parseForeachStatement` | máquinas separadas se llaman por bloque |
| `parseForeachStatement` | control estructurado | reentrada inversa por `parseStatement` |
| control estructurado/foreach | condition-pattern | parser monolítico conserva bodies |
| condition-pattern | control/foreach/condition-pattern | embedded block y `else if (let ...)` |
| cualquier bloque | cleanup/exceptions | migrado a la máquina estructural en R04D |
| switch/match | statements de sección/brazo | migrado parcialmente; se audita mezcla |

## Estado

R04E cerrado. Las aristas cruzadas del parser están cubiertas por las pruebas focales. El
desbordamiento que sobrevivió pertenece a `ProjectMemberModel`, se transfirió a B09 y no se
cuenta como una reentrada pendiente de `syntax-parse`.

## R04E1 — ordinary control / `foreach`

### Evidencia roja

Una unidad con 1,024 cuerpos alternados `if (true) { ... }` / `foreach (...) { ... }` terminó
contra `9dc2ddebb` con `0xC00000FD` (`-1073741571`). La prueba aisló la reentrada mutua entre
`StructuredControlParser` y la máquina local de `parseForeachStatement`.

### Cierre

`foreach` se integró a la misma pila `work_` mediante `ParseForeach`/`FinishForeach`. Se reutilizan
sin cambios `parseForeachHeader` y `lowerForeachStatement`; el wrapper público delega a la máquina
estructural. La prueba alternada pasa dentro de `PASS: 106 checks` y `nsc` conserva su contrato de
host runtime estático.

R04E permanece activo para condition-pattern y para la clasificación separada del overflow en
`members`.

## R04E2 — cruces con condition-pattern

### Evidencia roja

Una unidad con 1,024 cuerpos alternados ordinary `if` / `if (let ...)` terminó contra
`cdf047338` con `0xC00000FD`. La reentrada era:

`StructuredControlParser -> parseStatement -> parseConditionalPatternStatement ->
parseEmbeddedStatementBlock/parseBlock -> StructuredControlParser`.

### Cierre de la arista corporal

Las formas `if-let`, `while-let` y `let-else` ahora separan header, bodies y lowering mediante
estado heap-backed y continuaciones de la máquina estructural. Se conservaron validación de
bindings, dangling else, temporales, marker/flatten de let-else, lowering while y diagnostics.

Evidencia:

- alternancia ordinary/condition-pattern de 1,024 niveles: PASS;
- mezcla de 1,024 niveles entre ordinary, foreach, try, using, defer, switch, match y while: PASS;
- `let-else` con 2,048 `if` divergentes: PASS;
- contratos directos de `if-let`, `else if-let`, `while-let` y `let-else`: PASS;
- foco completo: `PASS: 113 checks`;
- `nsc`: construido; contrato estático: PASS;
- `nsc_parser_tests`: conserva el único rojo ajeno de ternario;
- T03 conditional-pattern: conserva dos rojos semánticos por asignabilidad de `ref alias`; no se
  contó como aprobación ni se atribuyó al parser.

## Obligaciones restantes

- R04E3: terminado. Al elevar el contrafactual de 2,048 a 16,384 ramas, la implementación
  recursiva terminó en `0xC00000FD`. Un worklist explícito conserva evaluación de block-last,
  requisitos de ambas ramas y cortocircuito de la rama `then`; el mismo caso pasa dentro de
  `PASS: 113 checks`.
- R04E4: terminado. El overflow posterior pertenece a `members`; no pertenece a syntax-parse y
  no se ocultó bajo el cierre del parser. Su corrección queda abierta explícitamente en B09.

## R04E4 — transferencia causal del overflow de `members`

### Reproducción

El fixture conservado
`A:\temp\nativesharp_stage27_project_method_body_tests\DeepMixedR04BControl` contiene 1,024
controles mixtos dentro de un único cuerpo de método. Ejecutado con el `nsc.exe` focal de
`A:\nsc-devel\build-test-infrastructure\bin`, `-O0`, `-j 1` y `--progress`, produjo esta frontera:

```text
syntax-parse finished (1 ms)
generics finished (3 ms)
names finished (2 ms)
-- NSC: members
EXIT_CODE=-1073741571
```

`-1073741571` corresponde a `0xC00000FD` (`STATUS_STACK_OVERFLOW`). La fase parser termina antes
del fallo; por tanto, el resultado no puede atribuirse a R04 ni utilizarse para reabrir una
familia de statements.

### Productor y símbolo responsable

`ProjectCompilation::compileProjectMembers` construye `ProjectMemberModel`. Al final de
`ProjectMemberModel::build`, cada método con cuerpo se entrega a
`ProjectMemberModel::scanMethodBody`. Ese walker se llama recursivamente para `first`, `second`,
`third` y cada elemento de `children`; la profundidad del stack nativo crece con la profundidad
del AST del método.

La prueba contrafactual descarta `scanInvalidThisContexts` como causa de esta reproducción:
ese segundo walker se ejecuta después y retorna inmediatamente al encontrar un
`MethodDeclaration`, `ConstructorDeclaration` o `DestructorDeclaration`; no desciende al cuerpo
profundo que activa el fallo.

Cadena causal:

```text
1,024 controles ya parseados
  -> ProjectCompilation::compileProjectMembers
  -> ProjectMemberModel::build
  -> ProjectMemberModel::scanMethodBody
  -> recursión proporcional a la profundidad del AST
  -> agotamiento del stack nativo en members
```

### Transferencia sin hueco

Se abre B09 bajo el dueño real `ProjectMemberModel`:

- B09A convierte `scanMethodBody` a un recorrido iterativo preservando exactamente el orden
  preorden, los early returns, `bodyBindings_` y el orden de diagnósticos;
- B09B obtiene una reproducción independiente para `scanInvalidThisContexts` y elimina su
  recursión sin mezclarla causalmente con B09A.

R04 y R04E pueden cerrarse porque la obligación pendiente dejó de pertenecer al parser y conserva
un dueño, reproducción, criterio de corrección y siguiente acción explícitos.
