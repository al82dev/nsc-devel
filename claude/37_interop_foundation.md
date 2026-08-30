# Stage 55.x -- Interop Foundation: el bloque completo, cerrado

Alberto pidio los cinco puntos del gate en un solo bloque, sin checkpoints. Este documento cierra
la ronda: certificacion cruzada C/MSVC, callbacks C→NativeSharp, FFI generica (imports dinamicos
y estaticos), inputs externos del linker, punteros en el pipeline OOP, y el estado exacto de la
superficie de tipos. Todo medido antes y despues, PE real bajo Wine, `-O0` y `-O2`.

El objetivo estrategico del gate, en palabras del propio roadmap: *"que una API C desconocida
pueda entrar y salir de NativeSharp sin tocar el frontend/HIR/MIR/lowerer especificamente para
ella"*. **Eso ya es cierto**, y la prueba es que los tests de esta ronda declaran
`GetSystemMetrics`, `GetCurrentProcessId` y una .lib de clang inventada para la ocasion sin que
el compilador contenga una sola linea que los nombre.

## 1. Certificacion cruzada C/MSVC ↔ NativeSharp -- CERRADA

La pieza que faltaba desde 40.2: un host C compilado por clang con ABI MSVC, linkeado por lld
contra una **lib estatica NativeSharp**, ejecutando bajo Wine e intercambiando structs campo a
campo:

- 8B (`V2`) directo, 12B (`V3`) indirecto, 64B (`M4`) via sret -- **en ambas direcciones**;
- el original del host queda intacto tras la llamada (semantica de copia por llamada, verificada
  desde el lado C);
- **exit 0 con lado NS a -O0 y a -O2**.

Y la respuesta al gate de riesgo de apertura: **un host con `main()` propio maneja el runtime NS
sin que el entry de NativeSharp haya corrido jamas** -- el heap y todo el runtime funcionan
lazy. Nadie habia ejecutado nunca esa combinacion (Stage 48.3 linkeaba y verificaba el PE, no lo
corria).

El generador de headers C ya emitia los structs >8B con `NS_STATIC_ASSERT` de tamaño, alineacion
y offset por campo -- el host compila con `-Wall -Wextra -Werror` contra ese header y las
aserciones son la doble contabilidad del layout.

## 2. Callbacks C → NativeSharp -- CERRADOS (con un P0 medido en el camino)

**El P0**: `Adder f = AddImpl;` con `Adder` extern C delegate **compilaba y miscompilaba** --
construia la celda ordinaria de delegate NS (code+receiver) y la registraba donde C esperaba
codigo. Invocarla ejecutaba los bytes de la celda: exit 5 para un programa cuya respuesta es 42.
(Los extern C delegates solo se habian *recibido* desde hosts; nadie habia creado uno desde NS.)

**El arreglo no necesita thunk**: desde Stage 40.2 la firma LLVM de la funcion interna ES el ABI
C de Win64 para todo tipo C-expresable (agregados indirectos incluidos), asi que la creacion
devuelve **la direccion de la funcion misma**. Certificado por ejecucion: el host recibe los
punteros via registrars y llama codigo NativeSharp **directamente desde C** --

- un transform entero (21→42),
- un callback que recibe un **M4 de 64 bytes** entrando a codigo NS,
- una firma **WndProcLike** (`nint, uint, nint, nint → nint`) con dispatch por msg.

Negativos con assert de mensaje: metodo de instancia ("a C function pointer has no receiver") y
lambda ("no closure environment") no convierten.

Nota de semantica preexistente, documentada: la inicializacion de estaticos de tipo se dispara
en call sites NS; un callback invocado desde C no la dispara. Igual que en todo delegate NS
actual; se hereda, no se agrava.

## 3. FFI generica -- CERRADA

### La tabla unica primero

Los 62 simbolos de kernel32 estaban **duplicados verbatim** entre `WindowsExecutableBuilder.cpp`
y `WindowsLibraryBuilder.cpp` (mas kernelbase/shell32/ucrt). Ahora viven en UN componente
(`WindowsRuntimeImports`) que ambos builders consumen y por el que tambien entran los imports
del proyecto. **La regla de "sincronizar kernel32Symbols en 5 lugares" queda retirada**: hay un
lugar, y lo consume todo el mundo.

### Las dos grafias

```csharp
public class User32 {
    extern C from "user32.dll" {          // import DINAMICO
        public int GetSystemMetrics(int index);
    }
}
public class CLib {
    extern C {                            // resolucion ESTATICA (via --link-lib)
        public float clib_sum_v3(V3 v);
    }
}
```

Cada declaracion es un miembro estatico sin cuerpo; el simbolo C es el nombre del miembro; los
pares (dll, simbolo) fluyen del frontend al linker y la import library se materializa sola. Los
tipos se validan con las MISMAS reglas C-ABI de los exports (string rechazado con el diagnostico
existente; `ushort*` es la convencion LPCWSTR y compila -- la declaracion con forma de
`MessageBoxW` esta en la suite).

**Por que dos grafias y no una** (medido, no teorizado): enrutar una .lib estatica por el camino
dinamico acuñaba thunks de import contra un DLL inexistente -- el link "funcionaba" y el loader
fallaba en runtime (exit 53). `extern C` sin `from` declara y deja la resolucion al link, que es
exactamente el contrato de un archive.

