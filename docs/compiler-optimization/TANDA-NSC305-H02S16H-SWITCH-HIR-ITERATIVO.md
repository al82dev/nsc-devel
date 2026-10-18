# H02S16H · `switch` integrado en la máquina HIR iterativa

## SCOPE LOCK

- Objetivo: eliminar las reentradas C++ de `SwitchStatement` hacia gobernante,
  secciones y statements ya migrados mediante la worklist HIR común.
- Naturaleza: refactor interno; no cambia sintaxis, semántica, API, ABI,
  NSMETA, shape HIR, diagnósticos, rangos, cleanup ni política `noheap`.
- Archivos de producto: modelo HIR privado y máquinas HIR de
  expresión/statement.
- Evidencia: fixture focal de lowering OOP y este expediente.
- Protegido: AST/Sema, `MethodSwitchBinding`, Parser, Bodies, MIR, LLVM,
  Builder, instalación y tracking global.
- Validación autorizada: target `nsc_project_oop_lowering_tests` y selector
  único `hir-switch-statement-depth`.
- Prohibido: CTest, Host completo, minisuite, batería, baseline, deploy y red.

## Contrato histórico congelado

El binding semántico se resuelve antes de bajar el gobernante. Un binding
ausente conserva su diagnóstico exacto y ninguna expresión ni sección se
visita. Después, el gobernante se baja en contexto de valor y sus
postcondiciones de frontera se aplican exactamente una vez.

Los hijos de sintaxis se recorren en orden fuente. Sólo `SwitchSection`
consume una entrada posicional de `MethodSwitchBinding::sections`; hijos nulos
o de otro kind se ignoran. Un exceso de secciones conserva su diagnóstico
exacto antes de crear o bajar el body afectado. Bindings sobrantes no cambian
el resultado.

Cada sección produce `SwitchSection` con sus `labelValues` y `hasDefault`
copiados desde Sema, y un único hijo `Block` cuya sintaxis sigue siendo la
sección. Los `SwitchLabel` no se evalúan ni se lowerizan. Sólo una
`LocalDeclaration` directa activa cleanup; declaraciones envueltas no se
reclasifican. La profundidad se restaura al cerrar o fallar cada sección.

## Diseño implementado

`lowerStatement` delega a `lowerSwitchStatement`, que siembra un frame
`SwitchStatement`. Su estado propio conserva binding, nodo provisional,
sección/body provisionales y los tres cursores independientes de sintaxis,
binding y body.

El inicializador común se usa desde:

- raíz de statement;
- `LoopBlock`;
- alternativa directa de `if`;
- declaraciones sintéticas de `match`;
- body de otra sección `switch`.

La publicación es transaccional. El nodo Switch sólo se devuelve cuando todas
las secciones terminaron; cada section/body se anexa únicamente después de
restaurar su cleanup. El rollback reconoce secciones activas, pero se difiere
mientras la recuperación de condición de un `if`/`while`/`do` descendiente aún
debe ejecutar su cuerpo.

## Cierres recursivos cubiertos

La prueba profunda rota entre:

1. `Switch → Section → Switch`;
2. `Switch → Section → Block → Switch`;
3. `Switch → Section → If → Block → Switch`;
4. `Switch → Section → While/Do → Block → Switch`.

Los statements reservados para H02S16I–N conservan temporalmente el fallback
histórico y se cerrarán en sus propias hojas; H02S16H no afirma que esos SCC ya
sean iterativos.

## Complejidad

- visita de sintaxis y binding: `O(N)`;
- memoria explícita: `O(D)` para profundidad `D`;
- pila C++ para las aristas migradas: `O(1)`;
- labels: copia lineal de los valores ya plegados por Sema;
- ownership único y cero publicación parcial ante error.

## Evidencia

- Target exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector único: `hir-switch-statement-depth`, PASS 8/8, exit 0.
- Profundidad: 32,768 niveles mixtos.
- Satélites: binding antes del gobernante, gobernante antes del overflow,
  overflow exacto, null/no-section, label imposible no visitado, binding
  sobrante, shape/labels/default/orden y rollback de cleanup.
- Build aislado: `A:\nsc-devel\build-test-infrastructure`.
- No se ejecutaron pruebas adicionales, CTest, suites, baseline, instalación,
  deploy ni red.
