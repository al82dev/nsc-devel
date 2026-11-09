# T08-D05: DLL TLS homónimas con dependencias transitivas homónimas

Fecha: 2026-11-09. Casillas: T08-D05 y T08-002-09m7c1d2c ([decisión anterior](t08-dll-homonimas-transitivas-decision.md)).

## Problema

Dos `api.dll` homónimas y distintas ya se resolvían con alias TLS (`__nsc_tls_<hash>.dll`) cargados por ruta. Si
cada una importaba su propia `dep.dll`, homónima pero distinta, NSC rechazaba el paquete:

```
runtime package contains different DLLs named 'dep.dll': '…/1/dep.dll' and '…/2/dep.dll'
```

El cargador de Windows resuelve una importación por nombre base y reutiliza un módulo ya cargado con ese nombre,
sin importar su carpeta. Por eso, aunque los dos alias estén en carpetas distintas, acaban usando la misma
`dep.dll`.

## Mecanismo de aislamiento evaluado

Los **contextos de activación** (ensamblados side-by-side privados) son el mecanismo de Windows para que un
proceso tenga cargados dos módulos con el mismo nombre base. Con un contexto activo, el cargador redirige el nombre
importado a la ruta del archivo del ensamblado y busca por ruta completa, como ocurre con las dos versiones de
`comctl32`.

La sonda `claude/sondas/sxs1` (C puro, Clang y LLD) lo comprobó sin modificar ninguna DLL:

- con contexto: `alias1 → dep (101)` y `alias2 → dep (102)`;
- sin contexto, el control reutiliza la primera `dep.dll` (`101, 101`).

`CreateActCtxW` falla con `ERROR_SXS_CANT_GEN_ACTCTX` (14001) si el nombre del ensamblado es largo: el hash
completo de 64 dígitos falló y 16 dígitos funcionan. Por eso el nombre usa los últimos 16 dígitos del hash del
alias, y una colisión dentro del paquete se rechaza.

## Cambio

**Empaquetado** (`RuntimeClosurePackaging.cpp`, `packageRuntimeClosure`). Cada raíz conserva su propio cierre de
dependencias hermanas. Un nombre con contenidos distintos se acepta si todas las raíces cargadas que lo alcanzan
cumplen dos condiciones:

- son alias TLS que el cargador de alias carga por ruta, y no aparecen en la tabla de importación del producto;
- el nombre en conflicto no es la propia raíz.

Para cada alias afectado, NSC publica:

- `NativeSharp.Isolation.<16 hex>/`, con la versión del alias de cada nombre en conflicto y el manifiesto del
  ensamblado (`<assemblyIdentity …/>` y un `<file name="dep.dll"/>` por archivo);
- `<alias>.dll.nsc-isolation.manifest`, el manifiesto de contexto que depende de ese ensamblado.

Los nombres sin conflicto siguen junto al producto. Las DLL se copian byte a byte: no se modifica ninguna, ni se
altera su firma.

Una raíz que el producto no importa ni carga en ejecución no participa en la comprobación de conflictos. Es el caso
de una `dep.dll` declarada como `Runtime` solo para empaquetarla.

Se siguen rechazando:

- un conflicto alcanzado desde una DLL de la tabla de importación (cargada antes de que exista un contexto) o
  desde un runtime cargado por nombre;
- un alias que el producto importa directamente. Pasa cuando una función solo existe en una de las DLL homónimas.
  El diagnóstico añade:

  ```
  C TLS alias '…' is imported directly, so its dependencies cannot be isolated
  ```

**Cargador de alias** (`OopLlvmCalls.cpp`, `__nsc_cpp_dll_tls_export`). Antes de `LoadLibraryExW` del alias,
comprueba con `GetFileAttributesW` si existe `<ruta del alias>.nsc-isolation.manifest`. Si existe, carga el alias
así:

1. `CreateActCtxW` (ACTCTXW de 56 bytes, con `lpSource`);
2. `ActivateActCtx`;
3. `LoadLibraryExW`;
4. `DeactivateActCtx` y `ReleaseActCtx`.

Un manifiesto presente pero inválido hace fallar el enlace del import (trampa de enlace). No se recurre en
silencio a la carga sin contexto. El manifiesto se compone en el mismo búfer UTF-16 de la ruta, y el terminador se
restaura después.

