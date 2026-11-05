# Línea base observada para las tandas de interoperabilidad

Fecha: 2026-11-03. Esta evidencia es una observación de configuración y no certifica una construcción o prueba nueva.

| Elemento | Evidencia |
|---|---|
| Rama NSC | `git -C A:\nsc-devel status --short --branch`: `development-cpp-clang`; sin cambios locales listados. |
| Revisión NSC | `git -C A:\nsc-devel rev-parse HEAD`: `152654e5a0e2d84d4c2f245ea7590488f0497430`. |
| Rama LLVM | `git -C A:\llvm-project-al82dev status --short --branch`: `nativesharp-daikon-interop`; `nativesharp-interop/` no rastreado y preexistente. |
| Revisión LLVM | `git -C A:\llvm-project-al82dev rev-parse HEAD`: `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`. |
| Clang | `A:\al82dev-clang\bin\clang.exe --version`: 23.1.0, objetivo `x86_64-pc-windows-msvc`, commit LLVM anterior. |
| Paquete LLVM | `A:/al82dev-clang/lib/cmake/llvm/LLVMConfig.cmake`: `LLVM_PACKAGE_VERSION 23.1.0`; calcula `LLVM_INSTALL_PREFIX` a partir de su ubicación real. |
| Paquete Clang | `A:/al82dev-clang/lib/cmake/clang/ClangConfig.cmake`: calcula su prefijo a partir de su ubicación real y solicita LLVM 23.1.0 exacto; `ClangConfigVersion.cmake` indica 23.1.0. Ambos paquetes observados están bajo `A:/al82dev-clang`. |
| Recursos Clang | `A:/al82dev-clang/bin/clang.exe -print-resource-dir` devuelve `A:\al82dev-clang\lib\clang\23`; existe su `include/stddef.h`. Todavía no se ha probado el descubrimiento de recursos mediante una sesión embebida. |
| Windows | `A:/vc2026-runtime/SYSROOT.md` describe toolset 14.52.36815 y SDK 10.0.28000.0; las rutas efectivamente utilizadas por la construcción nueva aún deben verificarse. |
| Enlazadores disponibles | Existen `A:/nsc-devel/build-clang/bin/nslink.exe` y `A:/al82dev-clang/bin/lld-link.exe`; aún no se ha establecido cuál utilizará cada paso de la construcción nueva. |
| Caché anterior de NSC | `A:/nsc-devel/build-clang/CMakeCache.txt` contiene `C:/clang-llvm/bin/clang-cl.exe` y `LLVM_DIR=C:/clang-llvm/lib/cmake/llvm`. No se reutilizará para la construcción nueva en A:. |
| Opciones de LLVM | `A:/al82dev-clang/build-full/CMakeCache.txt` contiene `LLVM_ENABLE_EH=OFF`, `LLVM_ENABLE_RTTI=OFF`, `LLVM_ENABLE_LIBXML2=FORCE_ON`, `LLVM_ENABLE_ZLIB=FORCE_ON` y `LLVM_ENABLE_ZSTD=FORCE_ON`. Esto describe la construcción anfitriona de LLVM, no las excepciones del código C++ de usuario. |

## Tanda 01: entorno de trabajo verificado el 2026-11-05

Esta observación actual complementa la línea base histórica anterior; no la reemplaza ni convierte los cambios
actuales sin commit en parte de la revisión congelada. `git rev-parse HEAD` sigue informando
`152654e5a0e2d84d4c2f245ea7590488f0497430` para NSC en `development-cpp-clang` y
`ea7d852a70e8bdfaf601d6626a760f9771b2c4b4` para LLVM en `nativesharp-daikon-interop`. La observación inicial
de cambios preexistentes es la tabla del 2026-11-03; actualmente NSC tiene numerosos cambios locales y no
deben confundirse con esa observación inicial ni descartarse.

