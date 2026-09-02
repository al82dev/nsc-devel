# Hallazgo crítico: ejecución real bajo Wine — Stage 52 (cierre)

Este documento registra el resultado real del intento de Task 27 (P0 3.2 de la crítica:
"ejecutar el binario resultante en Windows real"). El resultado NO es "sin Windows/Wine
disponible" como en Stage 50/51 — Wine sí se pudo instalar y ejecutar binarios reales en este
sandbox, y eso permitió encontrar y aislar con precisión un bug de compilador real,
reproducible, previamente indetectable porque **ningún binario de NativeSharp había sido
ejecutado nunca antes de esta sesión** (todas las etapas anteriores sólo verificaron COFF/PE
estructuralmente).

## Qué SÍ funciona: la máquina de PE/entry-point/exit-code es correcta

```bash
apt-get install -y --no-install-recommends wine64   # instala limpio, sin Wine previamente
/usr/lib/wine/wine64 --version                       # wine-9.0 (Ubuntu 9.0~repack-4build3)
```

Un programa NativeSharp trivial (`public static int Main() { return 42; }`), compilado con
`--emit-project-oop-exe` a un PE32+ real, **corre bajo Wine y devuelve el exit code correcto
(42)**. Esto es la primera confirmación real, por ejecución (no sólo verificación estructural
de COFF), de que el entry point, la convención de llamada del arranque, y la propagación del
exit code del compilador son correctos de punta a punta.

## Qué NO funciona: `NativeSharp_Stage52_Package`'s `Stage52Collections.exe` — y por qué

El ejecutable del ejemplo oficial de Stage 52 (y ya el binario mínimo de 2 líneas
`List<int> numbers = new List<int>(); numbers.Add(3);`) crashea bajo Wine con:

```text
err:virtual:virtual_setup_exception stack overflow NNNN bytes addr ... stack ...
```

**No es recursión real ni un stack frame legítimamente enorme.** Aislado con bisección binaria
(reduciendo `Main()` función por función, luego línea por línea) hasta un repro mínimo de 2
líneas, y **confirmado que NO es específico de generics**: el mismo patrón reproduce con una
clase no-genérica de control (`Holder.SetValue(owned int v)` que internamente llama a
`EnsureCapacity(int)`), mientras que la construcción/destrucción sola, o un método con parámetro
`owned` que NO llama a otro método, corren limpio (exit code correcto, sin overflow).

## Causa raíz identificada en el código fuente del compilador

`src/LLVM/ProjectOopLlvmLowerer.cpp`, función `emitOwnedParameterDestroy` (~línea 1743,
etiquetada "Stage 50.8" en el propio código -- **este mecanismo es anterior a Stage 52, no fue
introducido por este stage**). Genera el cleanup-pad SEH que destruye un parámetro `owned` si el
método lanza una excepción mientras ese parámetro sigue vivo. El bug:

```cpp
LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
LLVMValueRef value = LLVMBuildLoad2(builder_, ptr, found->second, "owned.param.cleanup");
// ... siempre trata found->second como si guardara un puntero de 8 bytes a un objeto con
// class descriptor, sin comprobar antes si node->typeKey es realmente reference-like.
```

Para un parámetro `owned int` (o cualquier `owned` de tipo valor), `found->second` es un
`alloca i32` de **4 bytes** -- pero este código siempre hace una carga/escritura de **8 bytes**
(pointer-sized) sobre esa dirección, lee memoria de stack fuera de los límites de la variable, y
si el valor "leído" (en realidad basura/bytes vecinos del stack) no es null, lo trata como un
puntero a un class descriptor real y **llama a un puntero a función leído de esa basura**
(`call void %owned.param.entry(...)`). Comportamiento indefinido real, no un artefacto de Wine.

Contraste: el resto del compilador (fixes de Stage 52, `move()`, etc.) ya usa un chequeo
`elementIsReferenceLike` antes de decidir si algo necesita destrucción real -- exactamente el
chequeo que falta en `emitOwnedParameterDestroy`. El código existe, el patrón ya está
establecido en otras rutas; simplemente no se aplicó nunca a esta ruta específica de limpieza-
por-excepción de parámetros `owned` de tipo valor.

**Por qué nunca se detectó antes**: el path sólo se ejecuta cuando el cleanup-pad SEH
correspondiente entra en juego (o, más precisamente, cuando su sola presencia afecta el
unwind info / layout de la función que lo contiene bajo la ABI SEH de Windows x64) -- algo que
la verificación estructural de COFF/PE (`CoffObjectVerifier`, usada en los 206 tests de esta
sesión) no ejercita, porque nunca ejecuta el binario, sólo valida su forma. Es indetectable sin
ejecución real -- exactamente el gap que Alberto señaló con el requisito P0 3.2 de la crítica.

## Alcance real del riesgo -- no es exclusivo de Stage 52

El patrón disparador es: **cualquier método con un parámetro `owned` de tipo valor que también
llama a otro método** (necesita un `invoke`/cleanup-pad). Este patrón existe en el compilador
desde Stage 50 (`emitOwnedParameterDestroy` es "Stage 50.8"), así que **esto probablemente
afecta código de etapas anteriores también**, no sólo `NativeSharp.Core.Collections.Generic`.
No se puede confirmar el alcance completo sin auditar sistemáticamente cada método con parámetro
`owned` de tipo valor que también invoca otro método -- trabajo fuera del alcance de este cierre.

## Repro package para verificación en Windows real

Tres binarios mínimos, listos para copiar y ejecutar en una máquina Windows real (ver
`WINE_CHECKLIST_STAGE52.md`):

| Binario | Qué hace | Resultado esperado (Windows real) |
|---|---|---|
| `repro_1_minimal.exe` | `return 42;`, sin `owned`/`raii` | exit code 42 |
| `repro_2_owned_no_nested_call.exe` | `owned int` param, sin llamada anidada | exit code 5 |
| `repro_3_owned_nested_call.exe` | `owned int` param + llamada a otro método (EL REPRO) | **exit code 1 esperado; si crashea, confirma el bug en Windows real** |
| `Stage52Collections.exe` | ejemplo oficial completo de Stage 52 | corre bajo Wine 9.0 en este sandbox reproduce el crash; comportamiento en Windows real desconocido |

## Qué significa esto para el cierre de Stage 52

- La superficie de `NativeSharp.Core.Collections.Generic` **compila, lowerea a LLVM limpio, y
  emite objeto/PE32+ real y estructuralmente válido** -- eso sigue siendo cierto y verificado
  (206/206 CTest).
- **No se puede afirmar "corre correctamente en Windows"** sin que Alberto confirme en una
  máquina Windows real si `repro_3_owned_nested_call.exe` (el repro mínimo) crashea o no. Si
  crashea igual que en Wine, es un bug de compilador real, de prioridad alta, que probablemente
  predata Stage 52 -- se recomienda tratarlo como bloqueante antes de dar por buena la ejecución
  de CUALQUIER etapa anterior en Windows real, no sólo Stage 52.
- Si NO crashea en Windows real, es un artefacto específico de la emulación SEH/stack-guard de
  Wine 9.0 en este sandbox, y no bloquea nada -- pero el hallazgo en el código fuente
  (`emitOwnedParameterDestroy` sin chequeo `elementIsReferenceLike`) sigue siendo una lectura de
  código real, no una suposición, y vale la pena revisarlo/corregirlo de todas formas por
  higiene, incluso si nunca se manifiesta en la práctica.
