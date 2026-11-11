# T09-001 y T09-002: auditoria inicial de llamadas externas

Base: 7d04d52180a6015d9fd7da96269bc175b6c61380. Revision estatica parcial; no marca casillas ni acredita pruebas ejecutadas. Destino observado: x86_64-pc-windows-msvc.

1. ClangSession.cpp:382 y 415-423 captura la convencion y el simbolo de una funcion. CppSemanticFunction.cpp:300-309 valida datos y rechaza variadicas no cdecl; 337-346 transporta simbolo, convencion y variadicidad al modelo.

2. HirBuildCoordinator.cpp:1711-1739 y 1794-1820 crea la declaracion HIR, admite cdecl y vectorcall y rechaza otras convenciones. Las DLL con aislamiento TLS pueden usar una identidad interna distinta y conservar el simbolo externo en cAbiName. HirCallFrameLowering.cpp:557-562 registra la funcion elegida y su nombre en el nodo de llamada.

3. MirNodeMachine.cpp:1898-1926 resuelve la funcion por ordinal y genera DirectStaticCall con el nombre de destino. MirFunctionLowerer.cpp:207-257 copia metadatos, convencion vectorcall, simbolo, tipo y parametros del HIR al MIR.

4. MirLayoutImporter.cpp:14-19 y 115-120 clasifica agregados por tamano y da tratamiento indirecto a miembro flexible; MirFunctionLowerer.cpp:292-316 calcula el retorno, parametros y posicion de retorno oculto. Esta observacion es de la implementacion, no demuestra el ABI completo de C++ ni otros destinos.

5. OopLlvmDeclarations.cpp:12-69 construye la firma, incluido retorno indirecto y variadicidad; 100-139 declara el simbolo y marca vectorcall. OopLlvmCalls.cpp:820-935 obtiene el destino, ordena argumentos, prepara copia para agregados y emite la llamada; vectorcall usa emitExternCDelegateCall. OopLlvmClosuresDelegatesEvents.cpp:1038-1122 genera llamadas por puntero y marca la convencion de llamada.

6. CallAnalyzer.cpp:489-505 y 4097-4105 solicita promociones para llamadas directas e indirectas. ConversionAndPassKindChecks.cpp:139-208 convierte float a double, bool y enteros estrechos a int, maneja enums y registros C completos y rechaza tipos no representados. La correccion del conjunto requiere controles, no se infiere solo del codigo.

Evidencia anterior: TRACKING-TANDA-08-DETALLADO.md:160-175 y registros t08-funciones-declaraciones.md y t08-funciones-visibilidad-enlace.md informan controles previos O0-O3 para escalares, punteros, variadicas, estatica, DLL y simbolo ausente. Alcance de esas muestras limitado a lo descrito; no fueron reproducidas aqui.

Preparacion de validacion: Clang del PATH informa 23.1.0 y target x86_64-pc-windows-msvc. build-tanda09 se configuro parcialmente en A:. Primer intento fallo por TEMP con espacio accidental; corregido. Segundo y tercer intento fallaron porque CMake exige NSC_NSLINK_EXECUTABLE existente; nslink.exe si se resuelve por PATH. El proceso start no transmitio el codigo de cmake en las consultas; se conserva el texto y no se presenta como aprobada la configuracion.

Las pruebas anteriores tienen rutas del equipo de origen y alteran PATH; no se ejecutaron. Siguiente: resolver configuracion de NSLink sin seleccionar ejecutables fuera del procedimiento autorizado, completar auditoria de llamadas y seleccionar controles reproducibles para cada casilla. No se modificaron fuentes ni pruebas.
Contraste adicional: T08-004-04a a 04e informa globales y T08-017-03 callbacks con controles previos. T09-015 tiene un control historico de simbolo ausente; queda por comprobar diagnostico de biblioteca ausente. Estos resultados no fueron reproducidos en la laptop.

T09-007 y T09-008: t08-d09-record-dos-encabezados.md informa paso y retorno directo de struct y union y controles Clang en O0-O3. t08-typedef-record-retorno.md informa retorno indirecto por puntero a funcion de records de 4 y 24 bytes, con encabezados ocultos y cache. La evidencia no se ha reproducido en esta sesion.

T09-011 y T09-012: t08-variadicas-agregados.md describe promociones y paso de records C de 3, 8 y 24 bytes por llamada directa e indirecta en dos DLL, con control Clang. Los argumentos sin tipo y structs NativeSharp se rechazan en los casos descritos. No se amplia este resultado a todos los tipos ni destinos.

