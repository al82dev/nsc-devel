# Tanda 02: construcción opcional de interoperabilidad C/C++

Estado: **verificada para los criterios de la tanda 02 el 2026-11-05**. La cobertura global de interoperabilidad
pertenece a las tandas posteriores y no se infiere de esta validación de construcción.

- `A:/nsc-devel/CMakeLists.txt` compara `LLVM_PACKAGE_VERSION` con `23.1.0` cuando
  `NSC_ENABLE_CPP_INTEROP=ON` y busca Clang `23.1.0 EXACT`. Resuelve con `file(REAL_PATH)` los prefijos de LLVM y
  Clang y rechaza una diferencia. Los paquetes de `A:/al82dev-clang/lib/cmake/{llvm,clang}` indican 23.1.0.
- El target estático `nsc_cpp_interop` incorpora las fuentes bajo `src/Interop/Cpp` enumeradas en CMake; publica
  `A:/nsc-devel/include` y usa privadamente los headers de LLVM y Clang. Enlaza `nsc_compilation`,
  `clangTooling` y `clangCodeGen`. La lista efectiva y sus dependencias se obtienen de `build.ninja`.
- El caché ON señala `NSC_HOST_LINKAGE=STATIC`, `clang-cl` y `lld-link` de `A:/al82dev-clang/bin`. El comando
  generado de enlace de `nsc.exe` usa ese `lld-link`; `build.ninja` registra `-MT` para el runtime anfitrión.
- El LLVM instalado registra `LLVM_ENABLE_EH=OFF` y `LLVM_ENABLE_RTTI=OFF` en `LLVMConfig.cmake`. Los flags
  generales de NSC incluyen `/EHsc`; la regla generada para `ThinLtoBackend.cpp` incluye además `/GR-`. Esas
  observaciones corresponden a objetivos y fuentes diferentes: no se atribuyen los flags de LLVM a todo NSC.
- LLVM registra zlib, zstd y libxml2 como opciones habilitadas en su paquete. Para esta configuración de NSC,
  `CMakeLists.txt` deshabilita su búsqueda automática externa y utiliza targets estáticos incluidos de zlib y
  zstd; `build.ninja` lista `nsc-zlib.lib` y `nsc-zstd.lib` en el enlace. No se ha identificado libxml2 en el
  enlace generado; esa observación no prueba que toda capacidad de LLVM carezca de dicha dependencia.
- `openCppGraphSessions` omite los proyectos sin `CppLibraryReference`; la prueba de la tanda también debe
  demostrar que el driver no llama a la fábrica de sesiones para un proyecto normal aunque la integración esté
  habilitada. `tests/unit/CppClangSessionTests.cpp` instala una fábrica que falla si se invoca, prepara un grafo
  sin referencias con el entorno de Clang habilitado y exige cero invocaciones y cero sesiones.

Comandos de configuración existentes, ejecutados desde CMD y con el entorno definido por el script del proyecto:

```cmd
call A:\llvm-project-al82dev\nativesharp-interop\configurar-nsc-tanda1.cmd OFF
call A:\llvm-project-al82dev\nativesharp-interop\configurar-nsc-tanda1.cmd ON ON
```

El segundo argumento activa el target de pruebas de sesión. Los comandos anteriores documentan la configuración;
durante el cierre se reutilizaron los árboles configurados y no se ejecutaron de nuevo. La evidencia de los casos
negativos anteriores para versión y mezcla de instalaciones está en [línea base](LINEA-BASE-TANDAS.md).

Validación conjunta del 2026-11-05:

- `cmake --build A:\nsc-interop-build\tanda1-ON --target nsc nsc_cpp_session_tests`: código 0; construcción
  incremental. `cmake --build A:\nsc-interop-build\tanda1-OFF --target nsc`: código 0.
- `A:\nsc-interop-build\tanda1-ON\bin\nsc_cpp_session_tests.exe A:/al82dev-clang/lib/clang/23 A:/nsc-interop-build A:/al82dev-clang/bin/clang.exe`:
  código 0, incluida la prueba de cero invocaciones para el proyecto sin referencias.
- `llvm-readobj --coff-imports` del `nsc.exe` ON mostró `KERNEL32.dll`, `ADVAPI32.dll`, `ntdll.dll`,
  `VERSION.dll`, `OLEAUT32.dll`, `SHELL32.dll` y `ole32.dll`; no se observaron DLL dinámicas del CRT ni LLVM.
- `clang.exe --version` informó 23.1.0. No se desplegó ni se modificó el compilador instalado en C:.
