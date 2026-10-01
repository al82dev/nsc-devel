# Tanda correctiva — retiro de .NET y C++/CLI

**Fecha:** 1 de octubre de 2026

**Rama:** `development`

**Plataforma:** Windows x64

**Validación:** configuración y construcción dirigida O0; baseline no ejecutado

## Decisión de producto

NativeSharp 3.0 no incluye interoperabilidad administrada con C#, .NET o C++/CLI. Esos hosts
dejan de formar parte del producto, roadmap y baseline. La superficie externa certificada queda
limitada a ABI C y hosts nativos C/C++.

Esta decisión no cambia la ABI MSVC de Windows x64 ni retira UCRT o el runtime nativo de Visual
C++. Tampoco modifica la estrategia `/MD` vigente. Se eliminan únicamente dependencias del
ecosistema administrado: `dotnet`, MSBuild, CLR, proyectos C# y proyectos C++/CLI.

## Cambios

- `CMakeLists.txt`: elimina los targets y registros CTest de Stage 48.5 y Stage 48.6.
- `examples/project_stage48_5`: elimina el host C# P/Invoke, su proyecto y su script .NET.
- `examples/project_stage48_6`: elimina el bridge C++/CLI, host C#, proyectos MSBuild/.NET y
  script de extremo a extremo.
- `tests/unit/Stage48_5CSharpPInvokeHostTests.cpp`: eliminado.
- `tests/unit/Stage48_6CppCliHostClosureTests.cpp`: eliminado.
- `DLL_HANDOVER_AND_ROADMAP.md`: limita DLL-P8 a C/C++ nativo y documenta expresamente que el
  runtime nativo de Visual C++ permanece permitido.
- comentarios del compilador y tests: sustituyen referencias administradas obsoletas por el
  contrato vigente de consumidores ABI C nativos.
- `nscbuilder.py`: elimina del comentario del filtro CTest el nombre de la prueba retirada; el
  descubrimiento continúa siendo dinámico desde CMake.

## Efecto en el baseline

El baseline no mantiene una lista fija: descubre las pruebas registradas por CMake. Al retirar
ambos `add_test`, el inventario pasa de 571 a 569 pruebas por linker. Ya no construye, ejecuta ni
certifica componentes que requieran `dotnet`, MSBuild, Visual Studio o CLR.

Stage 48 conserva como certificación oficial:

- Stage 48.1: header C;
- Stage 48.2: metadata de biblioteca;
- Stage 48.3: host C nativo;
- Stage 48.4: host C++ nativo.

## Validación realizada

- CMake regeneró `build-lsp-debug` correctamente.
- `nsc` construyó en Debug/O0.
- inventario CTest: 569 pruebas.
- `nsc.stage48_5.csharp-pinvoke-host`: ausente.
- `nsc.stage48_6.cppcli-stage48-closure`: ausente.
- sintaxis de `nscbuilder.py`: correcta.
- búsqueda sobre compilador, baseline, tests y ejemplos activos: ninguna conexión administrada;
  sólo permanece la exclusión normativa explícita del roadmap.
- directorios temporales de Stage 48.6 eliminados.
- no se ejecutó baseline ni se modificó `C:\native-sharp`.

## Mejora

La certificación vuelve a medir el producto que se entrega. Una instalación de NativeSharp 3.0
ya no puede fallar porque exista o falte un SDK administrado ajeno al compilador. La dependencia
nativa de Visual C++ permanece intacta y separada de esta decisión.
