# Stage 53.6 -- implementacion paso a paso

Ver `56_stage53_6_directory_enumeration.md` para el diseno completo y el razonamiento. Este
documento resume el orden real en que se toco cada capa.

1. **Design lock-in**: Alberto pregunto "que falta de 53?" (solo enumeracion de directorios),
   despues "que debemos decidir?" (la forma de la API). `AskUserQuestion` presento las dos
   opciones (cuatro metodos vs un par parametrizado por booleano); Alberto pregunto "que harias
   tu?"; la recomendacion (Opcion A, cuatro metodos) se confirmo con "hagamozlo como comentas".
2. **`lib/NativeSharp.Core.IO/Directory.ns`**: reescrito el comentario de cabecera de la clase con
   el diseno completo de esta ronda; agregados los cuatro metodos stub
   (`GetFileCount`/`GetFiles`/`GetDirectoryCount`/`GetDirectories`) con comentarios de diseno
   individuales.
3. **Frontend, capa por capa** (mismo patron de las 4 capas + runtime de cada ronda anterior):
   - `ProjectMethodBodyModel.h`/`.cpp`: reconocimiento de los cuatro metodos (dentro del
     `if`-block combinado File/Directory existente, con el booleano `isFile` ya establecido para
     distinguir), cuatro `MethodCoreOperationKind` nuevos.
   - `ProjectOopHirModel.h`/`.cpp`: cuatro `ProjectOopHirNodeKind` nuevos.
     `GetFileCount`/`GetDirectoryCount` se sumaron al bucket compartido de un-operando (junto a
     `DirectoryDelete`/`PathGetFullPath`); `GetFiles`/`GetDirectories` cayeron en el bucket por
     DEFECTO de dos operandos (el mismo que `EnvironmentSetVariable`/`WriteAllText`/
     `AppendAllText` ya usan) sin necesitar ninguna entrada explicita nueva -- el bucket nunca
     exigio que el SEGUNDO operando fuera especificamente un string, asi que un `string[]` en esa
     posicion entra sin cambiar el mecanismo.
   - `ProjectOopMirModel.h`/`.cpp`: cuatro `ProjectOopMirNodeKind` nuevos, `toString()` y
     conversion HIR->MIR, calco letra por letra del patron de
     `CoreEnvironmentGetCommandLineArgCount`/`CoreEnvironmentGetCommandLineArgs`.
4. **`ProjectOopLlvmLowerer.cpp`**: cuatro simbolos de runtime nuevos, cuatro entradas en
   `declareRuntime()` (tipos `i32(ptr)` para los dos Count, `i32(ptr,ptr)` para los dos Get --
   ninguna de las cuatro usa el patron "retorno i1 dummy" de las operaciones `void`, porque las
   cuatro devuelven un `int` realmente significativo), cuatro bloques de lowering.
5. **`WindowsRuntimeBuilder.cpp`**: declaraciones Win32 de `FindFirstFileA`/`FindNextFileA`/
   `FindClose` (primer uso de las tres en este runtime); dos helpers compartidos nuevos
   (`buildDirectorySearchPattern`, `buildAnsiCStringToNativeSharpString`) y dos helpers de
   filtrado (`buildDirectoryEntryMatchesKind`, `buildDirectoryEntryIsPseudo`) factorizados para no
   duplicar la logica de atributo/pseudo-entrada cuatro veces; dos funciones constructoras
   compartidas (`buildDirectoryGetEntryCount`, `buildDirectoryGetEntries`), cada una parametrizada
   por `bool wantDirectories` (booleano de C++, tiempo de generacion de codigo, NO de runtime) y
   llamada dos veces para producir los cuatro simbolos LLVM exportados.
6. **Linker** (`WindowsExecutableBuilder.cpp` + `WindowsLibraryBuilder.cpp`): tres simbolos nuevos
   en `kernel32Symbols` (30->33) en ambos archivos.
7. **Build limpio**: `cmake --build build --target nsc` compilo sin errores en el primer intento
   tras completar las capas 3-6 (incluyendo el paso 5, la funcion runtime mas larga escrita hasta
   ahora en este proyecto en numero de lineas).
8. **Sincronizacion de los tres tests de host-integration**: a diferencia de Stage 53.5 (donde
   este paso fue una correccion posterior, descubierta por la suite completa), esta vez las tres
   copias locales de `kernel32Symbols` (`Stage47HostCAbiTests.cpp`,
   `Stage48_3CHostIntegrationTests.cpp`, `Stage48_4CppHostIntegrationTests.cpp`) se actualizaron
   desde el principio, aplicando la leccion que Stage 53.5 dejo documentada: cualquier simbolo
   Win32 nuevo en el modulo runtime debe sincronizarse en las CINCO ubicaciones, no solo en las
   dos "reales" del linker.
9. **Ejemplo + probe manual bajo Wine ANTES del harness definitivo**:
   `examples/project_stage53_6` -- puebla un directorio con tres archivos y dos subdirectorios,
   verifica conteos y contenido de las cuatro operaciones sin asumir orden, prueba el caso de
   buffer mas chico que la cantidad real, y el caso de directorio inexistente. Probado a mano
   compilando con `nsc --emit-project-oop-exe` a O0 y O2, despues corriendo bajo
   `/usr/lib/wine/wine64` en un directorio de trabajo dedicado -- exit code 0 en ambos niveles de
   optimizacion, directorio de trabajo completamente vacio despues de correr (confirmado desde
   Linux). Ademas un control negativo manual: una copia temporal de `Main.ns` con
   `fileCount != 999` en vez de `!= 3` confirmo exit code 1 (el programa realmente discrimina, no
   pasa de forma vacia) antes de restaurar el archivo original.
10. **`tests/unit/Stage53_6DirectoryEnumerationTests.cpp`**: fixture minima + ejemplo empaquetado
    en O0/O2 + el mismo helper `runUnderWine` portable (`std::system()`, sin codigo POSIX
    especifico) que cada test de ejecucion real de Stage 53 ya usa. 45/45, estable en tres
    corridas.
11. **Suite completa**: 217/218 -- el unico fallo (`nsc.stage52-4.owned-cleanup-runtime`) es el
    mismo hallazgo preexistente ya documentado desde Stage 53.5, no relacionado con esta ronda.
    (Una segunda corrida encontro ademas un `nsc.stage50_7.borrow-escape-analysis` fallando por
    `BAD_COMMAND` -- un binario de 0 bytes sin permiso de ejecucion, un artefacto de entorno sin
    relacion con Stage 53.6; `chmod +x` lo arreglo y quedo verde en la corrida siguiente.)
12. **`CMakeLists.txt`**: registrado `nsc_stage53_6_directory_enumeration_tests` /
    `nsc.stage53-6.directory-enumeration`.
13. **Cierre de Stage 53**: con esta ronda, las seis subetapas de Stage 53 (Time, Environment,
    Environment variables, Filesystem/Paths x3, Command-line arguments, Directory enumeration)
    quedan completas. Stage 54 (threading) sigue bloqueado hasta nueva senal de Alberto.