**Importaciones del runtime** (`WindowsRuntimeImports.cpp`). Se añaden `CreateActCtxW`, `ActivateActCtx`,
`DeactivateActCtx` y `ReleaseActCtx` de KERNEL32.

**Publicación y caché**:

- `ArtifactPublisher::addRuntimeArtifacts` y `validatePackagedCppRuntimeDestinations` publican con la ruta relativa
  (`runtimePackageRelativePath`): un archivo de ensamblado conserva su carpeta.
- `CompilationInputManifest::addRequiredBundleRelativeOutput` registra salidas con un subdirectorio.
- `CompilationBundleCache` acepta nombres de artefacto con un subdirectorio (sin raíz, sin `.` ni `..`, como
  mucho dos componentes) y los convierte a separadores nativos. Un nombre con `/` unido a una ruta larga `\\?\`
  fallaba con `ERROR_INVALID_NAME`.
- Un nombre empaquetado solo dentro de ensamblados ya no se exige como salida junto al producto
  (`addRuntimeBundleOutputs`).

**Producto DLL.** `buildDynamicLibraryProduct` devuelve los runtimes cargados dinámicamente para que
`commitDynamicLibraryProduct` aplique el mismo empaquetado.

## Efectos visibles

- **Disposición del producto.** Junto al producto aparecen las carpetas `NativeSharp.Isolation.*` y los archivos
  `*.nsc-isolation.manifest`. Forman parte del paquete: al copiar la aplicación hay que copiarlos con ella. Si falta
  un manifiesto, el alias no encuentra su `dep.dll` y el programa falla en la primera llamada, en lugar de usar
  otra versión.
- **Integridad.** Las DLL no cambian, así que sus firmas Authenticode y sus hashes siguen coincidiendo con los del
  proveedor. Los manifiestos son XML sin firmar, generados por NSC.

## Pruebas

**Nueva:** `pruebas-tanda-08-identidad-c/tls-dll-distintas/validar-transitivas-homonimas.py`. Aprobada.

- **Aislamiento O0–O3.** El consumidor observa la TLS inicial de cada alias (1 y 2) y `api_dep()` = 101 y 102.
  El paquete contiene dos ensamblados con copias exactas de cada `dep.dll` y ninguna `dep.dll` en la raíz, y las
  DLL de origen no cambian.
- **Caché.** Tras borrar la salida, la segunda construcción restaura de la caché el ejecutable, los alias, los
  ensamblados y los manifiestos, y el programa se ejecuta.
- **Manifiesto retirado.** El consumidor falla (`0x87`, trampa de enlace) y no usa otra `dep.dll`.
- **Producto DLL NativeSharp** (O0 y O2). Aplica el mismo aislamiento junto a sí mismo. Un anfitrión Python
  (`ctypes`) lo carga y `product_check()` devuelve 0.
- **Rechazo explícito con la causa.** Un alias importado directamente, por una función que solo exporta una de las
  DLL.
- **Control.** Con `dep.dll` iguales hay una sola `dep.dll` en la raíz y ningún artefacto de aislamiento.
- **Rechazo conservado.** Una DLL sin alias (en la tabla de importación) que alcanza otra `dep.dll`.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d05.cmd`, resultado 0. Reúne:

- el conjunto de D04: TLS y DLL, `tls-dll-distintas` con `validar-mismo-nombre.cmd` (invocado por ruta absoluta),
  y los validadores `.cmd` de TLS y DLL;
- el conjunto de D01: rutas Unicode y TLS DLL;
- el conjunto de D02: calificadores, callbacks, typedef, globales, plantillas y caché C directa;
- la prueba nueva.

El arnés de regresión de D04 ejecutaba `tls-dll-distintas/validar.cmd` sin su argumento obligatorio y
`validar-mismo-nombre.cmd` por ruta relativa. Ese validador borra los fixtures `uno/dos` antes de fallar, así que
dejaba sin preparar a `validar-directo-precompilado.py`. Ahora se invoca primero, por ruta absoluta, y prepara los
fixtures. Es un problema del arnés, no del compilador.

## Pendiente (T08-D05b)

Un alias importado directamente no se puede aislar: la tabla de importación lo carga antes de que exista ningún
contexto. Para admitirlo, las funciones exclusivas de un alias que necesita aislamiento deben enlazarse en
ejecución con el cargador de alias, igual que las funciones con origen ambiguo. La decisión tendría que tomarse en
HIR con el mismo análisis de cierres. Mientras tanto, el caso se rechaza con la causa.