| Contrato | Evidencia consultada |
|---|---|
| Headers del toolset | `A:/vc2026-runtime/toolsets/14.52.36815/include`; `yvals_core.h` existe. |
| Headers del SDK | `A:/vc2026-runtime/windows-sdk/10.0.28000.0/include/{ucrt,shared,um,winrt}`; se verificaron `ucrt/corecrt.h` y `um/Windows.h`. |
| Bibliotecas Windows x64 | `A:/vc2026-runtime/toolsets/14.52.36815/lib/x64`, `A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib/{ucrt,um}/x64`; DIA bajo `toolsets/14.52.36815/dia/{include,lib/x64}`. Las listas utilizadas al configurar están en [configurar-nsc-tanda1.cmd](configurar-nsc-tanda1.cmd). Algunos fixtures de lifetime enlazan explícitamente el runtime histórico `A:/vc2026-runtime/lib/x64/release`; no se identifica silenciosamente como el toolset versionado. |
| Construcción de NSC | `A:/nsc-interop-build/tanda1-ON/CMakeCache.txt`: `Release`, `NSC_ENABLE_CPP_INTEROP=ON`, `NSC_HOST_LINKAGE=STATIC`, compilador `A:/al82dev-clang/bin/clang-cl.exe`, linker `A:/al82dev-clang/bin/lld-link.exe`; `build.ninja` registra C++20, `-MT`, `/O2`, `/W4` y `/permissive-`. El comando de Ninja para enlazar `nsc.exe` usa `lld-link` x64 y `/Brepro`. La configuración sin integración está en `A:/nsc-interop-build/tanda1-OFF`. |
| Linker para programas de prueba | El caché configura `A:/nsc-devel/build-clang/bin/nslink.exe`; los scripts de proyecto consultados fijan por proceso `NSC_LINKER=C:/native-sharp/bin/nslink.exe`. Son configuraciones distintas: el enlace de NSC usa `lld-link`; los fixtures que lo indican usan NSLink. El ejecutable de C: se consulta como herramienta existente y no es destino de despliegue. |
| Directorios | Construcciones `A:/nsc-interop-build/tanda1-ON` y `tanda1-OFF`; temporales generales `A:/nsc-interop-build/temp`; fixtures y resultados generados `A:/nsc-interop-build/cpp-fixtures`; fixtures fuente y scripts bajo `A:/llvm-project-al82dev/nativesharp-interop`; esta línea base y [TRACKING.md](TRACKING.md) están juntos. Los scripts de fixture pueden usar subcarpetas `temp` propias. |
| Instrucciones | Leídos los lineamientos `A:/LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md` y la instrucción de usuario `AGENTS.md` de este chat. La búsqueda `rg --files --hidden --no-ignore -g AGENTS.md -g !.git` en `A:/nsc-devel` y en `A:/llvm-project-al82dev/nativesharp-interop` no encontró archivos adicionales. |

Las fuentes de esta observación son los archivos y comandos indicados, no una reconstrucción nueva. Las pruebas y
resultados de implementación posteriores se registran por separado en [SCOPE-LOCK-EJECUCION.md](SCOPE-LOCK-EJECUCION.md).

## Primera verificación de configuración

- `configurar-nsc-tanda1.cmd OFF`: configuración y generación correctas en `A:/nsc-interop-build/tanda1-OFF`; CMake advirtió que `Clang_DIR` no se utilizó. El script establece explícitamente compilador, LLVM, enlazador y temporales en A:.
- `configurar-nsc-tanda1.cmd ON`: configuración y generación correctas en `A:/nsc-interop-build/tanda1-ON`. El caché registra `NSC_ENABLE_CPP_INTEROP=ON`, `Clang_DIR` y `LLVM_DIR` bajo `A:/al82dev-clang`, y `NSC_NSLINK_EXECUTABLE` bajo `A:/nsc-devel/build-clang/bin`.
- Prueba negativa: `cmake -S A:/nsc-devel -B A:/nsc-interop-build/tanda1-ON -DClang_DIR=C:/clang-llvm/lib/cmake/clang` terminó con código 1 e identificó `LLVM: A:/al82dev-clang; Clang: C:/clang-llvm`. Se ejecutó después el script con `ON`, que restableció y regeneró correctamente la configuración de A:.
- CMake mostró que el valor predeterminado de `NSC_TEST_COMPILER` apunta a C:. Las pruebas no estaban activadas y no se ejecutó CTest; será obligatorio sobreescribir esa ruta antes de habilitarlas.
- CMake informó que las variables de detección opcional de zlib, zstd, libxml2 y DIA proporcionadas en el script no fueron utilizadas durante la configuración. No se infiere de ello que el enlace final esté resuelto.

No se ha construido NSC ni ejecutado una prueba de interoperabilidad en esta tanda.

## Interfaces y validación iniciada

