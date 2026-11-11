# Alcance de ejecucion: tanda 9

Base: development-cpp-clang 7d04d52180a6015d9fd7da96269bc175b6c61380; arbol limpio al inicio. Referencia correcta: cierre T08 ec75d42cd y actualizacion documental 7d04d5218. Rango: ec75d42cd..7d04d5218.

Objetivo: completar T09-001 a T09-016 con auditoria, implementacion, pruebas dirigidas, validacion agrupada, documentacion y commits razonables. El usuario autoriza decisiones de diseno. Direccion: bibliotecas C y C++ consumidas desde NativeSharp.

Naturaleza: auditoria, implementacion funcional, pruebas y documentacion. Preservar conducta y pruebas existentes, contrato unico acumulativo de NSMeta y limites de T08. stage48_2 permanece intacta y no crea requisito historico.

Repositorio autorizado: A:/nsc-devel. Salidas y temporales en A:. LLVM/Clang 23.1.0 y NSLink protegidos salvo necesidad demostrada; no desplegar en C:, reescribir historia ni publicar. Excluidos: Daikon, .NET, vcpkg y consumo de NativeSharp desde C++.

Presupuesto inicial: este documento y tracking y evidencia de nativesharp-interop para auditar T09-001 y T09-002. Fuentes, simbolos y pruebas de produccion protegidos hasta determinar por evidencia el presupuesto concreto de cada caso y registrarlo antes del cambio.

Pruebas existentes intactas. Pruebas dirigidas nuevas y validacion agrupada segun caso; no reconstruir todo por cada cambio. Contrastar con Clang simbolos, ABI, valores, variadicas, globales, diagnosticos y enlace.

Terminacion: todos los elementos T09 con evidencia, pruebas dirigidas y validacion del bloque aprobadas o limitaciones expresamente aceptadas; diff auditado y cambios documentados y agrupados. No inferir compatibilidad general C++.

Inventario inicial: 16 casillas; T09-013 y T09-014 marcadas previamente con limites y requieren contraste; otras 14 pendientes. La marca historica no acredita pruebas ejecutadas en esta sesion.

