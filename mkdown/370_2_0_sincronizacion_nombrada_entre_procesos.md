# 370 — Sincronización nombrada entre procesos / Named cross-process synchronization

## Español

`NativeSharp.Core.Threading.Mutex`, `Semaphore` y `Event` conservan sus constructores anónimos y
añaden sobrecargas que reciben un `string name`. El nombre se convierte a una cadena ANSI terminada
en cero y se entrega a `CreateMutexA`, `CreateSemaphoreA` o `CreateEventA`. Procesos independientes
que usan el mismo nombre abren el mismo objeto del kernel de Windows.

El primer proceso que crea un semáforo o evento establece su conteo, modo de reset y estado inicial;
las aperturas posteriores reutilizan el objeto existente, según el contrato de Win32. Los métodos de
espera, liberación, señalización, reset y cierre son los mismos de las variantes anónimas, por lo que
no se duplica la ABI operativa.

La prueba del ítem compila dos ejecutables NativeSharp separados. El primero espera un evento
nombrado y el segundo abre ese nombre y lo señala; ambos deben terminar con código 42. También valida
el lowering de los tres constructores y los wrappers del runtime.

## English

`NativeSharp.Core.Threading.Mutex`, `Semaphore`, and `Event` retain their anonymous constructors and
add overloads accepting a `string name`. The name is copied to a null-terminated ANSI string and
passed to `CreateMutexA`, `CreateSemaphoreA`, or `CreateEventA`. Independent processes using the same
name open the same Windows kernel object.

The first process creating a semaphore or event establishes its count, reset mode, and initial state;
subsequent opens reuse the existing object according to the Win32 contract. Wait, release, signal,
reset, and close operations are shared with the anonymous variants, avoiding a duplicate operational
ABI.

The item test builds two separate NativeSharp executables. The first waits on a named event and the
second opens that name and signals it; both must exit with code 42. It also validates all three
constructor lowerings and their runtime wrappers.
