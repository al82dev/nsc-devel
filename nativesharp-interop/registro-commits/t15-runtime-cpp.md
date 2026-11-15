# Tanda 15, cambio 1: el runtime de C++ en un ejecutable NativeSharp

## Defecto

En la [auditoría inicial](../TANDA-15-AUDITORIA-INICIAL.md), un ejecutable NativeSharp que enlaza una
biblioteca C++ con excepciones no enlazaba (`__CxxFrameHandler3`, `type_info`), y enlazando el runtime de
MSVC a mano una excepción o un `operator new` terminaban en una violación de acceso. NSC terminaba al
analizar `<type_traits>` de la STL, LLVM abortaba con `<coroutine>` y quedaban sin definir funciones inline
de `<memory>`.

Causa en el código:

- `WindowsRuntimeBuilder::buildEntry` define solo `__nativesharp_start`, y `LldLinker` enlaza con
  `/entry:__nativesharp_start` y `/nodefaultlib`: nada inicializa el runtime de C y C++ (estado por hilo de
  las excepciones, montículo, constructores globales de `.CRT$XCU`, manejadores de salida);
- `collectGlobals` (`ClangSession.cpp`) trata como variable una especialización parcial de una plantilla de
  variable, que es un `VarDecl` del namespace pero un patrón de plantilla;
- `emitCppInlineObject` (`CppLifetimeObjectEmitter.cpp`) genera el objeto del IR de Clang sin el pipeline de
  pases que Clang ejecuta incluso en -O0, donde se bajan los intrínsecos de corutina;
- `emitCppInlineIR` solo entrega al generador de código las definiciones pedidas, así que una función inline
  que llama a otra función inline o instanciada la deja como declaración.

## Diseño

- **Arranque por el CRT** (`--crt-startup`, solo con `--emit-project-oop-exe`): el módulo de entrada define,
  además de `__nativesharp_start`, una función `main` que inicializa las excepciones NativeSharp y llama al
  `Main` del programa bajo el mismo filtro de `panic`, pero devuelve el resultado en vez de terminar el
  proceso. El enlace entra por `mainCRTStartup`, que inicializa el runtime de C y C++, ejecuta los
  constructores globales, llama a `main` y, con su resultado, los manejadores de salida y los destructores
  globales. Un `panic` y un fallo al iniciar siguen terminando el proceso (142, 253). El runtime estático de
  MSVC lo da el proyecto con `--link-lib` (`libcmt`, `libvcruntime`, `libcpmt`, `libucrt`, `kernel32`): NSC no
  busca Visual Studio. La identidad del objeto de entrada y del producto incluye el modo de arranque.
- **Enlazador**: NSLink no acepta `libucrt.lib`, así que un producto con `--crt-startup` se enlaza con
  lld-link; si `NSC_LINKER` elige otro enlazador, la orden se rechaza con el motivo.
- **Plantillas de variable**: `collectGlobals` omite los `VarDecl` con plantilla (`isTemplated`).
- **Definiciones alcanzadas**: después de las pedidas, `emitCppInlineIR` ofrece al generador de código cada
  definición con enlace vago o interno de la unidad (inline, instanciaciones de plantilla, variables inline e
  internas), reunidas después de que Sema instancie lo que usan las pedidas. Clang las difiere y emite solo
  las que alcanzan los cuerpos emitidos, con enlace `linkonce_odr` en COMDAT, como al compilar la unidad.
- **Pipeline de Clang**: `emitCppInlineObject` ejecuta el pipeline del nivel de optimización (`default<On>`)
  antes de generar el objeto.

## Cambio

- `WindowsRuntimeBuilder.cpp/.h`: `buildStart` con símbolo y retorno; `setCrtStartup`; `main` en la entrada.
- `WindowsExecutableBuilder.cpp/.h`, `LldLinker.cpp/.h`: modo de arranque, identidad y símbolo de entrada.
- `CompilerInvocation.cpp/.h`, `CompilerOptionParser.cpp`, `CompilerSession.cpp`, `DriverExecutionScope.h`,
  `PrecompiledModuleConfiguration.cpp`, `DriverArtifactGeneration.cpp`: la opción `--crt-startup`, su
  validación, su identidad y la elección de lld-link.
- `ClangSession.cpp`: `collectGlobals` omite plantillas; `emitCppInlineIR` ofrece las definiciones alcanzadas.
- `CppLifetimeObjectEmitter.cpp`: pipeline antes de emitir.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Los fixtures de los cambios 2 a 4 ([excepciones](t15-excepciones.md), [callbacks](t15-callbacks.md),
[recursos](t15-recursos.md)) usan este cambio. Antes de él, los experimentos de la auditoría reproducen cada
defecto (`A:/temp/nsc-t15-exp-8staq55u`, `A:/temp/nsc-t15-bisect-6v3xbvny`, `A:/temp/nsc-t15-x-36j35yod`);
con él, `<type_traits>`, `<utility>`, `<memory>`, `<vector>`, `<string>`, `<coroutine>` y `<functional>` se
analizan (`A:/temp/nsc-t15-stl-y62i4ync`).

Regresión: en la validación agrupada de la tanda (`TANDA-15-VALIDACION-AGRUPADA.md`).

## Límites

- El runtime de C++ es el estático de MSVC (`/MT`); el dinámico (`/MD`) no se probó.
- Una función inline de la STL que usa la espera atómica necesita además `synchronization.lib`.
