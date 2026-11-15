# Tanda 16: auditoría inicial

Base: `c42a52c74` (cierre de la tanda 15). Clang 23.1.0, objetivo `x86_64-pc-windows-msvc`.

La sonda `pruebas-tanda-16-auditoria/sonda.py` (no es una prueba de soporte) compila cada caso con Clang y
un consumidor NativeSharp, con NSLink y con lld-link. Los casos con observación externa escriben una marca
con Win32 (`CreateFileA`/`WriteFile`), sin el CRT. Evidencia: `A:/temp/nsc-t16-sonda-5rncjimz`; registro en
`build-claude-t08/logs/sonda-t16.log`. Los experimentos están en `build-claude-t08/tools/exp-t16-*.py`.

## Lo que hace NSC hoy

- **Punto de entrada (T16-001).** Un ejecutable NativeSharp entra por `__nativesharp_start`
  (`WindowsRuntimeBuilder::buildStart`), que inicializa las excepciones NativeSharp, llama a `Main` y termina
  el proceso con su resultado. Con `--crt-startup` (tanda 15) entra por `mainCRTStartup`, que inicializa el
  runtime de C y C++ y llama a un `main` que entra en NativeSharp.
- **Entradas del enlace (T16-002).** Los objetos del programa, el runtime NativeSharp
  (`NativeSharp.Runtime.Win32.lib`), los objetos que NSC emite para el código C++ inline, las bibliotecas de
  `CppLibraryReference` (estáticas o de importación), las bibliotecas NativeSharp precompiladas y las de
  `--link-lib`. Con `--crt-startup`, el runtime estático de MSVC: `libcmt`, `libvcruntime`, `libcpmt`,
  `libucrt` y `kernel32` (y `synchronization` si se usa la espera atómica). El enlace es con
  `/nodefaultlib`.
- **Constructores globales sin el CRT (T16-003).** Una biblioteca estática con un entero de inicialización
  dinámica enlaza y el programa devuelve 0 en vez de 7, con los dos enlazadores: el constructor no se
  ejecuta y no hay diagnóstico. Causa: Clang pone el puntero al inicializador en `.CRT$XCU` (o `.CRT$XIU`
  para C); solo `_initterm` del arranque del CRT recorre esas secciones, y `__nativesharp_start` no lo hace.
  Nada en NSC inspecciona las entradas en busca de inicializadores.
- **Constructor y destructor de un objeto global (T16-004, T16-005).** Sin el CRT, el enlace falla
  (`atexit`, `__CxxFrameHandler3`, `__std_terminate`). Con `--crt-startup`, devuelve 7 y la marca es `cd`:
  el constructor corre antes de `main` y el destructor al salir.
- **TLS (T16-006, T16-007).** Un `thread_local` con constructor leído por el hilo principal y por un
  `std::thread` de la biblioteca, con `--crt-startup`, devuelve 1020: cada hilo construye su copia. Sin las
  bibliotecas del CRT, el enlace de una biblioteca con TLS falla: `_tls_index` sin definir para TLS estático
  (`exp-t16-tls2.py`) y además `__dyn_tls_init`, `__tls_guard` y `__dyn_tls_on_demand_init` para TLS con
  inicialización dinámica (`exp-t16-tls.py`, sección `.CRT$XDU`). El TLS estático con la entrada de
  NativeSharp y las bibliotecas del CRT como referencias ya está cubierto (tandas 8 y 10).
- **DLL con objetos globales (T16-012, T16-013).** Una DLL C++ enlazada con su propio runtime estático
  devuelve 7 con marca `cd` con los dos enlazadores: el arranque de la DLL (`_DllMainCRTStartup`) ejecuta el
  constructor al cargarse y el destructor al descargarse.
- **Enlace separado y duplicados (T16-014, T16-015).** Dos bibliotecas que definen `A::one` se rechazan al
  enlazar con el nombre del símbolo: NSLink `NSL1021` (`?one@A@@YAHXZ`), lld-link `duplicate symbol: int
  __cdecl A::one(void)`.
- **LTO (T16-016).** Una biblioteca compilada con `-flto=thin` contiene bitcode: NSLink la rechaza
  (`NSL1003 ... not AMD64 (0x4342)`) y lld-link la enlaza (42). Sin `NSC_LINKER`, NSC elige lld-link para una
  entrada estática con bitcode o TLS (`requiresLldForStaticInput`).
- **Bibliotecas del CRT (`exp-t16-crtsec.py`).** De `libcmt`, `libvcruntime`, `libcpmt` y `libucrt`, solo
  el miembro `excptptr.obj` de `libcpmt` tiene `.CRT$XCU`: un diagnóstico que mire la biblioteca entera y no
  los miembros que toma el enlace rechazaría las referencias al CRT que ya usan las tandas 8 y 10.

## Plan

Ver [SCOPE-LOCK-TANDA-16.md](SCOPE-LOCK-TANDA-16.md).
