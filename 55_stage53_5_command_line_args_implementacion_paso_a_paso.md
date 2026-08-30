# Stage 53.5 -- implementacion paso a paso

Ver `55_stage53_5_command_line_args.md` para el diseno completo y el razonamiento. Este documento
resume el orden real en que se toco cada capa.

1. **Design lock-in**: dos rondas de `AskUserQuestion` con Alberto confirmaron "una sola
   implementacion via `CommandLineToArgvW` + tests exhaustivos de quoting" como diseno final.
2. **`lib/NativeSharp.Core/Environment.ns`**: agregados `GetCommandLineArgCount()` y
   `GetCommandLineArgs(string[] destination)`, con comentarios de diseno completos.
3. **Frontend, capa por capa** (mismo patron de las 4 capas + runtime de cada ronda anterior):
   - `ProjectMethodBodyModel.h`/`.cpp`: reconocimiento de los dos metodos, dos
     `MethodCoreOperationKind` nuevos.
   - `ProjectOopHirModel.h`/`.cpp`: dos `ProjectOopHirNodeKind` nuevos --
     `GetCommandLineArgCount` se sumo al bucket de cero-argumentos (refactorizado de ternario a
     if-chain); `GetCommandLineArgs` se sumo al bloque compartido de un-operando (el mismo bloque
     que ya usan `PathGetFullPath`/`FileDelete`, sin cambios de mecanismo -- nunca exigio que el
     operando fuera especificamente un string).
   - `ProjectOopMirModel.h`/`.cpp`: dos `ProjectOopMirNodeKind` nuevos, `toString()` y conversion
     HIR->MIR, calco letra por letra del patron de `CoreDirectoryDelete`/`CorePathGetFullPath`.
4. **`ProjectOopLlvmLowerer.cpp`**: dos simbolos de runtime nuevos, dos entradas en
   `declareRuntime()` (tipos `i32()` y `i32(ptr)` -- NINGUNA de las dos usa el patron "retorno i1
   dummy" que usan las operaciones que devuelven `void`, porque ambas devuelven un `int`
   realmente significativo), dos bloques de lowering.
5. **`WindowsRuntimeBuilder.cpp`**: declaraciones Win32 de las cuatro funciones *W/utilitarias
   (`GetCommandLineW`, `CommandLineToArgvW`, `WideCharToMultiByte`, `LocalFree`);
   `buildEnvironmentGetCommandLineArgCount` (simple); `buildEnvironmentGetCommandLineArgs` (la
   funcion mas compleja del proyecto -- ver la nota sobre el bug de terminador-en-medio-de-bloque
   corregido antes de la primera compilacion, documentada en el doc principal).
6. **Linker** (`WindowsExecutableBuilder.cpp` + `WindowsLibraryBuilder.cpp`): tres simbolos nuevos
   en `kernel32Symbols` (27->30), bloque nuevo de import-library para `SHELL32.dll`
   (`CommandLineToArgvW`), agregado a los vectores `importLibraries`/`libraries`.
7. **Build limpio**: `cmake --build build --target nsc` compilo sin errores en el primer intento
   tras completar las capas 3-6.
8. **Ejemplo + probe manual bajo Wine ANTES del harness definitivo**: `examples/project_stage53_5`
   con la matriz de quoting de seis argumentos; probado a mano con `python3 subprocess` (argv
   explicito, sin shell) contra los binarios O0 y O2 -- exit code 0 en ambos, mas dos casos
   negativos de control (conteo incorrecto -> exit 1, contenido incorrecto -> exit 4) para
   confirmar que el programa realmente discrimina, no que pasa de forma vacia.
9. **`tests/unit/Stage53_5CommandLineArgsTests.cpp`**: fixture minima + ejemplo empaquetado en
   O0/O2 + el harness `runUnderWineWithArgs` (`std::system()` + escaping de comilla simple,
   ver el doc principal para el porque). 43/43, estable en dos corridas.
10. **Efecto colateral detectado por la suite completa**: los tres archivos de test de
    host-integration (`Stage47HostCAbiTests.cpp`, `Stage48_3CHostIntegrationTests.cpp`,
    `Stage48_4CppHostIntegrationTests.cpp`) fallaban por simbolos no resueltos al linkear
    directamente contra la biblioteca estatica (su copia local de `kernel32Symbols` no tenia los
    tres simbolos nuevos, y les faltaba el import de SHELL32.dll). Corregido replicando
    exactamente el mismo bloque que ya tienen para `KERNELBASE.dll`/`kernel32Symbols`.
11. **Suite completa**: 216/217 -- el unico fallo (`nsc.stage52-4.owned-cleanup-runtime`) es un
    hallazgo preexistente no relacionado con esta ronda, documentado en el doc principal.
12. **CMakeLists.txt**: registrado `nsc_stage53_5_command_line_args_tests` /
    `nsc.stage53-5.command-line-args`.
13. **Bug real de build reportado por Alberto (Windows/clang-cl)**: la primera version del
    harness del paso 9 usaba `fork()+execvp()` con headers POSIX sin proteger, que no existen bajo
    `clang-cl` -- `'sys/wait.h' file not found`. Corregido quedandose dentro del `std::system()`
    portable que ya usa toda la suite (ver el doc principal para el detalle completo). Reconstruido
    y re-verificado (43/43, dos corridas) antes de reempacar los ZIPs finales.