- Se agregaron cuatro encabezados de sesión independientes de Clang en `A:/nsc-devel/include/nativesharp/Interop/`. El contrato tiene opciones y diagnósticos poseídos, cancelación compartida poseída, resultado de apertura como variante exclusiva y una interfaz abstracta de sesión.
- `verificar-interfaces-tanda1.cmd` terminó con código 0 al compilar `prueba-interfaces-tanda1.cpp` con Clang 23.1.0, C++20, `/W4 /WX` y solo los headers de NSC y del sysroot; no enlaza Clang. Es una prueba sintáctica y de tipos, no una prueba de sesión real.
- La combinación `NSC_ENABLE_CPP_INTEROP=ON` y `NSC_HOST_LINKAGE=DYNAMIC` fue rechazada con código 1 y diagnóstico explícito; se restauró después la configuración `STATIC` en A:.
- Un paquete ficticio aislado en `A:/nsc-interop-build/fixture-clang22` anuncia Clang 22.1.0. La configuración `ON` lo rechazó con código 1 y mostró la versión requerida y la encontrada; la configuración 23.1.0 y las opciones de búsqueda originales se restauraron después.
- `construir-frontend-tanda1.cmd` terminó con código 0 para `nsc_frontend` en modo `OFF`. También se construyó `nsc_precompiled`, requerido por el enlazado del cliente de prueba.
- `verificar-inyeccion-tanda1.cmd` terminó con código 0. La prueba de sesión simulada comprueba ausencia de sesión por defecto, transferencia de propiedad, rechazo tras un primer intento fallido, destrucción, rechazo de una sesión cerrada y cancelación compartida entre copias. La prueba enlaza el frontend sin bibliotecas Clang; no abre una sesión real.
- `A:/al82dev-clang/bin/llvm-readobj.exe --coff-imports A:/nsc-interop-build/prueba-inyeccion.exe` informó `KERNEL32.dll` como importación directa del ejecutable de prueba. Esto no prueba las dependencias del futuro `nsc.exe` ni del paquete completo.

## Primera sesión Clang real

- Se agregó `nsc_cpp_interop`, activado solo con `NSC_ENABLE_CPP_INTEROP`, y una factoría que conserva un `clang::ASTUnit` y sus diagnósticos sin exponer tipos Clang en la interfaz pública. La sesión utiliza `clang::tooling::buildASTFromCodeWithArgs`; no ejecuta `clang.exe` para analizar la unidad.
- `configurar-nsc-tanda1.cmd ON ON` y `construir-frontend-tanda1.cmd nsc_cpp_session_tests ON` configuraron, construyeron y enlazaron la primera versión de la prueba aislada con código 0.
- `A:/nsc-interop-build/tanda1-ON/bin/nsc_cpp_session_tests.exe A:/al82dev-clang/lib/clang/23 A:/nsc-interop-build A:/al82dev-clang/bin/clang.exe` terminó con código 0. Abrió unidades C y C++, consultó el AST tras analizar, rechazó sintaxis inválida con ubicación, recursos ausentes y cancelación previa, comprobó cierre repetido y mantuvo independiente la segunda sesión.
- La prueba se amplió a una entrada física que usa un header con ruta de inclusión relativa y opciones de definición/eliminación de macros ordenadas; reconstrucción y ejecución también terminaron con código 0.
- La prueba se reconstruyó con `nsc_frontend` y `nsc_precompiled` y la ejecución anterior terminó nuevamente con código 0. Transfirió la sesión C++ real a `ProjectCompilation` y comprobó que su AST seguía accesible. No se compiló un programa NativeSharp que consuma esa clase.
- El frontend sin Clang se reconstruyó tras el cambio de interfaz y `verificar-inyeccion-tanda1.cmd` volvió a terminar con código 0.
- La inspección `llvm-readobj --coff-imports` del ejecutable de prueba combinado mostró `KERNEL32.dll`, `ADVAPI32.dll`, `ntdll.dll`, `VERSION.dll`, `OLEAUT32.dll`, `SHELL32.dll` y `ole32.dll` como importaciones directas. Es evidencia de ese ejecutable, no de un paquete distribuible ni del compilador final.
- La implementación usa un sistema de archivos físico por sesión; `llvm/include/llvm/Support/VirtualFileSystem.h:379-384` documenta que su directorio de trabajo es independiente del proceso. Esto no sustituye una prueba de concurrencia.

## Proyección inicial de funciones