### Verificado por ejecucion

- **Dinamico**: `user32!GetSystemMetrics` + `kernel32!GetCurrentProcessId` (simbolo que la tabla
  del runtime NUNCA tuvo -- camino de DLL nueva y de simbolo nuevo) + `kernel32!Sleep` (que SI
  estaba -- camino de merge/dedupe). Exit 42, y el PE importa `user32.dll` verificablemente.
- **Estatico**: `--link-lib mathlib.lib` con un archive compilado por clang en el propio test;
  V3 (12B) y M4 (64B) cruzan hacia codigo C foraneo y vuelven. Exit 42.

## 4. Inputs externos del linker -- `--link-lib`

El driver paso de **cero** opciones de link a `--link-lib <ruta.lib>` (repetible), extraida en la
normalizacion de argumentos como `-O`, asi que funciona con todas las acciones de emision (exe y
dll). Los paths van directos a la linea de lld.

## 5. Punteros en el pipeline OOP -- `&local` / `*p` / `(*p).campo`

El muro *"Stage 34 OOP lowering keeps raw pointer address/dereference in the procedural backend
boundary"* cayo. Como todos los locals son memory-homed desde 40.2, `&local` es literalmente el
alloca del operand:

- `&x` → nodo wrapper `AddressValue` (tipo `T*`). **Leccion medida**: la primera version mutaba
  el typeKey del nodo operando y un `ParameterValue` reescrito cargaba el agregado `[24 x i8]`
  donde iba el puntero ("GEP base pointer is not a vector..."). Se envuelve, nunca se muta.
- `*p` como valor → `PointerLoad` (load tipado del pointee; structs cargan su carrier).
- `*p` como destino/`(*p).campo` → el valor del puntero ES la direccion; GEP y store directos.

Es el **tier crudo** del lenguaje -- la tercera categoria de referencia que Bloque 1 dejo
documentada: sin chequeo de lifetime, cuelga si sobrevive a su objetivo, por diseño; las APIs C
se expresan exactamente con esta forma. Verificado: `Fill(&msg)` estilo `GetMessage`,
read-modify-write via `int*`, copia de struct de 64B via `*(&big)`, todo 42/42.

## 6. Superficie de tipos FFI -- estado exacto

| Pieza | Estado |
|---|---|
| primitivos, punteros (`T*`, `void*`, `ushort*`), enums, structs por valor (cualquier tamaño), extern C delegates | **funcionan y estan certificados cross-host** |
| `string` en firmas C | **rechazado** (correcto -- un string NS no es un `char*`); convencion: `ushort*` = LPCWSTR, `byte*`/`nint` segun API |
| alias de tipo (`using HWND = nint`) | no existe; **convencion**: bindgen emite `nint` directo. Feature de ergonomia futura |
| arrays embebidos (`float[4]` en struct) | **parcial medido**: el TIPO parsea, pero `c.rgba[i]` no baja en OOP ("index expression requires an indexer reference or a pointer..."). Workaround certificado: campos nombrados (M4 = 16 floats nombrados, cross-host OK). Necesario para Vulkan; NO bloquea Win32 basico |
| unions / layout explicito | pendiente (el propio gate lo marca "posteriormente"); necesario para `INPUT` de Win32 |

## Suite: `Stage55XInteropFoundationTests.cpp` -- 8 escenarios, 29 checks

Imports dinamicos ejecutando contra user32/kernel32 reales; archive estatico de clang via
`--link-lib` intercambiando structs; punteros OOP; creacion de extern C delegate (el programa
que antes daba 5); negativos de instancia/lambda/string; y la certificacion cross-host completa
(header generado → clang `-Werror` → lld → Wine → exit 0 con 14 chequeos C del lado host).
clang/lld/Wine se detectan como en las suites de Stage 47/48: si falta la herramienta, el
escenario se degrada a skip explicito, nunca a rojo falso.

## Verificacion global

- Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON`: limpio.
- `ctest`: **232/232, 100%** (las 231 previas + interop-foundation).
- Manifest regenerado; verificado **dentro del zip extraido**; extraccion aislada
  configure+build+ctest desde cero.

## Lo que esto desbloquea (el diagrama del gate, actualizado)

```text
            Stage 40.2 ✅            Interop Foundation ✅
        aggregate ABI interno     imports/libs/callbacks/punteros
                 │                            │
        Stage 59 Math ✅ libre        Stage 56 Win32: ABIERTO
                                   (CreateWindowExW sale por FFI,
                                    WndProc entra por function pointer,
                                    GetMessage(&msg) tiene &)
```

`WndProc` puede entrar desde C y `CreateWindowExW` puede salir por FFI generica: **los bindings
(Win32, Vulkan, OpenGL, OpenAL, SDL, GLFW) pueden evolucionar en paralelo como proyectos
NativeSharp ordinarios**, que era el punto que buscaba el gate completo.

## Pendientes descendientes (ninguno bloquea empezar bindings)

1. Arrays embebidos en structs (Vulkan) y unions/layout explicito (`INPUT`).
2. Alias de tipos (ergonomia de bindgen).
3. `ref`/`out`/`in` como azucar sobre los punteros ya funcionales.
4. Ventana real bajo Wine (CreateWindowExW + pump): el smoke definitivo de Stage 56 -- ya no hay
   pieza de compilador conocida entre medio.
