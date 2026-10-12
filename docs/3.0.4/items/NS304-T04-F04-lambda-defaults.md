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

## Causa raíz + precedente (mecanismo exacto)

- Sintetizar el default solo en `arguments` (Sema) NO basta: el codegen lee los argumentos del AST.
- **Mecanismo exacto para MÉTODOS** (replicable): `mapArgumentsToParameters` produce
  `defaultedParameterOrdinals`; el call binding guarda `selectedDefaultArguments` con el
  `defaultValueSyntax` de cada omitido (`ProjectMethodBodyModel.cpp:14833-14835`, :16488); y el
  HIR sintetiza el default (`ProjectOopHirModel.cpp:3785-3794`, :6986-7001`, :7337-7362`).
- Falta replicarlo en la invocación de DELEGATE (:15642+): leer los defaults del delegate
  (`ParameterDeclaration.first()`), añadir `selectedDefaultArguments` a `MethodDelegateBinding`
  (ProjectMethodBodyModel.h:1307), y en el HIR de delegate emitir el default.

## Plan

1. Añadir `std::vector<const ast::AstNode*> selectedDefaultArguments` a `MethodDelegateBinding`.
2. En la invocación de delegate (:15642+), cuando falten argumentos, recoger los defaults del
   delegate (`first()`) en `selectedDefaultArguments` (y validar que todos los omitidos tengan).
3. En el HIR de delegate (junto al caso Invoke), sintetizar los defaults como argumentos reales.
4. Prueba `TL-LANG-317` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-317`: `Add add = (int x = 5) => x + 1; return add();` — exit 6.