- `CppFunctionDeclaration.h` y `CppInteropSession::freeFunctionsSnapshot()` ofrecen copias poseídas de funciones libres concretas vistas en la unidad, con nombres, tipos textuales y canónicos, parámetros, variadicidad, ubicación y símbolo calculado por Clang cuando la declaración tiene enlace externo. No constituye una clasificación ABI ni una importación en la semántica NativeSharp; métodos y templates dependientes permanecen pendientes.
- La prueba `nsc_cpp_session_tests.exe`, ejecutada con las tres rutas indicadas arriba, terminó con código 0. Comprueba función C variádica, deduplicación de redeclaraciones, `extern "C"`, sobrecargas en namespace C++, ausencia de símbolo público para función interna y conservación de copias después de cerrar la sesión.
- El fixture `A:/nsc-interop-build/cpp-fixtures/symbols.cpp` se compiló con `A:/al82dev-clang/bin/clang.exe -target x86_64-pc-windows-msvc -std=c++20 -c`. `llvm-nm --defined-only` mostró `c_api`, `?blend@tools@@YAHH@Z` y `?blend@tools@@YANN@Z`; la prueba comprueba exactamente esos nombres para sus declaraciones correspondientes. La presencia de esos símbolos en una biblioteca externa distinta aún debe verificarse por separado.
- Se añadió `CppType.h`: la proyección de parámetros y retornos conserva categorías, calificadores, tamaño y alineación de objeto cuando son conocidos, además del elemento de punteros, referencias y arreglos. La ejecución de `nsc_cpp_session_tests.exe` terminó con código 0 y comprobó `long` de 32 bits, `long long` de 64, puntero de 64 a `const int` y ausencia de tamaño inventado para un struct incompleto. No prueba la clasificación ABI de argumentos ni el tratamiento de todos los tipos C++.
- Tras el cambio de tipos, se reconstruyeron los targets de prueba `ON` y frontend `OFF`; sus ejecuciones de prueba terminaron con código 0.

## Proyección inicial de records y métodos

- `CppRecordDeclaration.h` y `CppInteropSession::recordsSnapshot()` copian records nombrados no dependientes, campos, tipos, accesibilidad, layout y métodos explícitos. Las copias sobreviven al cierre de la sesión. Los records anónimos, tipos dependientes, herencia proyectada y operaciones de construcción y llamadas desde NativeSharp siguen pendientes.
- La prueba `nsc_cpp_session_tests.exe` terminó con código 0 para una estructura C con padding, bitfields, record incompleto, declaración adelantada seguida de definición y una clase C++ con campo privado, público, miembro estático, constructor, destructor y métodos estáticos y virtuales. Comprueba tamaños y offsets suministrados por Clang y la ausencia de tamaño para el record incompleto.
- Se compilaron con Clang 23.1.0 y `-target x86_64-pc-windows-msvc -std=c++20 -c` los fixtures [de clase](fixture-record-symbols.cpp) y [de base virtual](fixture-virtual-base-symbols.cpp). `llvm-nm` mostró para `MyClass` los símbolos `??0MyClass@@QEAA@H@Z`, `??1MyClass@@QEAA@XZ`, `?B@MyClass@@SAHN@Z`, `?C@MyClass@@UEAAHXZ` y `?shared@MyClass@@2HA`; para la destrucción explícita de `Derived` con base virtual mostró `??_DDerived@@QEAAXXZ`. La prueba compara exactamente esos nombres con la proyección.
- La primera comparación detectó que manglar siempre `Dtor_Complete` producía `??_D` para una clase sin bases virtuales, mientras el objeto usaba `??1`. Se corrigió la selección siguiendo `clang/lib/CodeGen/MicrosoftCXXABI.cpp:1703-1704`: en el ABI Microsoft, un destructor completo sin bases virtuales utiliza la variante base. Ambas ramas se contrastaron mediante los objetos citados; esto no valida aún su invocación ni todos los casos ABI.
- Se reconstruyeron los targets con `ON` y `OFF`; la prueba de sesión real y `verificar-inyeccion-tanda1.cmd` terminaron con código 0. Este resultado no demuestra interoperabilidad de extremo a extremo.

## Referencias C/C++ de proyecto: avance parcial del 2026-11-03

