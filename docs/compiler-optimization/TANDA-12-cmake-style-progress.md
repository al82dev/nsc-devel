# Tanda 12: progreso estilo CMake

## Objetivo

Mostrar actividad continua y comprensible durante una construcción NativeSharp, incluyendo los módulos LLVM que se procesan en ese momento.

## Cambios realizados

- Las órdenes de construcción de OBJ, EXE, LIB, DLL, header y NSMETA activan progreso automáticamente.
- `--quiet` desactiva tanto la salida normal como el progreso automático.
- Las fases usan líneas `-- NSC: <fase>` y reportan su finalización y duración.
- Lectura, comptime, lexing y parsing muestran contadores `[actual/total]`, fase y archivo.
- `ProjectOopObjectGenerator` acepta un sink de progreso y anuncia el inicio de cada módulo LLVM.
- Cada módulo terminado incrementa un contador visible `[actual/total]` con numeración serializada y monotónica, aunque el trabajo se ejecute en paralelo.
- `--progress` sigue disponible para activar la misma salida en órdenes diagnósticas.
- La ayuda y las expectativas dirigidas del driver se actualizaron al nuevo formato y a la reutilización del grafo de proyecto.

## Archivos modificados

- `src/nsc/main.cpp`
- `src/Driver/Driver.cpp`
- `src/Frontend/ProjectCompilation.cpp`
- `include/nativesharp/LLVM/ProjectOopObjectGenerator.h`
- `src/LLVM/ProjectOopObjectGenerator.cpp`
- `tests/unit/DriverTests.cpp`

## Archivos nuevos

- Este informe. No se creó un subsistema nuevo; se conectaron los sinks de progreso que ya delimitaban las fases con el backend LLVM.

## Mejora esperada

Una compilación larga deja de parecer bloqueada. El usuario ve qué fase está activa, qué fuente acaba de procesarse, cuántas faltan y qué particiones LLVM están ejecutándose, con salida inmediata y vaciada en cada evento.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- El target dirigido `nsc_driver_tests` compila con las nuevas expectativas. Su ejecutable completo no se usó como gate de tanda porque también recorre O2 y numerosas rutas de enlace ajenas a esta feature.
- Construcción real de Stage 46 sin `--progress`: mostró automáticamente todas las fases, el archivo fuente y cuatro módulos LLVM con contadores ordenados 1/4 a 4/4.
- La misma construcción con `--quiet`: correcta y sin salida de progreso.
- No se ejecutó baseline completo.
