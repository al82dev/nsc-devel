# NS304-T04-F04 — Parámetros opcionales y defaults consteval en lambdas

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development
- **Alcance (SCOPE LOCK)**: `Parser::parseLambdaExpression` (parámetros con default),
  `ProjectMethodBodyModel::bindLambdaToDelegate`, y la prueba `TL-LANG-317`. NO tocar CMake,
  toolchain, Builder, foreach, rango T01-F04.

## Auditoría (línea base)

- `parseParameter` ya soporta `int x = 5` (default) en delegados/métodos
  (TP-PAR-145: `delegate void Work(int first, long second = 42);`).
- Los parámetros de lambda SIN tipo contextuales (`(x, y)`) no admiten default; los que llevan
  tipo van por `parseParameter` (:13546) y en principio sí.
- `bindLambdaToDelegate` no valida/baja defaults de lambda contra el target (la firma del target
  `fn*`/delegate no lleva defaults en la identidad del tipo).

## Gap F04

Permitir `(int x = 5) => ...` en lambdas (y defaults consteval) y validar contra el target: el
default de la lambda no puede entrar en conflicto con el del delegate.

## Causa raíz + precedente

- Sintetizar el default solo en `arguments` (Sema) NO basta: el codegen (HIR/LLVM) lee los
  argumentos del AST (`expression->children()`), no del vector de Sema.
- **Precedente**: las llamadas a MÉTODOS ya aplican defaults (`mapArgumentsToParameters` :276 →
  `defaultedParameterOrdinals` (:330,:413) sobre `candidate.parameters[index].defaultValueSyntax`;
  `evaluated.defaultedParameterOrdinals` :14554 y `defaultedParameterCount` :14576). El codegen de
  método sintetiza el default desde `defaultValueSyntax`.
- Falta replicar ese mecanismo en la invocación de DELEGATE (:15642+): leer los defaults del
  delegate (`ParameterDeclaration.first()`), guardar `defaultedParameterOrdinals` en
  `MethodDelegateBinding`, y que el lowerer de delegate emita el default.

## Plan

1. En la invocación de delegate (:15642+), calcular los `defaultedParameterOrdinals` (parámetros
   omitidos con `first() != null`) y guardarlos en `MethodDelegateBinding`.
2. En el lowerer de delegate (HIR/LLVM), sintetizar el default como argumento real (como hace el
   lowerer de método).
3. Validar que el default del lambda no contradiga el del delegate.
4. Prueba `TL-LANG-317` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-317`: `Add add = (int x = 5) => x + 1; return add();` — exit 6.
