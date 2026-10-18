# NSC 3.0.5 — H02S13R: `await` HIR iterativo

## Alcance cerrado

Esta hoja elimina la reentrada recursiva de `AwaitExpression` en el lowering HIR. La
clasificación de la ruta directa frente a la ruta de valor `future`, el descenso del
operando y la construcción de `AwaitFuture` ocurren ahora dentro de la máquina iterativa
común de expresiones.

No se modifica sintaxis, AST, resolución semántica, ABI, NSMETA, MIR ni LLVM. Tampoco se
adelantan las familias de llamadas de H02S14.

## Contratos preservados

- La clasificación directa consulta primero el `MethodCallBinding` del operando y sólo
  acepta un ordinal de miembro válido.
- La exploración transitiva del productor es iterativa, tolera ciclos y conserva la
  política efectiva previa: `canSuspend` permanece siempre `false`.
- El operando se baja exactamente una vez, con `addressContext = false`, mediante la
  worklist común.
- La ruta por valor conserva el diagnóstico exacto para un operando no `future`, deja el
  símbolo de poll vacío y no adjunta cleanup ni región de excepción.
- La ruta directa conserva el símbolo mangled con sufijo `$poll`, el estado async/sync y
  la captura de cleanup/handler del await bloqueante.
- Los errores siguen atribuyéndose al rango del `await` cuando el operando o la
  finalización fallan.

## Implementación

- `HirExpressionMachine.cpp` incorpora un frame `AwaitExpression` con slots estables para
  ordinal, cleanups y región de excepción.
- `HirAsyncLowering.cpp` separa el scanner iterativo `awaitProducerCanSuspend` de la
  finalización pura de un operando HIR ya construido.
- `lowerExpressionImpl` conserva un rechazo defensivo si un `await` escapara de la
  máquina iterativa.

## Prueba puntual

Selector único: `hir-await-expression-depth`.

El contrafactual cubre:

1. Un `await` por valor cuyo operando contiene 32,768 niveles de políticas de overflow,
   demostrando que el descenso no consume la pila del proceso.
2. Conservación de tipo interno, tipo del future, ruta `pollFn`, estado sync y aridad.
3. Una ruta directa con productor y caller reales obtenidos por `ProjectCompilation`,
   verificando símbolo `$poll`, estado async y ausencia de metadata de handler fuera de
   un bloque protegido.

## Evidencia focal

- Configuración aislada: `A:\NSC-devel-paralelo\builds\agente-1-s13r` — PASS.
- Target único: `nsc_project_oop_lowering_tests` — PASS, 294 pasos.
- Selector único: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-await-expression-depth` —
  `PASS: 5 checks`, exit code 0.
- Logs:
  - `A:\NSC-devel-paralelo\logs\agente-1\s13r-configure.log`
  - `A:\NSC-devel-paralelo\logs\agente-1\s13r-build.log`
  - `A:\NSC-devel-paralelo\logs\agente-1\s13r-focus.log`

No se ejecutaron CTest, suite completa, baseline, deploy ni instalación.