- Se agregó `CppLibraryReference` como entrada independiente de `LibraryReference`: registra header, lenguaje, estándar, biblioteca y DLL opcionales, directorios de inclusión y macros ordenadas. El contrato previo de metadata de NativeSharp permanece separado. Puede declararse una biblioteca formada solo por headers.
- `ProjectLoader` valida entradas y resuelve rutas; `Project` conserva las referencias y las reconstrucciones de `ProjectGraph` y `ProjectGraphLoader` las propagan. El manifiesto registra el header principal y los archivos binarios declarados.
- La compilación y el cálculo de identidad de caché rechazan expresamente estas referencias mientras no estén conectadas la importación semántica, el enlace y el seguimiento de headers transitivos. El rechazo es una protección temporal, no soporte de interoperabilidad.
- `construir-frontend-tanda1.cmd nsc_cpp_session_tests ON` terminó con código 0. `A:/nsc-interop-build/tanda1-ON/bin/nsc_cpp_session_tests.exe A:/al82dev-clang/lib/clang/23 A:/nsc-interop-build A:/al82dev-clang/bin/clang.exe` terminó con código 0: verifica carga C y C++, macros ordenadas, referencia sin biblioteca, preservación al reconstruir el grafo, entradas del manifiesto, rechazos de caché y compilación, y diagnósticos específicos para cuatro configuraciones inválidas. Los fixtures están en `A:/nsc-interop-build/cpp-fixtures/`.
- La reconstrucción `construir-frontend-tanda1.cmd nsc_precompiled OFF` terminó con código 0; `verificar-inyeccion-tanda1.cmd` también terminó con código 0. Esto comprueba construcción y la prueba dirigida de inyección, no todos los programas NativeSharp existentes. No se ha ejecutado un programa NativeSharp que llame a C o C++ mediante estas referencias.

## Sesiones a partir de referencias: avance parcial del 2026-11-03

- `CppProjectSessions` transforma cada referencia de un proyecto en opciones propias de Clang y abre las sesiones mediante una factory proporcionada por el llamador. El entorno común se proporciona explícitamente y cada sesión conserva su lenguaje, estándar, header, inclusiones y macros. El resultado conserva el orden de las referencias; ante un fallo devuelve su índice y el diagnóstico, sin entregar un conjunto parcial.
- `construir-frontend-tanda1.cmd nsc_cpp_session_tests ON` y la ejecución de `nsc_cpp_session_tests.exe` con las rutas de Clang y fixtures descritas arriba terminaron con código 0. La prueba abre las dos referencias reales del fixture, verifica sus opciones diferenciadas y comprueba un fallo en la segunda referencia y la propagación de cancelación antes de abrir la primera.
- El entorno compartido admite rutas explícitas de headers de sistema: el fixture `cpp-fixtures/system-probe.h` incluye un header accesible mediante esa configuración y la prueba encuentra la función declarada allí. Aún falta configurar las rutas reales del SDK/STL para bibliotecas concretas y comprobar su integración completa.
- La misma ejecución comprobó dos sesiones simultáneamente vivas con macros distintas, el cierre de una sin invalidar la otra, una advertencia con ubicación e identificador que permanece en una copia después de cerrar, y un error con una nota asociada y ambas ubicaciones.
- Este puente se invoca desde la prueba, aún no desde el driver ni el análisis semántico. La compilación de proyectos con referencias C/C++ continúa rechazada deliberadamente, así como su identidad de caché; no se ha comprobado una llamada NativeSharp a una biblioteca externa.

## Propiedad de sesiones por proyecto: avance parcial del 2026-11-03

- Una sobrecarga de `openCppProjectSessions` recibe un `Project` y conserva su archivo como propietario del conjunto. `ProjectCompilation` puede tomar varios conjuntos antes de comenzar y mantiene las sesiones vivas durante la vida de sus modelos. Rechaza propietarios ausentes o duplicados y sesiones nulas o no listas; rechaza cambios después de comenzar. La inyección individual anterior permanece disponible.
- `construir-frontend-tanda1.cmd nsc_cpp_session_tests ON` y `nsc_cpp_session_tests.exe` con las rutas indicadas anteriormente terminaron con código 0. La prueba transfirió sesiones Clang reales de dos proyectos distintos, comprobó su identidad y sus opciones separadas, y verificó los rechazos de propietario duplicado, ausente, sesión nula y reemplazo después de comenzar sin modificar el conjunto anterior.
- `construir-frontend-tanda1.cmd nsc_precompiled OFF` y `verificar-inyeccion-tanda1.cmd` terminaron con código 0. El frontend puede enlazar sin la implementación Clang en la configuración examinada.
- El driver todavía no crea ni inyecta estos conjuntos; la asociación al grafo y la importación en la semántica están pendientes. La protección de compilación y caché continúa activa, por lo que esta prueba no demuestra llamadas desde NativeSharp.

## Preparación de sesiones para el grafo: avance parcial del 2026-11-03

