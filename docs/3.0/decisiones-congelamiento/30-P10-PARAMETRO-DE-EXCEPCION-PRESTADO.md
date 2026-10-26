# Congelamiento 3.0 P10 — un parámetro de excepción es un préstamo no escapable

Fecha: 2026-10-26. Revisión de partida: `80b186758`.

## Decisión aplicada

P10, opción 1, en palabras de Alberto: todo parámetro que reciba una excepción atrapada es un préstamo no
escapable: no puede almacenarse en campos, statics, locales exteriores, closures ni retornarse. Solo una
transferencia explícita de ownership puede sobrevivir al catch.

## Problema

Límite documentado de DEC-10 (doc 13, sonda `x10`): un método que recibe la excepción atrapada y la guarda en un
campo no dueño compilaba. El catch destruía la excepción y el campo quedaba apuntando a memoria liberada:

```
public static void Store(Holder h, Boom b) { h.Saved = b; }
…
catch (Boom problem) { Store(holder, problem); }
return holder.Saved.Code;   // lectura después de liberar
```

## SCOPE LOCK

- **Regla.** Un parámetro no `owned` cuyo tipo es una clase de excepción (`NativeSharp.Core.Exception` o
  derivada) es un préstamo del llamador. El llamado no sabe si recibió una excepción atrapada, así que la regla
  es por tipo. Dentro del llamado:
  - guardarlo en un campo, un static, un elemento, otro parámetro o un local de un callable exterior es error;
  - retornarlo es error;
  - capturarlo en un lambda es error;
  - leerlo, pasarlo como argumento y copiarlo a un local del mismo callable se permite; ese local hereda el
    préstamo (mismas restricciones).
- **Transferencia.** Un parámetro `owned` recibe la excepción con `move(e)` desde el catch (DEC-10) y puede
  guardarla.
- **Cambia.**
  - `MethodLocalSymbol.h`: `loanedExceptionParameterOrdinal`.
  - `StatementAnalyzer.cpp`: `parameterIsExceptionLoan`, `loanedExceptionParameterOfValue`,
    `reportExceptionLoanReturn` (en `reportDirectOwnerReturnWithoutMove`), `rejectExceptionLoanAssignment`; un
    local declarado con el préstamo lo hereda.
  - `AssignmentAnalyzer.cpp`: la asignación llama a `rejectExceptionLoanAssignment` junto a la regla de DEC-10.
  - `LambdaDelegateBinder.cpp`: capturar el parámetro o un local que lo copia es error.
- **Idéntico.** Las reglas de DEC-10 para la variable del catch, los parámetros de otros tipos, `throw`.

## Validación

nsc Release recompilado. Sondas en `A:\temp\nsc-dec\d17` (cada `Store` recibe la excepción atrapada):

| Sonda | Forma | Resultado |
|---|---|---|
| p1Field (`x10`) | `h.Saved = b` | rechazo «the exception in 'b' is a loan from the caller…» |
| p2Static | `Holder.Global = b` | rechazo |
| p3Element | `a[0] = b` | rechazo |
| p4Return | `return b` | rechazo «…cannot be returned» |
| p5Alias | `Boom alias = b; h.Saved = alias;` | rechazo en `alias` |
| p6Lambda | `Getter f = () => b.Code;` | rechazo «…cannot be captured by a lambda» |
| p7Exception | parámetro de tipo `Exception` guardado en un elemento | rechazo |
| p8ReadAndPass | leer, pasar a otro método y copiar a un local | 141: lee 7 dos veces, una destrucción |
| p9OwnedTransfer | `Keep(holder, move(problem))` con `owned Boom` guardado en un campo `owned` | 701: el catch no la destruye; se destruye con `holder` |
| p10LocalReassign | `Boom x = null; x = b; return x.Code;` | 71 |

Regresión de DEC-10 (`d10`): `x1`, `x3`, `x4`, `x7`, `x12`, `x13`, `x17` siguen rechazadas con el mismo mensaje; `x10`
ahora se rechaza; `c01` 0, `x2` 17, `x5` 114, `x8` 1107, `x9` 7, `x11` 120, `x14` 1207, `x16` 107, `x18` 1. Proyectos con
`catch` de DEC-10 con `--verify-project-oop-llvm`: todos exit 0 salvo `26_deitel_live_code_native`, que sigue sin
poder resolver `NativeSharp.Core.IO` (anterior a este cambio).

Casos `nsc_component_tests` Debug y Release: 96 PASS de referencias, 30 PASS de copia. Corpus de `await`: igual que
`d04`.

Prueba nueva: `Freeze30ContractTests::testCaughtExceptionParameterLoan`.

## Hallazgo (doc 31)

La variable del catch misma también es el parámetro que recibe la excepción atrapada, y capturarla en un lambda
compilaba (sonda `p11CatchLambda`: el delegate guardado en un static sobrevive al handler). Va en su propio commit.
