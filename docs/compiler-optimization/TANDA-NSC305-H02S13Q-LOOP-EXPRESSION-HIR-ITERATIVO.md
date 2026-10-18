# H02S13Q — loop expression y arista expression→block HIR iterativos

## SCOPE LOCK

- Repositorio: clon local `A:\NSC-devel-paralelo\development-agente-1-hir`.
- Rama: `codex/development-agente-1-hir`.
- Base de la hoja: H02S13N en `e90ca5056252d7e849fae44aeab30deca4d04d7d`.
- Cambio: migrar únicamente `LoopExpression` y su frontera directa
  loop→Block→ExpressionStatement a la worklist HIR existente.
- Preservado: sintaxis, API, ABI, NSMETA, diagnósticos, binding, label, result type, orden de
  statements, anidamiento de blocks y fronteras cleanup.
- Protegido: H02S13R y posteriores, las familias statement H02S16, Parser, MIR, LLVM, tracking y
  repositorio integrador.
- Validación autorizada: compilar sólo `nsc_project_oop_lowering_tests` y ejecutar sólo el selector
  `hir-loop-expression-depth`.
- Prohibido: otras pruebas, CTest, suite, baseline, deploy, instalación y red.

## Causa

La rama histórica de `LoopExpression` creaba la raíz HIR y llamaba directamente a `lowerBlock`.
Aunque `lowerBlock` ya recorría blocks anidados con un vector, cada `ExpressionStatement` volvía a
`lowerExpression`; por tanto una cadena loop→Block→ExpressionStatement→loop conservaba una trama
nativa por nivel y desbordaba el stack antes de completar HIR.

## Diseño implementado

`lowerExpressionWithStatementFrame` incorpora `LoopExpression`, `LoopBlock` y
`LoopBlockExpressionStatement` como continuaciones de la misma worklist. El frame loop congela
binding, label y result type antes de descender. El frame block conserva nodo resultado, índice de
statement, `memberOrdinal` y profundidad cleanup de entrada. Blocks anidados y expression
statements se agendan sobre la misma pila; al regresar se anexan exactamente una vez y en orden
fuente.

Las demás familias statement continúan como operación atómica mediante `lowerStatement`: Q no
absorbe `break`, `return`, `throw`, controls, exceptions, async, declaraciones profundas ni las
hojas H02S16. Un `LocalDeclaration` ya bajado activa su región sólo después de anexarse, y cada
block restaura su profundidad al completar. Ante error se restaura la profundidad raíz de la
operación.

La rama histórica de `lowerExpressionImpl` ya no llama `lowerBlock`; delega al coordinador
iterativo. Permanecen byte-exactos:

- `a loop expression produces a value, not addressable storage`;
- `T07-F05 loop expression has no semantic binding`;
- `Stage 34 OOP HIR requires block statement body`.

## Prueba puntual e infraestructura

El target existente `nsc_project_oop_lowering_tests` incorpora un único selector:

```text
NSC_PROJECT_OOP_LOWERING_FOCUS=hir-loop-expression-depth
```

El foco construye 32,768 niveles loop→Block→ExpressionStatement→loop y comprueba type, label,
shape, orden y terminal. Sus oráculos someros cubren address context, binding ausente, body no
Block, block vacío/anidado, orden fuente, activación cleanup y restauración tanto en éxito como al
primer error.

No se crea target, CTest, manifiesto, catálogo ni entrada de Builder nuevos: el selector y la ruta
default pertenecen al ejecutable unitario existente.

## Frontera pendiente explícita

Esta hoja cierra únicamente loop→block y la vuelta por `ExpressionStatement`. Las rutas de otros
statements que regresan a expresión permanecen contadas en H02S16A/B/D/E/F–N. En particular,
`break` con valor continúa siendo H02S16K. S13Q no declara cerrado el SCC global
expression/block/statement.

## Evidencia

- Auditoría estática: `git diff --check` limpio.
- Arista textual `LoopExpression -> lowerBlock`: cero ocurrencias.
- Selector `hir-loop-expression-depth`: una sola ocurrencia de registro.
- Configure aislado: PASS en `A:\NSC-devel-paralelo\builds\agente-1-s13q`.
- Build exacto: `nsc_project_oop_lowering_tests`, PASS, exit 0.
- Selector puntual: `hir-loop-expression-depth`, PASS 8/8, exit 0.
- Profundidad validada: 32,768 niveles loop→Block→ExpressionStatement→loop.
- Logs: `A:\NSC-devel-paralelo\logs\agente-1\H02S13Q-configure.log`,
  `A:\NSC-devel-paralelo\logs\agente-1\H02S13Q-build-final.log` y
  `A:\NSC-devel-paralelo\logs\agente-1\H02S13Q-focus.log`.
- Otras pruebas ejecutadas: ninguna.
