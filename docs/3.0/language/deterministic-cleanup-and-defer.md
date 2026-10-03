# NativeSharp 3.0 — Cleanup determinista y `defer` / Deterministic cleanup and `defer`

## Contrato normativo / Normative contract

NativeSharp 3.0 usa un solo grafo de cleanup para destrucción RAII, `finally` y `defer`.
Una salida normal, `return`, `break`, `continue` o propagación excepcional recorre exactamente
las acciones activas, en orden LIFO. `defer` no crea un segundo mecanismo de unwind ni requiere
asignación de heap.

NativeSharp 3.0 uses one cleanup graph for RAII destruction, `finally`, and `defer`. Normal scope
exit, `return`, `break`, `continue`, and exception propagation execute exactly the active actions
in LIFO order. `defer` does not introduce a second unwind mechanism or require heap allocation.

La sintaxis sólo está disponible con `<Language>3</Language>`.

The syntax is available only under `<Language>3</Language>`.

## Sintaxis y captura / Syntax and capture

```nativesharp
defer [ref state, move resource, ref this]
{
    Flush(state, resource);
}
```

- Los corchetes son obligatorios; `defer [] { ... }` es válido.
- Cada local, parámetro o receiver exterior usado por el cuerpo debe aparecer explícitamente.
- `ref name` conserva una referencia al storage y observa su valor al ejecutar el cleanup.
- `move name` evalúa y transfiere el valor al registrar el `defer`; el origen queda moved cuando
  su tipo exige ownership. Para valores copiables, captura un snapshot.
- El receiver sólo admite `ref this`; `move this` es inválido.
- Un nombre no puede aparecer dos veces. Símbolos estáticos y globales no requieren captura.

- Brackets are mandatory; `defer [] { ... }` is valid.
- Every enclosing local, parameter, or receiver used by the body must be listed explicitly.
- `ref name` keeps a reference to storage and observes its value when cleanup executes.
- `move name` evaluates and transfers the value when `defer` is registered; ownership-bearing
  sources become moved. Copyable values produce a snapshot.
- The receiver accepts only `ref this`; `move this` is invalid.
- A name may not appear twice. Static and global symbols require no capture.

## Registro, alcance y orden / Registration, scope, and order

El registro ocurre al alcanzar la sentencia. Sólo las sentencias textualmente posteriores del
mismo bloque quedan protegidas. Si el control nunca alcanza el registro, la acción no existe.
Varios `defer` se ejecutan en orden inverso al registro. Los `defer` interiores terminan antes que
los exteriores.

Registration occurs when execution reaches the statement. Only textually subsequent statements
in the same block are protected. If control never reaches registration, the action does not
exist. Multiple `defer` actions execute in reverse registration order. Inner actions finish before
outer actions.

El lowering normativo de un `defer` es un `try/finally` generado alrededor de la continuación del
bloque. Por ello, `return`, `break` y `continue` atraviesan el mismo grafo existente; una excepción
nativa que hace unwind también lo atraviesa. Una terminación inmediata de proceso en perfil abort
no promete ejecutar código posterior a la terminación.

The normative lowering wraps the remaining block continuation in a generated `try/finally`.
Therefore `return`, `break`, and `continue` traverse the existing graph; a native exception that
unwinds traverses it as well. Immediate process termination under an abort profile does not
promise execution after termination.

## Destructores frente a `defer` / Destructors versus `defer`

Un destructor pertenece al valor owner y se ejecuta cuando ese valor vivo abandona su alcance.
Un `defer` pertenece al punto de registro y ejecuta el bloque escrito por el usuario. Si un owner
se captura con `move`, primero se ejecuta el cuerpo diferido y después se destruye exactamente una
vez el owner capturado. La ranura fuente ya no vuelve a destruirlo.

A destructor belongs to an owner value and runs when that live value leaves scope. A `defer`
belongs to its registration point and runs the user-written block. When an owner is captured with
`move`, the deferred body runs first and the captured owner is then destroyed exactly once. The
source slot does not destroy it again.

El cuerpo de `defer` usa las restricciones de `finally`: no puede abandonar su región mediante
`return`, `break` o `continue`. NativeSharp 3.0 no admite `goto`; por tanto, no existe un salto que
pueda entrar en una continuación protegida o evadir el registro.

The `defer` body follows `finally` restrictions: it cannot leave its region using `return`,
`break`, or `continue`. NativeSharp 3.0 has no `goto`, so no jump can enter a protected
continuation or bypass registration.

## Diagnósticos mínimos / Required diagnostics

- uso de `defer` fuera de edición 3;
- captura sin `ref` o `move`, nombre duplicado o nombre inexistente;
- `move this`;
- acceso a local, parámetro o receiver exterior no capturado;
- uso posterior de un owner transferido mediante `move`;
- salida no local desde el cuerpo diferido.
