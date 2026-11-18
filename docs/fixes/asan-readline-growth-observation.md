# Control dirigido de crecimiento de lectura con ASan

`A:\nsc-devel-fixes-agent-1-lambda\tests\integration\asan_readline_growth_observe.py` creó un proyecto en A, compiló y ejecutó su programa en O0 y O2 tanto con `--sanitize-address` como sin él. En las cuatro combinaciones la compilación devolvió 0 y la ejecución devolvió 42: la lectura aceptó una línea de 8192 bytes y comprobó longitud y posiciones 0, 4096 y 8191. El log completo, comandos y hashes anteriores/posteriores de compilador, Core, metadata, Clang y runtime ASan están en `A:\nsc-fixes-agent1-temp\asan-readline-growth-20261112.log`; los artefactos están en `A:\nsc-fixes-agent1-temp\nsc-asan-readline-j9gzbzhj`. El proceso y sus hijos usaron directorio inicial del repositorio aislado y temporales en A. Se fijó `ASAN_OPTIONS=detect_leaks=0` para ambos modos.

Por lectura, `src/Runtime/WindowsRuntimeBuilder.cpp:615-630` llama `HeapReAlloc` al crecer el búfer de `Console.ReadLine`. La entrada larga y el resultado son evidencia funcional de esta ejecución, pero no se registró una traza dinámica que pruebe cuántas veces se ejecutó el adaptador ni si se movió la memoria. La inspección `llvm-readobj --coff-imports` del ejecutable ASan O0 mostró el runtime dinámico y símbolos como `__asan_init`, `__asan_realloc` y `__asan_report_load1`; no se guardó salida de esa inspección en archivo. Las importaciones no prueban que cada acceso o biblioteca binaria esté instrumentado. No se realizó control negativo de error de memoria en este fixture; no acredita detección ASan ni ausencia de fugas. El compilador contiene WIP y no representa una revisión limpia.

## Idoneidad como regresión permanente

El prototipo todavía no está listo para registro permanente. Antes de integrarlo conviene:

1. Hacer selección optativa en CMake, con skip 77 cuando la configuración opcional de Core o ASan no exista; una ruta expresamente configurada pero inexistente debe fallar. No asumir rutas particulares de máquina en el test.
2. Documentar y validar argumentos: compilador, Core y metadata, Clang, runtime dinámico ASan, runtime de NativeSharp e import libraries; comprobar que los paths de dependencias y sus hashes no cambien. El prototipo no registra hashes del runtime NativeSharp ni las import libraries.
3. Registrar explícitamente opciones ASan y configuración de entrada/salida, preservar salida de inspección si se quiere afirmarla, y usar un control negativo ya existente para comprobar detección, sin depender de que el nuevo caso provoque corrupción.
4. Comprobar el contenido completo o un patrón más amplio si se pretende afirmar preservación completa: ahora solo se verifican tres posiciones. Si el objetivo es acreditar ejecución de realloc, añadir una observación específica que no dependa de que el asignador mueva memoria; no atribuirla únicamente a la longitud de entrada.
5. Mantener O0/O2 y controles sin ASan, y verificar rutas/cwd/TEMP de los procesos. El archivo actual devuelve 1 si faltan argumentos o dependencias y conserva directorios de evidencia, lo que necesita adaptarse a la convención de prueba optativa antes de registrarlo.

No se editó la rama principal ni se observó un fallo en el caso ejecutado. Compatibilidad general de realloc, instrumentación total y ausencia de fugas permanecen sin demostrar.
