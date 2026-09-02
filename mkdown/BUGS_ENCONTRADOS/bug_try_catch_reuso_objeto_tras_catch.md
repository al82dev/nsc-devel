# Bug real de compilador -- crash tras reusar un objeto RAII en un segundo try luego de un catch

Encontrado el 2026-08-28 escribiendo `examples/tutorial_19_excepciones` (parte del paquete de
ejemplos academicos), NO buscado a proposito -- un ejemplo natural de "catch de una excepcion" +
"otro try/finally sobre el mismo objeto RAII, mas abajo en el mismo metodo" crashea el binario
generado (SIGSEGV real, visto como exit code 128+7=135 via la convencion de wrap de Wine).

## Repro minimo confirmado (bisectado a mano, 5 variantes probadas)

```csharp
namespace NativeSharp.Core { public class Exception { public string Message; } }
using NativeSharp.Core;

class InsufficientFundsException : Exception { public int Requested; }

class Account
{
    private int balance;
    public Account(int initialBalance) { balance = initialBalance; }
    public void Withdraw(int amount)
    {
        if (amount > balance)
        {
            InsufficientFundsException problem = new InsufficientFundsException();
            problem.Requested = amount;
            throw problem;
        }
        balance = balance - amount;
    }
}

class Program
{
    public static int Main()
    {
        raii Account account = new Account(100);
        int caught = 0;
        try
        {
            account.Withdraw(500); // SI lanza -- se atrapa abajo
        }
        catch (InsufficientFundsException problem)
        {
            caught = problem.Requested;
        }

        try
        {
            account.Withdraw(30); // NO deberia hacer nada especial: withdraw valido, sin
                                   // excepcion -- pero el binario crashea al llegar aca
        }
        finally
        {
        }

        return caught;
    }
}
```

Compila limpio (`nsc --emit-project-oop-exe`, sin error ni warning). Al ejecutar bajo Wine 9.0,
el proceso termina con SIGSEGV (exit code 135 = 128+7 via wrap de Wine) -- nunca llega al
`return caught;`.

## Condiciones necesarias (confirmadas variando una cosa a la vez)

1. El primer `try` debe REALMENTE lanzar y ser atrapado por su `catch` -- si el primer
   `Withdraw()` no lanza nada (fondos suficientes), NO crashea (exit=0, correcto).
2. El SEGUNDO `try` debe tocar el MISMO objeto `raii` que el primero -- si se usan dos
   `Account` distintos (uno por bloque `try`), NO crashea (exit=244=500%256, correcto).
3. El cuerpo del `finally` del segundo `try` puede estar VACIO -- no hace falta que haga nada
   para que crashee.
4. Con try/catch simples (sin `finally`) tambien se probo por separado y NO crashea solo -- hace
   falta la combinacion "catch real de una excepcion" + "un try SUBSECUENTE (con o sin finally)
   sobre el mismo objeto raii, mas abajo en el mismo metodo".

## Hipotesis (no verificada contra el codigo fuente del compilador -- fuera de alcance de esta
tarea, que era construir ejemplos, no arreglar el compilador)

Huele a bookkeeping de cleanup-region/personality de excepciones (Stage 45) que no se resetea
correctamente despues de que un `catch` maneja una excepcion -- el segundo `try` sobre el MISMO
objeto raii parece heredar un estado de landing-pad/cleanup inconsistente. Comparable en espiritu
al bug real que P0.2 de la revision de `foreach` destapo en `analyzeForeach` (cleanup region
nunca cerrada) -- pero este es un caso distinto (dos `try` hermanos, no anidados) y no fue
investigado a nivel de `ProjectMethodBodyModel.cpp`/lowering LLVM en esta sesion.

## Que se hizo mientras tanto

`examples/tutorial_19_excepciones/Main.ns` se reescribio para que cada seccion de demostracion
use su PROPIO objeto `Account` -- rodea el bug, no lo tapa: el comentario en el propio ejemplo
apunta a este archivo.
