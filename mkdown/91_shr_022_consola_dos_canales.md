# 91 — SHR-022: `Console.Write` y stderr. Un nodo, dos bits

**Fecha:** 2026-09-02
**Qué cierra:** SHR-022 — el segundo ítem del Bloque 2.
**Estado:** implementado, compilado en el contenedor, **pendiente de la corrida de Windows.**

---

## 1. El hueco, medido antes de escribir

`lib/NativeSharp.Core/Console.ns` **entero**, antes de esto:

```csharp
public class Console
{
    public static void WriteLine(string value)
    {
    }
}
```

Un método. Cuerpo vacío — es una operación núcleo y el compilador la sustituye por
`GetStdHandle` + `WriteFile` en `WindowsRuntimeBuilder.cpp`.

Lo que no había:

* **no había `Write`** — todo llevaba salto de línea obligatorio;
* **no había stderr** — no había forma de escribir un diagnóstico desde el lenguaje.

Un compilador manda diagnósticos por error estándar y resultados por salida estándar. Con un solo
canal, `nsc-next` no puede emitir un solo diagnóstico sin ensuciar su propia salida. Por eso el doc
68 puso este ítem por delante de casi todo el Bloque 2.

---

## 2. La decisión: cuatro métodos, UN nodo, dos bits

```csharp
public static void Write(string value)
public static void WriteLine(string value)
public static void ErrorWrite(string value)
public static void ErrorWriteLine(string value)
```

Las cuatro bajan al **mismo** `MethodCoreOperationKind` → mismo nodo HIR → mismo nodo MIR → **una**
función de runtime:

```
__nativesharp_console_write(ptr texto, i32 modo)
        bit 0 = añade CRLF
        bit 1 = escribe por error estándar
```

La máscara se fija **en compilación**, en el HIR, a partir del nombre del método:

```cpp
const bool appendNewline = core->kind == MethodCoreOperationKind::ConsoleWriteLine ||
    core->kind == MethodCoreOperationKind::ConsoleErrorWriteLine;
const bool useStandardError = core->kind == MethodCoreOperationKind::ConsoleErrorWriteLine ||
    core->kind == MethodCoreOperationKind::ConsoleErrorWrite;
node->integerValue = (appendNewline ? 1u : 0u) | (useStandardError ? 2u : 0u);
```

**Por qué un nodo y no cuatro.** La alternativa —cuatro operaciones núcleo con cuatro cuerpos de
runtime— multiplica por cuatro el sitio donde vive la lógica de `WriteFile`, y esa lógica es
idéntica salvo *qué handle* y *si hay un segundo `WriteFile` para el CRLF*. Cuatro copias de una
regla es exactamente cómo se separan: se arregla una y las otras tres se quedan como estaban. Es la
misma disciplina que el Stage 51 estableció para `String` — un núcleo pequeño, una superficie
grande encima.

Las cuatro capas siguen existiendo (`MethodCoreOperationKind` tiene cuatro enumeradores) porque el
**reconocimiento** sí es por nombre. Lo que se comparte es todo lo que hay debajo del HIR.

---

## 3. Por qué NO es `Console.Error.WriteLine`

Se lee mejor. Y pide un objeto: `Error` tendría que ser una propiedad que devuelve un `TextWriter`,
con despacho de instancia y un tipo entero detrás.

**`TextWriter` es SHR-020 y vive en el Bloque 5.** Adelantarlo para ganar un punto en el nombre
habría metido un tipo completo y su modelo de despacho dentro del Bloque 2 — y el Bloque 2 existe
precisamente para no tener que hacer eso.

Cuando `TextWriter` llegue, estas cuatro **siguen siendo las primitivas de debajo** y la forma con
objeto se construye encima sin tocar el backend. La decisión no se paga dos veces.

---

## 4. Lo que se encontró al escribir el runtime

**El handle se pedía dos veces.** `buildConsoleWriteLine` llamaba a `GetStdHandle` una vez para el
texto y otra para el CRLF. Ahora se pide **una** y se reutiliza. No era un defecto observable —dos
llamadas dan el mismo handle— pero era una llamada al sistema por línea impresa, y el sitio donde
se arregla es este.

**`LLVMBuildSelect` no estaba en la frontera.** `src/LLVM/LlvmCApi.h` es una **enumeración a mano**
de los símbolos de la API C de LLVM que el proyecto usa — es SHR-090, "congelar el subconjunto".
Elegir entre `STD_OUTPUT_HANDLE` (-11) y `STD_ERROR_HANDLE` (-12) según un bit del modo necesita un
`select`, y `select` no estaba.

Ampliar esa frontera **es un acto deliberado**, así que va documentado en el propio header:

```cpp
// SHR-022. Ampliacion deliberada de la frontera enumerada con LLVM: un simbolo mas, para elegir
// entre dos constantes sin abrir un bloque basico. La alternativa era aritmetica sobre el modo
// (`-11 - ((mode >> 1) & 1)`), que no habria anadido simbolos pero esconde la intencion.
```

La alternativa aritmética era real y funcionaba. Se descartó: ahorrar un símbolo enumerado a cambio
de que el código diga menos de lo que hace es un mal cambio.

---

## 5. Qué se tocó

| Archivo | Qué |
|---|---|
| `include/nativesharp/Frontend/ProjectMethodBodyModel.h` | Tres enumeradores nuevos junto a `ConsoleWriteLine` |
| `src/Frontend/ProjectMethodBodyModel.cpp` | Reconocimiento generalizado a un mapeo nombre→kind (misma clase, estático, void, un `builtin:string`); tres `case` en el switch de nombres |
| `src/Frontend/ProjectOopHirModel.cpp` | Las cuatro bajan a `CoreConsoleWriteLine` con la máscara de dos bits |
| `src/Frontend/ProjectOopMirModel.cpp` | La máscara pasa a `immediate` |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | El gancho pasa a `__nativesharp_console_write`, tipo `{ptr, i32}`, aridad 2 |
| `src/Runtime/WindowsRuntimeBuilder.cpp` | `buildConsoleWriteLine` → `buildConsoleWrite(texto, modo)`; handle una sola vez |
| `src/LLVM/LlvmCApi.h` | **+1 símbolo**: `LLVMBuildSelect` |
| `lib/NativeSharp.Core/Console.ns` | Los cuatro métodos |

---

## 6. Cómo está medido

`tests/cc1/c22_console_and_formatting`, ejecutando, salida 42. La parte de SHR-022:

* **Tres `Write` seguidos y un `WriteLine`** producen **una** línea. Si `Write` saltara de línea
  saldrían cuatro, y eso se ve en el log de la corrida.
* **Lo mismo por stderr, en la misma corrida.** Escribir por los dos canales en la misma ejecución
  es lo que mide que la máscara de dos bits llega entera hasta el runtime: con un solo canal
  probado, un bit invertido pasaría desapercibido.
* **Un `string` nulo escribe la línea vacía** en vez de reventar, que es lo que hacía `WriteLine`
  desde siempre y lo que un log a medias necesita.
* Y el criterio del bloque entero: un diagnóstico formateado **por stderr**.

Todo el lado C++ compila con `clang++ -std=c++20 -fsyntax-only` en el contenedor antes de tocar la
máquina. Es la regla que se adoptó después de la tercera vez que algo se entregó y tronó.