T09-015: ProjectLoader.cpp:1246-1260 forma un diagnostico con la ruta de biblioteca ausente. La ruta CppLibraryReference llama a la misma validacion en 1361-1379; el rechazo ocurre durante carga del proyecto, antes del enlace. Debe comprobarse con un caso dirigido y mantener separado el simbolo no resuelto del enlazador.
T09-015: CppClangSessionTests.cpp:2916-2927 comprueba biblioteca C ausente solo mediante la palabra library; ProjectLoaderTests.cpp:273-280 comprueba LibraryReference, no CppLibraryReference, y no exige la ruta. ProjectLoader.cpp:1246-1255 si incluye la ruta. Pendiente control que compruebe el nombre exacto y la ruta para CppLibraryReference, ademas del simbolo ausente. No se han ejecutado esas pruebas.
Infraestructura de pruebas: CMakeLists.txt:884-924 habilita los controles de sesion solo con NSC_CPP_INTEROP_SESSION_TESTS y requiere un directorio propio con cpp-fixtures/project/CppProbe.nsproj. ProjectLoaderTests.cpp:493-505 elimina recursivamente un subdirectorio fijo de TEMP; no ejecutarlo sin TEMP aislado y ruta verificada dentro de A:. Pendiente preparar entradas y aislar salidas antes de ejecutar.

T09-009 y T09-010, ruta estatica: CppSemanticInventory.cpp:923-930 conserva simbolo y calificadores de la global C seleccionada. IdentifierAnalyzer.cpp:1165-1176 impide asignar estados const o de solo lectura. HirNameExpressionLowering.cpp:251-272 genera valor o direccion y MirNodeMachine.cpp:1567-1571 los transforma en carga o direccion de MIR.