- `openCppGraphSessions` recorre los proyectos del grafo en orden, abre un conjunto por proyecto con referencias C/C++ y omite los que no las tienen. El resultado se puede mover directamente a `ProjectCompilation`; si una apertura falla, informa el proyecto, el índice de referencia y el fallo sin devolver conjuntos parciales.
- `construir-frontend-tanda1.cmd nsc_cpp_session_tests ON` y la ejecución de `nsc_cpp_session_tests.exe` con las rutas de Clang y fixtures indicadas antes terminaron con código 0. El grafo de prueba contiene tres proyectos: dos con una y dos referencias respectivamente y uno sin referencias. Se comprobó el orden de los conjuntos, su transferencia al frontend, un fallo controlado en la segunda referencia del segundo proyecto con referencias y cancelación en el primero.
- El target `nsc_cpp_interop` declara su dependencia de `nsc_compilation`, requerida por el acceso al grafo. La reconstrucción ON y la prueba anterior se repitieron con resultado 0 después de declararla. `construir-frontend-tanda1.cmd nsc_precompiled OFF` y `verificar-inyeccion-tanda1.cmd` terminaron con código 0 tras el cambio de interfaz.
- En esta prueba inicial el recorrido se llamó explícitamente desde la prueba; la conexión posterior con la preparación del driver se documenta en la sección siguiente. Las declaraciones aún no se incorporan al análisis semántico y no se ha demostrado una llamada NativeSharp a C/C++; la protección de compilación y de caché sigue vigente.

## Composición explícita en la preparación del driver: avance parcial del 2026-11-03

- Con la integración de compilación activada, `configurePrecompiledModules` consulta una configuración explícita del `DriverExecutionScope` y, si está presente, abre las sesiones del grafo y las entrega al frontend antes de preparar metadata o consultar la caché. El contexto posee las rutas y recibe prestada la factory durante la ejecución síncrona. Sin entorno explícito no abre sesiones; con entorno pero sin factory falla claramente.
- `construir-frontend-tanda1.cmd nsc_cpp_session_tests ON` y la ejecución de `nsc_cpp_session_tests.exe` con las rutas de Clang indicadas antes terminaron con código 0. La prueba invocó `configurePrecompiledModules` con un contexto configurado y dos proyectos con referencias, verificó ambas sesiones en el frontend, comprobó el camino sin configuración y comprobó un error de recursos Clang con proyecto e índice de referencia.
- El punto de configuración es interno y se estableció directamente en la prueba; todavía no existe una opción pública ni descubrimiento de rutas para activarlo desde una invocación ordinaria de `nsc`. La semántica, las llamadas y el enlace de las bibliotecas siguen pendientes y las protecciones de compilación y caché permanecen vigentes.
- `construir-frontend-tanda1.cmd nsc ON` produjo `A:/nsc-interop-build/tanda1-ON/bin/nsc.exe` y la comprobación de runtime estático del propio build pasó. Con ese ejecutable, `--check-project-syntax` sobre `cpp-fixtures/plain/Plain.nsproj` terminó con código 0; sobre `cpp-fixtures/project/CppProbe.nsproj` terminó con código 1 y el rechazo explícito de referencias todavía no conectadas. Estas pruebas cubren sintaxis y diagnóstico, no ejecución de llamadas externas.
- `construir-frontend-tanda1.cmd nsc_driver OFF` y `construir-frontend-tanda1.cmd nsc OFF` terminaron con código 0, incluida la comprobación de runtime estático. El `nsc.exe` OFF en A: también comprobó la sintaxis de `Plain.nsproj` con código 0. La prueba ON se repitió con resultado 0 incluyendo la ausencia de factory con entorno explícito.

## Configuración explícita desde consola: avance parcial del 2026-11-03

- El driver acepta `--cpp-clang-root=<ruta-absoluta>` y `--cpp-system-include=<ruta-absoluta>` repetible en las acciones de compilador que pasan por `CompilerOptionParser`. El root proporciona `lib/clang/23` y la ruta de referencia `bin/clang.exe`; Clang se utiliza como biblioteca y no se ejecuta ese binario. El triple se obtiene de `TargetInfo::WindowsX64()`. Esta configuración es inicial, no constituye detección automática ni demuestra interoperabilidad semántica.
- Se construyeron `nsc` ON y OFF en `A:/nsc-interop-build`, ambos con código 0 y comprobación de runtime estático aprobada. La prueba `nsc_cpp_session_tests.exe` con las rutas documentadas anteriormente terminó con código 0.
- En `--check-project-syntax`, un root inexistente produjo un error de recursos Clang con el proyecto y la referencia; un include sin root, una ruta relativa, un root duplicado y usar root en el build OFF devolvieron error de opciones. Un proyecto ordinario con root e include válidos pasó su comprobación sintáctica.
- En el fixture `cpp-fixtures/cli-system/CliSystem.nsproj`, sin include de sistema Clang informó `session_shared.h` ausente; al agregar `--cpp-system-include=A:/nsc-interop-build/cpp-fixtures/system`, Clang pudo analizarlo y la compilación llegó al rechazo temporal de referencias no conectadas. Esto comprueba la lectura real del header, no una llamada NativeSharp.
- En la prueba inicial `nsc build` utilizaba otro analizador y rechazaba `--cpp-clang-root`; su ajuste posterior se registra abajo. Su cálculo de identidad continúa rechazando referencias C/C++ antes de invocar el driver de generación, hasta implementar la semántica y el seguimiento correcto de dependencias transitivas.

