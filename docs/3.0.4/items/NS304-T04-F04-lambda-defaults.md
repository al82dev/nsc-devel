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

## Hallazgo (validado con compilación real)

- El default del lambda YA se parsea y almacena (`parseParameter` :8651-8655 → `parameter->first()`),
  y `Add add = (int x = 5) => x + 1;` compila contra el delegate target.
- Falta la **aplicación del default en el call-site**: `add()` (omitir el argumento) falla en
  `ProjectMethodBodyModel` (:15652-15661) con "delegate invocation argument count does not match".
- **Causa raíz**: sintetizar el default solo en `arguments` (Sema) NO basta: el codegen
  (HIR/LLVM) lee los argumentos del AST (`expression->children()`), no del vector de Sema. Se
  necesita bajar el default al AST (sintetizar un nodo) o guardarlo en `MethodDelegateBinding` y
  que el lowerer lo materialice. Es un cambio que toca codegen, no un fix puntual.

## Plan

1. Sintetizar el default como nodo AST (o almacenar el valor en `MethodDelegateBinding`) y hacer
   que el HIR/LLVM lo emita como argumento real.
2. Validar que el default del lambda no contradiga el del delegate.
3. Prueba `TL-LANG-317` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-317`: `Add add = (int x = 5) => x + 1; return add();` — exit 6.