MirBuildCoordinator.cpp:651-673 copia identidad, calificacion y datos de DLL y TLS al campo estatico MIR. OopLlvmStaticState.cpp:106-115 declara el almacenamiento externo y establece atributos de DLL o TLS. OopLlvmExpressionLeaves.cpp:227-254 obtiene la direccion y prepara la carga; la ruta de direccion se usa para asignaciones. Esto es evidencia estatica, no prueba de ejecucion ni cobertura general de atomicos, volatile o TLS.
T09-003: CppClangSessionTests.cpp:3278-3282 comprueba captura cdecl y simbolo C; 9879-9883 comprueba vectorcall hasta HIR. El registro t08-d22-vectorcall.md describe controles previos de llamadas directas e indirectas y comparacion con Clang, incluyendo HFA y sret. El HIR rechaza convenciones distintas de cdecl y vectorcall; no se afirma soporte para ellas sin control adicional. Estas pruebas no se ejecutaron en esta sesion.
Preparado T09-015: pruebas-tanda-09-biblioteca-ausente contiene MissingCppLibrary.nsproj, Main.ns y api.h. La referencia biblioteca_ausente_t09.lib se verifico ausente. Al ejecutarlo debe fallar e identificar esa ruta; falta control automatizado y ejecucion con NSC construido desde esta revision. El lector SimpleXmlParser.cpp:216-223 acepta las comillas simples del proyecto.
El caso T09-015 tiene validar.py, que exige fallo, mensaje de biblioteca ausente y ruta completa, y guarda diagnostico y codigo en un directorio temporal de A:. Se verifico la sintaxis del validador y la opcion library-reference-root en CompilerOptionParser.cpp:434-458; no se ejecuto el compilador ni se certifica el caso.
Oraculo T09-007 y T09-008: Clang 23.1.0, target x86_64-pc-windows-msvc y O0 genero A:/temp/t09-abi-oraculo/abi.ll desde abi.c. En estas muestras de alineacion 1, los records de 4 y 8 bytes usan i32 e i64; los de 3 y 24 bytes usan puntero para argumento y retorno indirecto sret, con copia visible. No se ejecuto NSC ni se ha comparado su IR; otros tipos y optimizaciones siguen pendientes.
El mismo oraculo Clang genero abi-o1.ll, abi-o2.ll y abi-o3.ll: las firmas fisicas observadas para 3, 4, 8 y 24 bytes conservan la misma clasificacion. Esto solo es referencia Clang, no validacion de NSC.
Entradas T09-007 y T09-008: pruebas-tanda-09-agregados incluye productor C y consumidores C y NS para records de 3, 4, 8 y 24 bytes, con comprobacion de copia y retorno. Clang 23.1.0 emitio api.ll y Control.ll en A:/temp/t09-abi-oraculo a O0. Esto verifica compilacion C a IR; no se enlazo ni ejecuto el control y la fuente NS permanece sin validar.
Control C T09-007 y T09-008: api.c y Control.c se compilaron con Clang 23.1.0 para Windows x64 y enlazaron con lld-link sin runtime para O0, O1, O2 y O3. Los cuatro ejecutables terminaron con codigo 0; resultados en A:/temp/t09-abi-oraculo/control-oN-resultado.txt. Esto valida solo el control C; falta enlazar y ejecutar el consumidor NativeSharp y comparar ambos.
Control C con biblioteca estatica: los objetos api-oN.obj se archivaron con llvm-lib y Control-oN.obj se enlazo contra cada api-oN.lib mediante lld-link. En O0, O1, O2 y O3 los ejecutables terminaron con codigo 0; resultados control-lib-oN-resultado.txt en A:/temp/t09-abi-oraculo. La comparacion con NativeSharp sigue pendiente.
Control C con DLL: lld-link genero api-dll-oN.dll e import libraries; los cuatro ejecutables terminaron con codigo 0 en O0-O3. llvm-readobj --coff-imports del ejecutable O0 identifica api-dll-o0.dll y pass3, pass4, pass8 y pass24. Las ejecuciones y archivos estan en A:/temp/t09-abi-oraculo; no se ha ejecutado NativeSharp.
Modalidad DLL: api-dll.h activa declaraciones dllimport y AgregadosDll.nsproj describe biblioteca y runtime. El control C compilado con API_DLL_IMPORT produjo referencias __imp_pass3, __imp_pass4, __imp_pass8 y __imp_pass24; enlazado con la DLL termino con codigo 0 en O0-O3. NativeSharp aun no se ha compilado ni ejecutado.
Se preparo pruebas-tanda-09-agregados/validar.py para generar productor, control C y consumidor NativeSharp en directorios temporales aislados; compara los resultados en estatica y DLL para O0-O3 y guarda logs por paso. Se comprobaron sintaxis y ayuda; el validador completo no se ejecuto y no demuestra aun ninguna combinacion NativeSharp.
Validacion del validador: validar.py --solo-c completo ocho casos, estatica y DLL por O0-O3, todos con codigo 0. Evidencia y comandos por paso en A:/temp/nsc-t09-agregados-e_tu9yu1. Los procesos se iniciaron desde el repositorio, con herramientas por nombre y resultados en A:. No se invoco NSC.
Sensibilidad del control C: se altero una funcion a la vez en copias temporales, cambiando incremento 10 por 11. Los records de 3, 4, 8 y 24 bytes devolvieron respectivamente codigos 1, 2, 3 y 4, tanto en estatica como DLL, a O0. Evidencia, fuentes y comandos en A:/temp/nsc-t09-control-negativo-hyz6zkn3. No valida NSC.
T09-004 a 006 y T09-009 a 012: preparado pruebas-tanda-09-llamadas, con sin argumentos, escalares, puntero, global mutable y variadica con short, float, bool y puntero. Primer control C fallo por simbolo _fltused en enlace sin runtime; se definio solo en los controles C. Repeticion validar.py --solo-c paso estatica y DLL en O0-O3: A:/temp/nsc-t09-llamadas-fifw9k3w. El ejecutable DLL O0 importa las cuatro funciones y shared_value. NativeSharp no ejecutado.
Actualizacion del control de llamadas: _fltused se declaro selectany para no imponer una definicion fuerte duplicada; se repitieron las ocho variantes C y pasaron. Evidencia vigente: A:/temp/nsc-t09-llamadas-8iviy0a7. La interoperabilidad NS sigue sin validar.
Sensibilidad del control de llamadas: en O0, cinco mutaciones independientes en copias temporales fueron detectadas tanto en estatica como DLL. Retorno sin argumentos, escalares, puntero, global y variadica produjeron respectivamente codigos 1, 2, 3, 4 y 6; evidencia y comandos en A:/temp/nsc-t09-llamadas-negativo-qpv351u8. No se ejecuto NSC.
T09-011 y T09-012, referencia LLVM Clang O0: tanto estatica como DLL emiten check_variadic como funcion variadica i32 con argumentos i32 -2, double 2.25, i32 1 y ptr. La global es external global en estatica y external dllimport global en DLL. Archivos estatica-varargs-o0.ll y dll-varargs-o0.ll en A:/temp/nsc-t09-llamadas-8iviy0a7. Falta comparar el LLVM y ejecucion de NSC.
T09-015: preparado pruebas-tanda-09-simbolo-ausente, con control C, proyecto NS y validador. El control C se compilo y lld-link rechazo el enlace con codigo 1 mencionando simbolo_ausente_t09 y el objeto de origen. Evidencia A:/temp/nsc-t09-simbolo-ausente-thzx7q_2. Queda pendiente ejecutar el caso con NSC y comprobar su diagnostico.
T09-003: preparado pruebas-tanda-09-vectorcall con H2 y funcion transform_h2. El validador solo C paso estatica y DLL en O0-O3 y comprobo simbolo transform_h2@@16 en los objetos. Clang O0 para DLL genera x86_vectorcallcc y parametro H2 inreg. Evidencia A:/temp/nsc-t09-vectorcall-lwc8jy3y; NativeSharp no ejecutado.
T09-010: preparado pruebas-tanda-09-global-const con intento de escritura y validador. Clang 23.1.0 rechazo el control C con codigo 1, identifico t09_const_global y su declaracion const; evidencia A:/temp/nsc-t09-global-const-qx7owx81. El rechazo NSC no se ha ejecutado.
T09-011 y T09-012, ruta estatica: CallAnalyzer.cpp:489-505 selecciona promociones C; HirCallFrameLowering.cpp:672-682 inserta la conversion y guarda el tipo destino para argumentos variadicos directos, y 950-959 hace lo correspondiente para invocaciones indirectas. OopLlvmFieldsAndConversions.cpp:541-542 emite FPExt para float a double. En Clang la muestra registra i32, double, i32 y ptr; la coincidencia de ejecucion y de LLVM NSC sigue pendiente.
2026-11-10, laptop: compilacion de nsc en build-tanda09 finalizo con codigo 0; log A:/temp/t09-compilacion-nsc-reanudada.log. Comparaciones C y NSC, estatica y DLL O0-O3, pasaron para llamadas y globales A:/temp/nsc-t09-llamadas-z2yaf2g9, agregados A:/temp/nsc-t09-agregados-2xmfb_8c y vectorcall A:/temp/nsc-t09-vectorcall-rulir0cp. Controles NSC de biblioteca ausente A:/temp/nsc-t09-lib-ausente-yvmdc0u4, global const A:/temp/nsc-t09-global-const-y7m7qlsr y simbolo ausente A:/temp/nsc-t09-simbolo-ausente-2dzzu8hf pasaron. El primer intento de simbolo ausente fallo antes del enlace por entorno de runtime no aplicado; se corrigio y se repitio. Casillas y validacion completa siguen pendientes.
Regresion de bloque en laptop: nsc.project-loader paso 1/1; nsc.item331 y nsc.item332 pasaron 2/2; nsc.item344 y nsc.item345 pasaron 2/2; nsc.item333.win64-vararg-abi paso 1/1 con consumidores NS y productores C y C++. Las sondas nsc_cpp_c_identity_tests pasaron con registro A:/temp/t09-identidad-c-20261110/resultado.log. No se ejecutaron los gates completos T08 ni stage48_2.
T09-011 y T09-012, validacion ampliada: llamadas variadicas directas e indirectas con short, float, bool, puntero, sbyte, byte, ushort, long, uint, enum y record C de 12 bytes. Las ocho comparaciones C y NSC de estatica y DLL en O0-O3 pasaron; evidencia vigente A:/temp/nsc-t09-llamadas-9swbf7p0. No equivale a cobertura de todos los tipos o targets.
T09-013 y T09-014: prueba nueva precompilada con plantilla NS que llama funciones C, usa puntero y global y pasa variadicos directos e indirectos incluidos enum y record. Pasaron 16 consumidores: estatica y DLL, con encabezados presentes u ocultos, O0-O3. Evidencia A:/temp/nsc-t09-precompilado-083o45y3. No prueba otras formas de metadata ni compatibilidad general.
T09-013, cache: la prueba directa de encabezados paso aciertos, invalidacion por archivo opcional y transitivo, rechazo de encabezado invalido y desactivacion ante __TIME__; evidencia A:/temp/nsc-t09-cache-1cwuruy5. La prueba de biblioteca estatica cambio bytes de api.lib sin cambiar ruta ni encabezado: not-found e hit para valor 12, luego not-found e hit para valor 14; hashes y logs en A:/temp/nsc-t09-cache-biblioteca-5g5v_ly_. Ambas en O0; no se extrapolan a todas las rutas de cache.

