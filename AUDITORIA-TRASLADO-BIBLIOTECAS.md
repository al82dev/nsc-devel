# Auditoría del traslado de las bibliotecas estándar a NativeSharp Libraries

Fecha: 2026-11-18. Rama: `development_cpp__clang_integracion` (merge `61eb49519`). Bibliotecas:
`A:\nativesharp-libraries`, rama `development` (`d8de3ca`, con `Float64x2.ns` registrado).

## Comprobado

- **Contenido.** Los 5 paquetes estándar (`NativeSharp.Core`, `.Collections.Generic`, `.Threading`, `.IO`,
  `.Diagnostics`) del antiguo `lib/` están completos en `stdlib/`: 88 archivos idénticos, 22 con cambios y
  ningún archivo `.ns` perdido. En los 22 no se pierde API pública: `Span` y `StringView` marcan como `readonly`
  los mismos métodos y `Algorithm` añade uno. El repositorio externo añade 19 archivos, entre ellos
  `Float64x2`, `Arc`/`Rc` y la familia `Shared*`. Las diferencias de comportamiento proceden de las
  correcciones LIB-4xx del repositorio externo; no se revisaron una por una.
- **Copias internas.** En el compilador quedan `lib/NativeSharp.LLVM.C`, que se distribuye, y
  `lib/NativeSharp.Win32.Compiler`, que usan el selfhost y sus pruebas. Ambas son necesarias. Los 5 `.nsproj`
  de Core en `lib/` solo redirigen al repositorio externo. Hay 5 `.md` duplicados en `lib/`: 2 idénticos al
  externo y 3 distintos. `tests/cases/sema/nsc.cc1.generic-constraints/fixture/lib`, con 44 archivos, es un
  fixture congelado, no una copia del producto.
- **Búsqueda.** CMake usa `NSC_LIBRARIES_SOURCE_ROOT`, por defecto `../nativesharp-libraries`, y lo pasa a las
  pruebas. `nsc` busca los paquetes en `<bin>/../packages/<paquete>/*/windows-x64/O{n}`; acepta
  `NSC_CORE_LIBRARY_ROOT` como alternativa. Se verificó que `LibraryReference Include="NativeSharpCore.lib"`
  resuelve al paquete del nivel de optimización en curso: un programa con `Float64x2` y `Float32x4` devuelve
  42 en O0-O3.
- **Orden.** NSC Builder construye primero el compilador y después las bibliotecas, con el `nsc.exe` de esa
  misma construcción. En la ejecución 20261118-090430 se compilaron 460 unidades con `build-clang\bin\nsc.exe`,
  que se instalaron en `build-clang\packages`.
- **`Float64x2`.** Está en Core y el proyecto lo incluye mediante `**/*.ns`. Aparece en el NSMeta construido
  en O0-O3 y es utilizable: lo confirman el programa de prueba y los fixtures de vectores, que dan el mismo
  resultado que Clang.

## Hallazgos pendientes

1. **Redirecciones relativas en las raíces limpias.** `cmake/VerifyNativeSharpFirstGeneration.cmake` y
   `cmake/VerifyNativeSharpFixedPoint.cmake` copian `lib/NativeSharp.Core` (y Collections/IO) a
   `OUTPUT_DIR/clean-*/lib`. Esos `.nsproj` solo contienen `../../../nativesharp-libraries/...`, que desde la
   copia resuelve a `OUTPUT_DIR/nativesharp-libraries`, una ruta inexistente. Las reglas son idénticas en
   `development`. El hallazgo procede de un análisis estático: la prueba exige `NS_ROOT` y no se pudo ejecutar.
2. **Plantillas precompiladas con Core como fuente.** Una biblioteca precompilada cuya plantilla importa un
   namespace de Core (`using NativeSharp.Core.Numerics;`) falla con «unknown namespace» cuando el consumidor
   referencia el proyecto fuente de Core. Con el Core construido funciona. El fallo se reproduce en
   NativeSharp puro con el `nsc` de `development`. Procede de las importaciones de fuente de las plantillas y
   de la visibilidad del proyecto precompilado; el reemplazo de origen ocurre en `ProjectGraph.cpp`.
3. **Paquetes desactualizados.** `build-integracion\packages` se construyó a las 03:18 con una versión
   anterior del compilador de ese árbol y `nsc` lo usa como `<bin>/../packages`.
4. **Salida compartida.** Todos los compiladores escriben en `nativesharp-libraries\build\O{n}` antes de
   instalar; dos construcciones simultáneas se pisarían.
5. **Restos de versión en el SDK externo.** Los `.nsproj` de las 5 bibliotecas declaran
   `<Language>3</Language>` y comentarios sobre `C:\native-sharp-3.0`. El compilador ignora el campo, así que
   no selecciona versión.
6. **Compilador por defecto de la herramienta.** `nativesharp_libraries.py` usa `C:\native-sharp\bin\nsc.exe`
   cuando no recibe `--compiler`; NSC Builder siempre se lo pasa.
7. **`nsc.precompiled-syntax-node-index`** sigue contando el corpus del antiguo `lib/` ("corpus found 6") y
   también falla en `development`.

## Resolución

1. **Redirecciones relativas.** Corregido en `01b1974f3` con `cmake/NativeSharpCleanRootLibraries.cmake`. Se
   comprobó con una raíz limpia de prueba: la copia literal falla y la reescrita compila y ejecuta un programa
   que usa `Float64x2`. `nsc.selfhost.first-generation` sigue requiriendo `NS_ROOT` y no se ejecutó.
2. **Plantillas con Core como fuente.** Corregido en `b47480afc`: el proyecto sintético ve los proyectos fuente
   de los módulos que nombran sus tipos externos. La reproducción en NativeSharp puro pasa en O0-O3 y el caso C
   de vectores devuelve 130828 con y sin encabezado.
3. **Paquetes desactualizados.** `build-integracion\packages` se reconstruyó con su propio compilador. Ese
   directorio no forma parte de git.
4. **Salida compartida.** NativeSharp Libraries `41d76a9` añade `--build-root`. `stdlib_build.py` construye
   ahora en `<prefijo>/nativesharp-libraries-build`, propio de cada construcción del compilador.
5. **Restos de versión.** NativeSharp Libraries `41d76a9` elimina `languageEdition` y `<Language>` de los 42
   proyectos. Ahora el catálogo rechaza que aparezca cualquiera de los dos.
6. **Compilador por defecto.** Se conserva. `C:\native-sharp` es el compilador instalado que usan la GUI,
   `nslibs.cmd` y las sondas de ese repositorio; NSC Builder siempre pasa `--compiler`, y el log registra el
   compilador usado. No es un defecto.
7. **`precompiled-syntax-node-index`.** Corregido en `01b1974f3`: el corpus incluye `stdlib`. La prueba pasa.

## No verificado

- La revisión semántica de cada cambio en los 22 archivos modificados.
- Las pruebas de selfhost de primera generación y punto fijo: requieren `NS_ROOT`, y el selfhost no compila
  en `development` (`ModuleSet.ns`).
- `nsc.cpp-interop.session`: faltan los fixtures `cpp-fixtures`.
- Cualquier instalación en `C:\native-sharp`, que no se tocó.