## Opciones en build y run: avance parcial del 2026-11-04

- `ProjectCommands.cpp` reconoce las dos opciones explícitas de C/C++ para `build` y `run`, valida rutas absolutas, existencia de los includes, root único y la dependencia del include respecto del root. Conserva la secuencia recibida y la agrega a los argumentos del compilador anidado.
- Las construcciones `construir-frontend-tanda1.cmd nsc ON` y `nsc OFF` terminaron con código 0 y sus comprobaciones de runtime estático pasaron. `nsc build` OFF sobre `Plain.nsproj` con root explícito terminó con código 2 y el diagnóstico de compilador construido sin interoperabilidad: esto demuestra que el root llegó al compilador anidado. Las pruebas de `build` y `run` también rechazaron root relativo, root duplicado e include sin root.
- `nsc build` ON de `CppProbe.nsproj` con root explícito aceptó la opción y terminó con código 1 por la protección de referencias C/C++ en la identidad del manifiesto. No se ha habilitado su caché ni se ha ejecutado una llamada a la biblioteca.

## Proyección inicial de variables globales: avance parcial del 2026-11-04

- `CppInteropSession::globalsSnapshot()` devuelve copias poseídas de variables de archivo y namespace: nombres, tipo, ubicación, enlace externo, almacenamiento por hilo y símbolo calculado por Clang cuando existe. Se deduplican redeclaraciones; el recorrido no baja a funciones ni a clases. El cierre impide nuevas consultas y las copias anteriores sobreviven.
- El fixture [fixture-global-symbols.cpp](fixture-global-symbols.cpp) se compiló con Clang 23.1.0 para `x86_64-pc-windows-msvc`. `llvm-nm --defined-only` mostró `c_global`, `?cpp_global@tools@@3HA` y `?tls_value@tools@@3HA`. La prueba de sesión contrasta esos nombres y comprueba tamaño, `const`, enlace interno, TLS y deduplicación.
- La primera compilación detectó que `VarDecl` no expone `isDependentContext`; se corrigió usando su `DeclContext`. La reconstrucción ON y `nsc_cpp_session_tests.exe` terminaron con código 0. `construir-frontend-tanda1.cmd nsc_precompiled OFF` y `verificar-inyeccion-tanda1.cmd` también terminaron con código 0.
- Este resultado solo proyecta información del AST. Todavía no importa las variables en el modelo semántico de NSC ni permite leerlas o escribirlas desde NativeSharp; T08-004 y T10-004 permanecen pendientes.

## Mapeo inicial de representaciones escalares: avance parcial del 2026-11-04

- `mapCppScalarType` conserva la escritura canónica del tipo de Clang junto con una clave de representación `builtin:` de NSC. Para los enteros admite anchos de 8, 16, 32 y 64 bits con signo o sin él; admite `void`, `bool`, `float` de 32 y `double` de 64 bits. Rechaza tipos no escalares, anchos no contemplados, tipos sin identidad y calificadores que todavía no se preservan en esta conversión. Un `long double` de 64 bits no se acepta solo por coincidir en tamaño con `double`.
- Las claves se contrastaron con `src/Frontend/MemberModel/TypeIdentity.cpp` y `src/LLVM/Lowering/Support/LlvmTypeMapping.cpp`. `construir-frontend-tanda1.cmd nsc_cpp_session_tests ON` y la ejecución de `nsc_cpp_session_tests.exe` con las rutas documentadas anteriormente terminaron con código 0. La prueba utilizó declaraciones reales de Clang para `void`, `bool`, enteros con y sin signo, `float` y `double`, y comprobó que `int` y `long` de C++ conservan identidades distintas aunque ambos ocupen 32 bits en Windows x64.
- Este mapeo expresa una representación de valor, no una clasificación de parámetros/retornos del ABI ni una resolución de sobrecarga. Todavía no se integra en el modelo semántico ni habilita llamadas; T08-006 a T08-008 siguen pendientes de su conexión y validación de extremo a extremo.