T09-001 y T09-002, contraste de representaciones: se generaron HIR, MIR y LLVM
de la prueba vigente de llamadas, con enlace estatico y DLL. Los seis comandos
terminaron con codigo 0; registros en A:/temp/nsc-t09-dumps-_86en4ho. El MIR
identifica check_variadic como llamada estatica directa y tambien registra la
creacion de un delegado; la declaracion MIR conserva variadicidad. LLVM emite
una llamada variadica con i32, double, puntero y los demas tipos promovidos,
incluida la copia indirecta del record de 12 bytes; la declaracion DLL conserva
dllimport. Estos volcados complementan las ejecuciones, sin probar todos los
tipos, convenciones o destinos.

Revision inicial T09-001 a T09-006: la auditoria cubre la ruta de proyecto
ProjectOop, desde las declaraciones importadas hasta la llamada directa o por
puntero y la emision LLVM, en el destino Windows x64. Las pruebas nuevas
comprueban cdecl, el simbolo decorado y llamada vectorcall directa, y funciones
sin parametros, escalares y con puntero, en estatica y DLL O0–O3. Las pruebas
item344 e item345 complementan vectorcall; no se afirma que otras convenciones
de Clang esten soportadas. Esta revision no certifica todos los tipos C ni ABI
de C++ y no modifica las limitaciones de la tanda anterior.

