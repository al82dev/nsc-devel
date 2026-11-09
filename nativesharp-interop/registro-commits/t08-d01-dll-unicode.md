# T08-D01: DLL NativeSharp con nombre no ASCII, cargada por ruta Unicode

Fecha: 2026-11-09. Casillas: T08-D01, T08-002-09m7c0g2d, T08-002-09m7c0g2 y T08-002-09m7c0g.

## Problema

El cargador de Windows lee el nombre de cada DLL de la tabla de importación con la página ANSI (CP1252 en este
equipo). Una DLL NativeSharp como `Lib漢.dll` no se puede importar por nombre: el consumidor terminaba con
`STATUS_DLL_NOT_FOUND`, aunque la DLL estuviera junto al ejecutable. Además, NSLink no podía enlazarla, así que NSC
la rechazaba con un diagnóstico.

## Solución: biblioteca cargadora Unicode

Cuando el nombre de archivo de la DLL no es ASCII, el productor publica una **biblioteca cargadora** con el nombre
de la biblioteca de importación (`Lib漢.lib`), en lugar de la de importación habitual.

- **Forma de importación.** Cada exportación `X` conserva su forma: un thunk `X` (`jmp *__imp_X(%rip)`) y una
  ranura `__imp_X`. Los consumidores NativeSharp y C la enlazan sin cambios, tanto con llamada directa como con
  `__declspec(dllimport)`.
- **Ranura inicial.** Apunta a un stub diferido por exportación (`movl $i, %eax; jmp común`).
- **Cola común.** Tiene datos de desenrollado SEH. Guarda `rcx`, `rdx`, `r8`, `r9` y `xmm0`–`xmm3`, llama al
  resolvedor, restaura y salta a la exportación. Los argumentos en registros y en pila llegan intactos.
- **Resolvedor.** En la primera llamada:
  - toma la ruta del módulo que enlazó la biblioteca (`GetModuleHandleExW` sobre sí mismo y
    `GetModuleFileNameW`);
  - sustituye el nombre de archivo por el de la DLL en UTF-16, que va como constante;
  - carga la DLL con `LoadLibraryExW(..., LOAD_WITH_ALTERED_SEARCH_PATH)` y guarda el módulo;
  - resuelve con `GetProcAddress` y publica la dirección en la ranura (`store atomic release`).

  El búfer de ruta se toma del heap del proceso: un consumidor C sin CRT no tiene `__chkstk`.
- **Errores.** Como el cargador: una DLL ausente lanza `STATUS_DLL_NOT_FOUND` y una exportación ausente
  `STATUS_ENTRYPOINT_NOT_FOUND`, ambas no continuables.
- **Concurrencia.** Varios hilos pueden resolver a la vez. Todos obtienen el mismo módulo; las cargas adicionales
  solo suben la cuenta de referencias, y todos publican la misma dirección.
- **Directorio.** La DLL se busca junto al módulo que la usa, la misma regla que sigue el cargador para una
  importación por nombre.

## Cambios

| Archivo | Cambio |
|---|---|
| `src/Linker/UnicodeDllLoader.cpp` y `Internal/UnicodeDllLoader.h` (nuevos) | Genera el módulo LLVM del cargador (IR textual con `module asm` para los stubs), lo emite y lo archiva con `llvm-lib` |
| `src/Linker/WindowsLibraryBuilder.cpp` | Tras enlazar una DLL con nombre no ASCII, sustituye su biblioteca de importación por la cargadora, en los dos caminos (monolítico y particionado). Si falla, borra la DLL, la biblioteca y el PDB |
| `src/Linker/LldLinker.cpp` | Se retira el rechazo de NSLink. La DLL se enlaza con el nombre ASCII `__nsc_output.dll` (directorio de exportaciones) y se publica con el nombre pedido, igual que un ejecutable |
| `src/Driver/LibraryReferenceValidation.cpp`, `Internal/DriverInternal.h` y `DriverArtifactGeneration.cpp` | La comprobación «Runtime declarado alcanzado» acepta una DLL no ASCII cuando el PE enlazado lleva su nombre UTF-16 terminado en NUL, la constante del cargador. Una biblioteca que no corresponde a esa DLL sigue rechazada |
| `CMakeLists.txt` | Agrega `UnicodeDllLoader.cpp` a `nsc_linker` |

Una DLL con nombre ASCII no cambia: misma biblioteca de importación y mismos argumentos de enlace.

## Pruebas

**Nueva:** `pruebas-tanda-08-identidad-c/rutas-unicode/validar-dll-unicode.py`. Aprobada.

- **Nombres y enlazadores.** Cinco nombres (NFC, NFD, acento combinante, U+6F22 y signo Kelvin), con NSLink y con
  lld-link.
  - El consumidor NativeSharp llama métodos con argumentos enteros y flotantes en registros y en pila, un
    accesor de campo estático y una función `export extern C`.
  - U+6F22 con NSLink se prueba en O0–O3, tanto productor como consumidor.
- **Consumidor C** (Clang + lld-link, sin CRT). Llama a `dl_add` por declaración simple y por `__imp_dl_add`
  desde ocho hilos a la vez, de modo que la primera resolución compite. Con NSLink y con lld-link.
- **Negativos.**
  - Sin la DLL, el consumidor termina con `0xC0000135`.
  - Con una DLL a la que le falta `Mix`, la llamada a `Mix` termina con `0xC0000139`, después de resolver `Get`.

**Actualizada:** `rutas-unicode/validar-nombre-salida.py`. Fijaba el rechazo de NSLink y no consumía las DLL de
lld-link. Ahora construye y consume las cinco DLL no ASCII con los dos enlazadores en O0 y O2 con `-g`. Comprueba:

- que con NSLink el directorio de exportaciones lleva `__nsc_output.dll`;
- que el RSDS nombra el PDB publicado;
- que `LoadLibraryW` las carga;
- que el consumidor se ejecuta.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d01.cmd`, resultado 0:

- `rutas-unicode`: `validar-dll-unicode.py`, `validar-nombre-salida.py`, `validar.py` y
  `validar-equivalencias.py`;
- `tls-dll-unicode/validar.py`, `tls-dll-producto/validar.py` y
  `tls-dll-distintas/validar-directo-precompilado.py`;
- DLL ASCII: `tls-c/validar-dll.cmd`, `tls-c/precompiled/validar-dll-precompilado.cmd`,
  `record-c/dll-bool/validar.cmd`, `atomic-global-dll/validar.cmd` y `tls-c/lld/validar-dll-emision-auto.cmd`.
