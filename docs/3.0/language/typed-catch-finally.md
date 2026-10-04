# Typed catch, filters, and finally / Catch tipado, filtros y finally

## Español

En edición 3, `try` conoce los tipos de error declarados por las operaciones de su bloque. Los
`catch` se prueban en el orden escrito:

```nativesharp
try { Load(); }
catch (ParseError problem) when (problem.IsRecoverable) { Recover(problem); }
catch (Error problem) { Report(problem); }
finally { Close(); }
```

Las formas admitidas son `catch (Error problem)`, `catch (Error)` y el comodín `catch`. Cualquiera
puede llevar `when (condición)` en edición 3. La condición debe ser `bool` y puede usar la variable
del catch.

Un filtro puede resultar falso, por lo que no hace exhaustivo al catch. Un catch posterior sigue
siendo necesario cuando el error no puede propagarse mediante `throws E`. `when (false)` es
inalcanzable y se diagnostica. Un catch derivado puede seleccionar instancias derivadas, pero no
cubre por completo una operación declarada como `throws BaseError`. Los catches que no pueden
coincidir con ningún error declarado se rechazan.

`throw;` relanza el mismo objeto nativo. `finally` se ejecuta exactamente una vez al continuar,
retornar, relanzar o propagar. `return`, `break`, `continue` y `?` no pueden salir desde dentro de
un `finally`. `defer`, destructores RAII y owned cleanup conservan su orden normal.

La selección usa RTTI nativo de NativeSharp y EH de Windows x64. No usa CLR, RTTI administrado ni
asignación oculta para evaluar catches o filtros.

## English

In edition 3, a `try` region knows the error types declared by operations in its protected block.
Catch clauses are tested in source order. Supported forms are `catch (Error value)`,
`catch (Error)`, and catch-all `catch`. Each may carry an edition-3
`when (boolean-expression)` filter, and the typed catch binding is in scope in that expression.

A runtime filter may decline the exception and therefore does not make a clause exhaustive. A
later unfiltered clause or a compatible enclosing `throws E` contract must cover the remaining
path. A literal-false filter and a clause that cannot match any declared protected-region error are
diagnosed. A derived pattern can select derived runtime values but does not fully cover an
operation declared as throwing a base error.

`throw;` preserves the same native exception object. `finally` runs exactly once for fallthrough,
return, rethrow, and propagation. Control flow cannot leave from inside a finally block through
`return`, `break`, `continue`, or `?`. Defer, RAII destruction, and owned cleanup retain their
normal ordering.

Catch selection uses NativeSharp class-descriptor RTTI and Windows x64 native EH. It introduces no
CLR dependency, managed RTTI, or hidden heap allocation for catch/filter dispatch.