Auditoria inicial: T09-001 y T09-002. Se observo transporte de simbolo y cdecl y vectorcall en HirBuildCoordinator, MirFunctionLowerer y OopLlvmDeclarations; falta seguir nodos, ABI, llamadas directas e indirectas y evidencia. No se han ejecutado pruebas.
Preparacion de pruebas: construir NSC con interoperabilidad habilitada en build-tanda09, directorio nuevo y separado; temporales en A:/temp. No se cambian fuentes ni pruebas existentes.
T09-015, preparacion: se autoriza crear solo nativesharp-interop/pruebas-tanda-09-biblioteca-ausente con un encabezado, proyecto NS y fuente NS de caso negativo; no modificar pruebas existentes. Comprobar diagnostico y nombre exacto de biblioteca cuando exista una construccion ejecutable autorizada.
T09-015, ampliacion: agregar validar.py en el mismo directorio de prueba para automatizar codigo de salida y diagnostico con nombre y ruta exactos; recibir el compilador por argumento y conservar salidas temporales en A:/temp. No se ejecutara hasta resolver la configuracion y seleccion autorizada del compilador.
T09-007 y T09-008: preparar un oraculo C autocontenido en A:/temp/t09-abi-oraculo con Clang del PATH, solo emision LLVM y sin enlazar; comparar tipos fisicos, copias y retorno de records de 3, 4, 8 y 24 bytes. No modificar el compilador ni ejecutar NSC hasta resolver la configuracion.
T09-007 y T09-008, ampliacion: crear nativesharp-interop/pruebas-tanda-09-agregados con encabezado y productor C, consumidores C y NativeSharp, proyecto NS y oraculo LLVM ya medido. Las entradas seran nuevas y no se modificaran pruebas existentes. Validacion pendiente de construir NSC.
T09-007 y T09-008: se permite compilar y enlazar el control C nuevo en A:/temp/t09-abi-oraculo usando clang y lld-link del PATH; ejecutar solo el programa de control con los cuatro niveles si el enlace es completo. No se modifica NSLink ni se usa NSC.
T09-007, T09-008 y T09-014: se permite enlazar y ejecutar el control C existente contra DLL generadas en A:/temp/t09-abi-oraculo mediante lld-link del PATH; sin modificar NSLink ni C:. El resultado solo acredita el control C.
T09-014: ampliar solo el encabezado nuevo api.h con opcion de importacion DLL y agregar api-dll.h y AgregadosDll.nsproj al caso nuevo. Conservar la modalidad estatica y las pruebas anteriores intactas; comprobar el control C y dejar el consumidor NS pendiente de construccion.
T09-007, T09-008, T09-013, T09-014 y T09-016: agregar validar.py al caso nuevo de agregados para construir referencias C y comparar con NSC en O0-O3, aislando salidas en A:/temp y sin modificar PATH ni pruebas preexistentes. El guion queda preparado, sin ejecutar NSC hasta resolver la configuracion autorizada.
T09-007 y T09-008: comprobar sensibilidad del control C alterando cada funcion de agregados solo en copias bajo A:/temp y exigiendo un fallo identificable para cada una, con enlace estatico y DLL. No alterar el productor ni las pruebas originales.
T09-004 a T09-006 y T09-009 a T09-012: crear pruebas-tanda-09-llamadas con productor y control C, consumidor y proyectos NS nuevos y validador aislado. Cubrir llamadas cero argumentos y escalares, puntero, global mutable, promociones variadicas y enlace estatico y DLL; conservar las pruebas previas. Compilar control C en A:, no ejecutar NSC hasta resolver la configuracion.
T09-004 a 006 y T09-009 a 012: verificar la sensibilidad del control C mediante copias temporales que alteran individualmente retorno sin argumentos, escalares, puntero, global y variadica; exigir codigos de error distintos con biblioteca estatica y DLL. Las fuentes de prueba permanecen intactas.
T09-015: crear pruebas-tanda-09-simbolo-ausente con un encabezado C, consumidores C y NS y un proyecto NS nuevo. Verificar con Clang y lld-link que el simbolo solicitado falta, y preparar su comprobacion posterior con NSC. No modificar pruebas preexistentes.
T09-003: agregar pruebas-tanda-09-vectorcall con funcion __vectorcall y agregado C flotante, controles C y NS, enlace estatico y DLL y comparacion de simbolo. Conservar pruebas anteriores y validar primero solo el control C.
T09-010: preparar pruebas-tanda-09-global-const con global C de solo lectura y un intento de escritura en C y NS. Ejecutar el rechazo de Clang y conservar pendiente el rechazo de NSC hasta disponer de la construccion; no modificar pruebas existentes.
Autorizacion del usuario: indicar a CMake la ruta de NSLink resuelta por el PATH, A:/nslink-devel/bin/release/nslink.exe, para continuar la tanda 9. Se mantiene NSLink sin modificar y no se despliega en C:.
Se configura NSC_TEST_COMPILER para build-tanda09/bin/nsc.exe y NSC_CPP_INTEROP_SESSION_TESTS con raiz temporal A:/temp/t09-session-root. La construccion de desarrollo sigue aislada en A: y las pruebas se ejecutaran solo tras verificar sus entradas y rutas.
T09-011 y T09-012: ampliar solo los archivos nuevos api.h, api.c, Control.c y Main.ns del caso de llamadas con argumentos variadicos adicionales de enteros estrechos, long, unsigned, enum y record C; comparar las ocho combinaciones y conservar los casos previos.
T09-012: ampliar el mismo caso nuevo para invocar la funcion variadica directamente y mediante un typedef C de puntero a funcion; conservar valores y comparar estatica y DLL en O0-O3.
T09-013, T09-014 y T09-016: agregar prueba nueva de biblioteca NS precompilada con plantilla que consume funciones, global y variadica C; comparar biblioteca estatica y DLL con encabezado presente y ausente en O0-O3. Usar contrato NSMeta vigente, conservar pruebas existentes y temporales en A:.
T09-013 y T09-016: agregar prueba nueva de cache C directa en pruebas-tanda-09-cache, con aciertos e invalidacion por encabezados transitivos y por archivo opcional. Mantener la prueba anterior intacta, el contrato NSMeta vigente y salidas aisladas en A:.
T09-013: agregar prueba nueva de cache para una biblioteca C estatica cuyo contenido cambia conservando ruta y encabezado. Comparar ejecucion y estados de cache antes y despues; fuentes y salidas bajo A:, sin cambiar pruebas existentes ni NSLink.
T09-001 y T09-002: obtener HIR, MIR y LLVM reales del caso de llamadas C en modalidades estatica y DLL, con NSC construido de la revision actual; guardar cada salida en A:/temp y contrastar simbolos, argumentos y estados.
T09-007 y T09-008: ampliar el caso nuevo de agregados, sin quitar las comprobaciones previas, con records de 1 y 2 bytes y union de 4 bytes; repetir el contraste completo estatico y DLL O0–O3.
T09-001: agregar un control reproducible de volcados HIR y MIR usando el fixture de llamadas ya creado, una biblioteca C temporal y el compilador de desarrollo; comprobar identidad de llamada, invocacion indirecta y promociones sin modificar pruebas existentes.
T09-001: ampliar ese control propio para repetir los volcados con importacion DLL; conservar las comprobaciones de la biblioteca estatica.
T09-002: agregar un control LLVM nuevo para las llamadas directas e indirectas, promociones y global importada en biblioteca estatica y DLL, usando el fixture de llamadas ya existente y salidas aisladas en A:.
T09-003: investigar con un productor C aislado si una funcion con atributo sysv_abi, admitida por Clang en Windows x64, puede consumirse desde NativeSharp. Primero comparar un consumidor C y el diagnostico o resultado NS; no modificar el compilador antes de reproducir y localizar la causa.
T09-003, primer caso: habilitar solo llamadas directas `sysv_abi` no variadicas con parametros `int` y retorno `int` o `void`. Transportar la convencion HIR–MIR y aplicarla tanto a declaracion como llamada LLVM; rechazar otras firmas hasta demostrar su ABI. Agregar pruebas dirigidas nuevas con control C, estatica y DLL, y conservar las pruebas existentes. No modificar Clang, NSLink ni NSMeta.
T09-003: incorporar en la interfaz privada `LlvmCApi.h` la constante de convencion SysV del LLVM oficial; rechazar el caso de enlace dinamico con alias TLS mientras no este validado para esta convencion.
T09-003: impedir que la conversion de la funcion sysv_abi a un callback cdecl o vectorcall la trate como si fuera cdecl; probar el rechazo mientras no exista representacion segura del puntero SysV.
T09-003: comprobar el uso de la nueva llamada desde una plantilla NativeSharp precompilada con encabezado C oculto, mediante el contrato NSMeta vigente y sin introducir etiquetas historicas.
T09-003: ampliar las llamadas directas SysV a parametros y retornos `float` y `double`, conservando el rechazo para agregados y callbacks; contrastar llamadas mixtas y retornos con Clang en estatica y DLL O0–O3.
T09-003: ampliar el mismo caso a paso y retorno de `int*`, con escritura observable y comparacion C/NS en estatica, DLL y precompilado. Mantener rechazados los demas punteros hasta probarlos.
T09-003: ampliar SysV a enteros sin signo de 32 bits y enteros con y sin signo de 64 bits; comparar argumentos, retornos y llamada mixta con Clang en estatica, DLL y precompilado.
T09-003: consultar el IR de Clang para `signed char`, `unsigned char`, `short`, `unsigned short` y `_Bool` bajo sysv_abi antes de admitirlos; verificar atributos de extension en retorno y parametros.
T09-003: transportar signext/zeroext para enteros estrechos SysV en declaraciones y llamadas LLVM; contrastar enteros estrechos y bool contra Clang y mantener rechazo de agregados y callbacks.
T09-003: investigar la clasificacion SysV de agregados frente al ABI Windows mediante IR de Clang antes de habilitar cualquier record o union; no reutilizar por suposicion la clasificacion Win64.
T09-003: probar en un proyecto aislado bajo A:/temp la API publica CodeGen de Clang 23.1.0 para consultar las firmas LLVM de declaraciones C; no incorporar punteros del AST al contrato de NSC ni habilitar agregados durante la exploracion.
T09-003: convertir la sonda de API ABI Clang en una prueba nueva `CppClangAbiProbeTests.cpp` y target CTest independiente, sin modificar pruebas existentes. Verificar tipos fisicos y atributos para casos SysV de referencia.
T09-003: extender la sonda para consultar `CGFunctionInfo` de Clang y comprobar clasificacion logica de retorno y argumentos, ademas de la firma fisica, sin conservar objetos de Clang tras su sesion.
T09-003: prototipar una copia por valor del plan ABI Clang para los casos de la sonda, con tipos fisicos, clases logicas y atributos seleccionados; rechazar formas no representadas. Mantener el prototipo aislado del compilador de produccion y de NSMeta hasta verificar exhaustividad y transporte.
T09-003: prototipar el mapeo conservador de argumentos logicos a posiciones fisicas, incluyendo retorno oculto, expansion de estructuras y rechazo de padding o formas no verificadas; probar firmas con varios argumentos.
T09-003: conservar en el prototipo la identidad de destino y data layout efectivos; comprobar que sobreviven al cierre de la sesion de Clang, sin usar aun el plan en produccion.
T09-003: contrastar el plan copiado de un record SysV empaquetado con campos desalineados, incluidos el alineamiento del retorno y el del argumento byval, sin habilitar aun el caso en NSC.
T09-003: conservar simbolo fisico y convencion elegidos por Clang en el prototipo por valor, y comprobar una funcion vectorcall con nombre decorado despues de cerrar la sesion.
T09-003: ampliar las firmas SysV a `double*`, `void*` e `int**`; comprobar paso, retorno e identidad de los punteros en las pruebas C/NS y en la muestra precompilada.
T09-003: probar en el prototipo el caso void sin argumentos y el rechazo de una declaracion C sin prototipo; mantener el prototipo aislado de produccion.
T09-003: hacer que el prototipo rechace atributos de retorno y parametros no reconocidos en vez de descartarlos silenciosamente; verificar un caso con `returns_nonnull`.
T09-003: verificar la copia luego de destruir tambien el contexto LLVM, no solo el generador y el AST.
T09-003: trasladar el prototipo por valor a infraestructura interna de interoperabilidad y conservar la sonda existente; aun no conectarlo a snapshots, backend ni NSMeta. Despues probar la consulta desde una sesion real.