La suite nsc_cpp_session_tests se construyo, pero no se ejecuto: falta
cpp-fixtures/project/CppProbe.nsproj bajo su raiz de pruebas. Los seis controles
CTest seleccionados y nsc_cpp_c_identity_tests si se ejecutaron como se indico
arriba. Tampoco se ejecuto stage48_2; se conserva intacta y su fixture historico
ausente no se convierte en requisito nuevo.

Avance T09-007 y T09-008: al ampliar la prueba, los records de 1 y 2 bytes y
una union de 4 bytes se sumaron a los records de 3, 4, 8 y 24 bytes. Los ocho
contrastes C y NS en estatica y DLL O0–O3 pasaron; logs en
A:/temp/nsc-t09-agregados-_wrupfy_. Estas muestras verifican paso, resultado y
que el productor no modifique el original; no prueban todas las formas C ni
otros destinos. Un primer intento no llego a invocar Clang por restricciones
del entorno; la repeticion con acceso al repositorio produjo el resultado citado.

Reglas T09-011 observadas en classifyCVariadicPromotion: float pasa a double;
bool y enteros de menos de 32 bits a int; enum usa su representacion entera,
promovida a int si es estrecha; double, punteros y punteros de funcion C
conservan su tipo; records y uniones C completas conservan el tipo y usan el
portador Win64 del agregado. Los tipos que no se reconocen en esa ruta se
rechazan. El argumento fijo mantiene su conversion normal. Las pruebas de la
sesion cubren los tipos concretos registrados arriba, no todas las variaciones
posibles de enum, union o record ni otros destinos.

T09-001, control repetible: `pruebas-tanda-09-llamadas/validar-hir-mir.py`
construye una biblioteca C temporal, solicita HIR y MIR al NSC de desarrollo y
exige la llamada directa, la creacion e invocacion de puntero y las conversiones
variadicas observadas. Paso en A:/temp/nsc-t09-hir-mir-iw6opfec. En HIR la
llamada directa conserva el ordinal de funcion; en MIR se resuelve al nombre
`check_variadic`. La invocacion indirecta permanece como `delegate-invoke` en
ambas representaciones y las promociones quedan explicitas. Este control usa
una biblioteca estatica y una sola firma: no demuestra por si solo las rutas de
DLL, otras convenciones ni todos los tipos. El item permanece abierto mientras
se completa la auditoria de esas rutas.

Ampliacion T09-001: el mismo control se repitio con biblioteca estatica y DLL;
los cuatro volcados HIR/MIR pasaron. Evidencia A:/temp/nsc-t09-hir-mir-fbojst03.
Los volcados verifican la estructura de la llamada y sus conversiones; no
exponen por si solos todos los atributos de enlace o ABI. Otras convenciones y
tipos requieren analisis separado.

Vectorcall, observacion adicional T09-001: se obtuvieron HIR y MIR de la muestra
estatica O0 ya validada en A:/temp/nsc-t09-vectorcall-rulir0cp/estatica-O0/
(t09-hir.log y t09-mir.log). Ambos identifican el simbolo decorado
`transform_h2@@16` precedido por el marcador interno; MIR lo conserva como
destino de la llamada directa. Estos volcados no muestran por si solos el plan
fisico vectorcall completo; ese contraste pertenece tambien a T09-002.
