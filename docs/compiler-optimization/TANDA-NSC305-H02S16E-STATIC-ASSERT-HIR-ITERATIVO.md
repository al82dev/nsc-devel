# H02S16E · `static_assert` integrado en la máquina HIR iterativa

## SCOPE LOCK

- Objetivo: eliminar la reentrada C++ `statement -> expression -> loop block ->
  statement` para `StaticAssertStatement` mediante la worklist HIR común.
- Base funcional: `0e97b5a8d`; prerrequisito correctivo transportado localmente como
  `cfad7e07f`, con el mismo patch-id que `5e4aae333` del repositorio principal.
- Naturaleza: refactor interno de lowering HIR, sin cambios de lenguaje.
- Archivos autorizados: modelo HIR privado, máquinas HIR de expresión/statement,
  fixture focal y este expediente.
- Contratos congelados: sintaxis, semántica, API, ABI, NSMETA, textos y rangos de
  diagnóstico, evaluación constante, cortocircuito, ownership, cleanup y noheap.
- Protegido: Parser, Sema, MIR, LLVM, Builder, CMake, tracking global y S16F.
- Validación autorizada: compilar sólo `nsc_project_oop_lowering_tests` y ejecutar
  sólo `hir-static-assert-statement-depth`.
- Prohibido: red, instalación, deploy, CTest, Host, minisuite, suite y baseline.

## Causa y cambio

La rama histórica de `lowerStatement` bajaba directamente la condición mediante
`lowerExpression`. Dentro de un `LoopExpression`, el bloque volvía a
`lowerStatement`, por lo que una alternancia profunda conservaba recursión de pila
de proceso aunque el evaluador constante ya fuera iterativo.

S16E agrega una continuación `StaticAssertStatement` a la máquina común. El
statement siembra la condición en contexto de valor; `LoopBlock` reconoce el mismo
statement y apila esa continuación en vez de reentrar. Al regresar el hijo, la
máquina aplica una sola vez `applyExpressionBoundaryPostconditions`, finaliza el
contrato histórico y destruye el HIR temporal iterativamente. El `Block` vacío de
éxito no recibe de nuevo la política de la condición.

El evaluador `evaluateStaticAssertConstant` no cambia: conserva exactamente sus
clases admitidas, operaciones, cortocircuito y errores.

## Contrafactual focal

El selector `hir-static-assert-statement-depth` contiene:

- 32,768 alternancias `StaticAssertStatement -> LoopExpression -> BlockStatement`;
- condición constante profunda positiva preexistente;
- cortocircuito `&&` y `||` preexistente;
- éxito `true` con `syntax`, `builtin:void` y cero hijos;
- `false` sin mensaje y con mensaje;
- constante no booleana;
- condición ausente con diagnóstico histórico exacto;
- fallo interno de loop sin binding y rango interno exacto;
- condición con asignación de heap bajo caller `noheap`.

El contrafactual SCC termina deliberadamente con
`static_assert condition is not a compile-time bool constant`: el loop es una
expresión válida, pero no pertenece a las clases constantes aceptadas. Ampliar el
evaluador para convertirlo en éxito habría cambiado el lenguaje.

## Complejidad

- lowering: una visita por arista, `O(N)`;
- frames: `O(D)` memoria explícita para profundidad `D`;
- pila C++ respecto a la alternancia: `O(1)`;
- evaluación y destrucción del árbol temporal: `O(N)` iterativas;
- no hay clonación ni doble evaluación de la condición.

## Evidencia focal

- Configuración aislada: PASS, Ninja/Release/`clang-cl`, todo el árbol generado
  bajo `A:\NSC-devel-paralelo\builds\agente-6-s16e-rebased`.
- Build exacto: `nsc_project_oop_lowering_tests`, PASS, exit `0`.
- Selector exacto: `hir-static-assert-statement-depth`, PASS `15 checks`, exit
  `0`.
- Log de configuración:
  `A:\NSC-devel-paralelo\logs\agente-6\h02s16e-rebased-configure.log`.
- Log de build final:
  `A:\NSC-devel-paralelo\logs\agente-6\h02s16e-rebased-build-final.log`.
- Log del selector final:
  `A:\NSC-devel-paralelo\logs\agente-6\h02s16e-rebased-focus-final.log`.

No se ejecutará ninguna otra prueba. La batería amplia permanece reservada para la
GUI del usuario al finalizar la tanda completa.
