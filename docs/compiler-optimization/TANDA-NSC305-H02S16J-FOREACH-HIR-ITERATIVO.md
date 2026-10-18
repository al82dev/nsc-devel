# H02S16J · `foreach` integrado en la máquina HIR iterativa

## SCOPE LOCK

- Objetivo: eliminar las reentradas C++ de `ForeachStatement` hacia su source
  statement y el candidate block seleccionado mediante la worklist HIR común.
- Naturaleza: refactor interno; no cambia sintaxis, semántica, API, ABI,
  NSMETA, shape HIR, diagnósticos, rangos, cleanup ni política `noheap`.
- Archivos de producto: modelo HIR privado y máquinas HIR de
  expresión/statement.
- Protegido: Parser, Sema, AST, MIR, LLVM, Builder e instalación.
- Validación autorizada: target `nsc_project_oop_lowering_tests` y selector
  único `hir-foreach-statement-depth`.
- Prohibido: CTest, Host completo, minisuite, batería, baseline, deploy y red.

## Contrato histórico congelado

El `foreach` continúa publicándose como un HIR `Block`, `builtin:void`, con
exactamente dos hijos y orden `[source statement, selected candidate Block]`.
No se introduce un kind HIR nuevo.

La selección conserva literalmente el contrato previo. Un `typeKey` terminado
en `[]` elige `second()` y tiene prioridad. Los seis spellings históricos de
`Span`/`ReadOnlySpan` eligen `children()[1]` cuando existe. Cualquier otro tipo
elige `third()`. No se visitan, validan ni usan como fallback los candidatos no
seleccionados.

El binding de source se consulta antes de bajar cualquier hijo y mantiene el
diagnóstico exacto. Después se baja primero el source como statement y sólo si
termina correctamente se baja el candidato mediante la ruta ordinaria de
`lowerBlock`. El frame `foreach` no crea, activa ni restaura una frontera de
cleanup propia: conserva deliberadamente la semántica histórica.

## Diseño implementado

`lowerStatement` delega a `lowerForeachStatement`. El inicializador del frame
resuelve el binding y congela el único candidato antes de programar hijos. El
source utiliza el router común de statements migrados, incluido otro
`ForeachStatement`; el candidato se programa como `LoopBlock`. El nodo
provisional sólo se publica cuando ambos hijos terminaron.

El mismo frame se usa desde raíz, blocks, bodies de sección `switch`,
alternativa directa de `if`, declaraciones de `match` y source de otro
`foreach`. El epílogo reconoce el wrapper como statement estructural para no
reaplicar políticas de expresión sobre el `Block` sintético.

## Complejidad y cierre recursivo

- visita: `O(N)`;
- memoria explícita: `O(D)`;
- pila C++ para las aristas migradas: `O(1)`;
- se cerró `Foreach → selected candidate Block → Foreach` a 32,768 niveles;
- se cerró `Foreach → source statement → Foreach` a 32,768 niveles;
- statements reservados para H02S16K–N conservan su fallback hasta su hoja.

## Evidencia

- Target exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector único: `hir-foreach-statement-depth`, PASS 8/8, exit 0.
- Satélites: shape de dos hijos, orden source/candidate, prioridad array,
  candidato no seleccionado inválido ignorado, precedencia de missing binding,
  diagnóstico delegado de candidate no-Block y cleanup exterior intacto.
- Build aislado: `A:\nsc-devel\build-test-infrastructure`.
- No se ejecutaron pruebas adicionales, CTest, suites, baseline, instalación,
  deploy ni red.
