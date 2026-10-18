# H02S16K · valores de `break`/`return`/`throw` integrados en la máquina HIR iterativa

## SCOPE LOCK

- Objetivo: eliminar las reentradas C++ de los valores de `BreakStatement`,
  `ReturnStatement` y `ThrowStatement` hacia `lowerExpression` mediante la
  worklist HIR común.
- Naturaleza: refactor interno; no cambia sintaxis, semántica, API, ABI,
  NSMETA, shape HIR, diagnósticos, rangos, cleanup, ownership ni política
  `noheap`.
- Archivos de producto: modelo HIR privado y máquinas HIR de expresión,
  statement y excepciones/salidas.
- Protegido: `ContinueStatement`, `UsingStatement`, `TryStatement`, coroutines,
  Parser, Sema, AST, MIR, LLVM, Builder e instalación.
- Validación autorizada: target `nsc_project_oop_lowering_tests` y selector
  único `hir-exit-value-depth`.
- Prohibido: CTest, Host completo, minisuite, batería, baseline, deploy y red.

## Contrato histórico congelado

`break(value)` crea primero el nodo `Break`, copia structured-exit y label,
exige después el `MethodLoopBreakBinding`, baja el valor, aplica la conversión
congelada no identidad, anexa un solo hijo y publica `resultTypeKey` como
`elementTypeKey`. Un `break` sin valor no exige ese binding.

`return(value)` crea y anota primero el nodo `Return`, resuelve el callable
antes del valor y lo baja en contexto de dirección únicamente para retornos
por referencia. En retornos por valor conserva, en este orden: conversión
implícita, wrapper `BorrowValue` para ownership `Borrowed`, validación
`must_tail` y anexado. Un `return` sin valor no valida callable, borrow ni
`must_tail`.

`throw(value)` exige el binding de excepción antes de crear el nodo o visitar
el valor. Congela kind `Throw`/`Rethrow`, tipo/ordinal, continuation
`Exception`, slice de cleanup en orden interno→externo y handler target antes
de bajar el valor. Después anexa el hijo sin conversión. Un rethrow sin valor
conserva la ruta histórica.

Los textos de error permanecen byte-exactos:

- `T07-F05 break value has no loop-expression semantic binding`;
- `Stage 34 return refers to invalid callable`;
- `Stage 50.8 borrowed return has no frozen borrow binding`;
- `T08-F06 validated must_tail return did not lower to a direct call`;
- `Stage 45.6 throw statement has no Stage 45.2 exception binding`.

## Diseño implementado

`ExitValueStatement` es un frame privado de la worklist. Su inicializador
construye el nodo provisional y ejecuta todo el preflight histórico antes de
programar el valor. El valor desciende por la misma máquina de expresiones con
el `addressContext` congelado. Al regresar se aplican exactamente una vez las
postcondiciones de frontera de expresión y luego la finalización específica de
la salida. El nodo provisional sólo se publica después de completar todos los
pasos; cualquier error destruye el estado parcial.

El mismo frame se agenda desde la entrada raíz y desde los cinco fallbacks que
podían cerrar un SCC recursivo: cuerpo de sección `switch`, source de
`foreach`, `Block`, alternativa directa de `if` y declaraciones de `match`.
Las ramas de `lowerExceptionOrExitStatement` ya no contienen llamadas
recursivas a `lowerExpression` para estas tres familias. `continue`, `using` y
`try` permanecen intactos para H02S16L/M.

## Complejidad y cierre recursivo

- visita: `O(N)`;
- memoria explícita: `O(D)`;
- pila C++ para las aristas migradas: `O(1)`;
- se cerró una cadena mixta de 32,768 salidas con el SCC
  `exit(value) → LoopExpression → Block → exit`;
- conversiones, borrow, `must_tail`, structured-exit y cleanup conservan el
  orden histórico;
- `using`, `try/catch/finally` y coroutines siguen reservados para H02S16L–N.

## Evidencia

- Target exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector único: `hir-exit-value-depth`, PASS 10/10, exit 0.
- Profundidad: 32,768 salidas mixtas `break`/`return`/`throw`.
- Satélites del mismo selector: forma/tipo/identidad, structured exits,
  conversión de `break`, borrow de retorno, `must_tail`, precedencia de los
  tres preflight errors, snapshot de cleanup/handler y estado cleanup exterior
  intacto.
- Build aislado: `A:\nsc-devel\build-test-infrastructure`.
- No se ejecutaron pruebas adicionales, CTest, suites, baseline, instalación,
  deploy ni red.
