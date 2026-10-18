# H02S16I · `for` integrado en la máquina HIR iterativa

## SCOPE LOCK

- Objetivo: eliminar las reentradas C++ de `ForStatement` hacia initializer,
  condition, increment y body mediante la worklist HIR común.
- Naturaleza: refactor interno; no cambia sintaxis, semántica, API, ABI,
  NSMETA, shape HIR, diagnósticos, rangos, cleanup ni política `noheap`.
- Archivos de producto: modelo HIR privado y máquinas HIR de
  expresión/statement.
- Protegido: Parser, Sema, AST, MIR, LLVM, Builder e instalación.
- Validación autorizada: target `nsc_project_oop_lowering_tests` y selector
  único `hir-for-statement-depth`.
- Prohibido: CTest, Host completo, minisuite, batería, baseline, deploy y red.

## Contrato histórico congelado

El HIR `For`, `builtin:void`, conserva exactamente cuatro posiciones:
`[initializer, condition, increment, body]`. Los tres primeros slots publican
un `nullptr` literal cuando su sintaxis está ausente.

El orden es estricto y de fallo inmediato: initializer, condition, increment,
comprobación de body y body. El initializer trata sólo `BlockStatement` y
`VariableDeclarationStatement` como statements especiales; cualquier otro kind
entra por expresión. El incremento sólo trata `BlockStatement` de forma
especial. El body usa exclusivamente `children().front()`; children posteriores
se ignoran y un front nulo conserva el diagnóstico de `lowerBlock`, distinto de
un vector vacío.

Sólo una `LocalDeclaration` directa del initializer activa cleanup para las
fases posteriores. La profundidad anterior al `for` se restaura en éxito y en
cualquier fallo. El nodo provisional nunca se publica parcialmente.

## Diseño implementado

`lowerStatement` delega a `lowerForStatement`. El frame `ForStatement` conserva
el nodo provisional, etapa y frontera cleanup. `advanceForStatement` programa
cada slot sobre la worklist común y aplica las postcondiciones de expresión una
sola vez únicamente a slots que históricamente llamaban `lowerExpression`.

El inicializador común se usa desde raíz, `LoopBlock`, alternativa de `if`,
declaraciones de `match` y bodies de sección `switch`. `ForStatement` no se
incluye en la recuperación especial de condiciones de `if`/`while`/`do`: un
fallo de cualquier slot es terminal y no visita los posteriores.

## Complejidad y cierre recursivo

- visita: `O(N)`;
- memoria explícita: `O(D)`;
- pila C++ para las aristas migradas: `O(1)`;
- se cerraron `For → body Block → For` y
  `For → initializer Block → For` a 32,768 niveles;
- statements reservados para H02S16J–N conservan su fallback hasta su hoja.

## Evidencia

- Target exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector único: `hir-for-statement-depth`, PASS 12/12, exit 0.
- Satélites: cuatro slots y nulls, orden completo, precedencia de fallos,
  missing body, body front nulo, children extra ignorados y rollback cleanup.
- Build aislado: `A:\nsc-devel\build-test-infrastructure`.
- No se ejecutaron pruebas adicionales, CTest, suites, baseline, instalación,
  deploy ni red.