## Calificadores escalares: ampliación parcial del 2026-11-04

- El mapeo inicial descrito arriba rechazaba `const` y `volatile`; ahora los conserva como atributos separados de la clave NSC y mantiene tanto la identidad canónica original como su forma sin calificadores para reconocer `float` y `double`. No se supone que `volatile` sea respetado ya por la generación de accesos.
- La prueba ON utilizó declaraciones C reales `const int`, `volatile int` y `const volatile float`, comprobó los atributos y claves devueltos, y terminó con código 0. También se reconstruyó `nsc_precompiled OFF` y pasó `verificar-inyeccion-tanda1.cmd`, ambos con código 0.
- El transporte de esos atributos no completa T08-015: siguen pendientes la incorporación semántica, las reglas de acceso y la emisión correcta de lecturas y escrituras.

## Frontera de funciones importadas: observación parcial del 2026-11-04

- En `src/Frontend/ProjectCompilation.cpp`, `compileProjectMembers` construye `ProjectNamespaceFunctionModel` después de los modelos de nombres y miembros. En `src/Frontend/ProjectNamespaceFunctionModel.cpp`, `build` recoge las declaraciones de `ProjectNameLookup`, construye índices y después enlaza las referencias de las unidades; una declaración importada debe estar disponible antes de ese enlace, no agregarse posteriormente.
- `collectNamespaceFunction` requiere `declaration.syntax` y `initializeNamespaceFunction` obtiene parámetros y atributos de nodos del AST de NSC. `mergeNamespaceFunction` indexa por proyecto, namespace y nombre; su identidad de fusión usa la firma de NSC. La integración debe conservar también la identidad y las reglas de sobrecarga de Clang: dos tipos C++ con el mismo tamaño y clave de representación no se deben fusionar automáticamente.
- `src/Frontend/HIR/HirCallLowering.cpp`, en `lowerNamespaceFunctionCall`, utiliza el ordinal seleccionado y `NamespaceFunctionSymbol::mangledName`. Hay que transportar hasta la generación el símbolo de Clang y la clasificación ABI; insertar un nombre en el índice por sí solo no demuestra una llamada válida. Falta revisar y modificar todas las etapas de clasificación, emisión y enlace correspondientes antes de retirar el rechazo temporal. Esta observación no constituye la auditoría completa de T08-001 ni de T09-001/002.

## Índice semántico inicial de funciones: avance parcial del 2026-11-04

- `ProjectNamespaceFunctionModel::setCppImportedFunctions` acepta símbolos ya proyectados antes de construir el modelo, comprueba campos obligatorios e identidades duplicadas y los inserta en los índices de funciones antes de enlazar referencias. El modelo valida contra `ProjectNameLookup::modules()` que el ordinal y nombre del propietario coincidan. `NamespaceFunctionSymbol::cppCanonicalIdentity` conserva separada la identidad suministrada por Clang.
- La prueba `nsc_cpp_session_tests.exe` construyó los modelos de un proyecto NativeSharp cuyo `Main` contiene `c_add(1, 2)`. La prueba convirtió manualmente la declaración C obtenida de una sesión Clang en un símbolo NSC, lo suministró al modelo y comprobó que la llamada encontrara ese candidato y que `Main` siguiera indexado. También comprobó rechazo de símbolos incompletos, identidades duplicadas, propietario inexistente y cambios tras construir el modelo.
- Otra prueba convirtió manualmente dos declaraciones reales de `tools::blend` de Clang y verificó que el índice conserve dos símbolos e identidades distintas. Esto comprueba almacenamiento y búsqueda, no selección de sobrecarga por Clang.
- `construir-frontend-tanda1.cmd nsc_cpp_session_tests ON`, la ejecución de esa prueba, `construir-frontend-tanda1.cmd nsc_precompiled OFF` y `verificar-inyeccion-tanda1.cmd` terminaron con código 0. Las conversiones de la prueba aún no están automatizadas ni conectadas a `ProjectCompilation`; el rechazo de compilación y caché sigue vigente. Faltan la incorporación de namespaces al modelo de nombres, el análisis de llamadas, ABI, emisión y enlace. No se ha ejecutado la función desde NativeSharp.
