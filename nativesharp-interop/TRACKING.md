# Tracking: interoperabilidad directa de C y C++ con NativeSharp

**Ubicación vigente:** `A:/nsc-devel/nativesharp-interop`. El historial, las
pruebas y la evidencia se trasladaron desde el fork de LLVM; detalles en
[TRASLADO-A-NSC.md](TRASLADO-A-NSC.md).

**Regla de ejecución:** completar las 18 tandas en orden. Implementar cada tanda
como un conjunto, registrando cada ítem o cambio en un commit propio con su
explicación en Markdown. Al terminar de implementar la tanda, ejecutar su
validación completa de forma agrupada y con construcción incremental. Si hay
errores, agrupar diagnóstico y correcciones y repetir la validación necesaria
hasta que pase antes de avanzar. No reconstruir todo ni ejecutar la batería
completa por cada ítem. Marcar un ítem o cerrar una tanda solo con evidencia.

El trabajo de interoperabilidad, sus pruebas y documentos pertenecen a NSC;
LLVM/Clang es una referencia de solo lectura. Daikon, .NET y el consumo de
NativeSharp desde C++ no forman parte de este trabajo.

## Objetivo y forma de usar este documento

Poder escribir programas en NativeSharp que consuman bibliotecas C y C++ de manera directa y cómoda, con una cobertura amplia de sus características. Las tandas ordenan el trabajo y su comprobación; completar una tanda no significa haber terminado la interoperabilidad.

La dirección es **bibliotecas C/C++ consumidas desde NativeSharp**. Que un programa C++ consuma bibliotecas NativeSharp está fuera del alcance. Daikon, .NET y la integración con vcpkg no forman parte de este trabajo. Los nombres de bibliotecas concretas son ejemplos de validación, no objetivos especiales.

Se prioriza Clang oficial y se extiende NSC. Una modificación del fork de Clang solo se considerará si se demuestra la necesidad y se documenta. La base acordada es LLVM/Clang 23.1.0; no se cambia de versión automáticamente. Las salidas de trabajo y pruebas permanecen en A:, sin desplegar en C:.

Cada casilla se marca solo con evidencia revisable de su resultado. Las casillas posteriores a la sesión inicial describen el recorrido previsto, no decisiones de implementación ya comprobadas: deben precisarse contra el código y ampliarse cuando se descubran requisitos. Esta lista no demuestra por sí sola que se hayan identificado todas las particularidades posibles de C++ ni que alguna característica ya funcione.

**Estado inicial:** documento de seguimiento; no autoriza ni implica comenzar la implementación. Las rutas y nombres de clases propuestos para la primera tanda deben contrastarse al comenzar. Los números identifican tareas; no obligan a ejecutar linealmente cuando exista una dependencia distinta.

## 01. Preparación y evidencia de partida

- [x] T01-001. Registrar el commit y la rama efectivos del repositorio NSC de trabajo. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T01-002. Registrar el commit y la rama efectivos del fork local de LLVM/Clang. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T01-003. Registrar la versión que reporta el Clang de A: y contrastarla con la base 23.1.0. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T01-004. Localizar la configuración CMake de LLVM que corresponde a esa instalación. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T01-005. Localizar la configuración CMake de Clang que corresponde a esa misma instalación. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T01-006. Registrar el directorio de recursos de Clang y comprobar que contiene sus encabezados internos. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T01-007. Registrar las rutas de headers y bibliotecas del entorno Windows que se utilizará en las pruebas. Evidencia: [línea base de la tanda 01](LINEA-BASE-TANDAS.md#tanda-01-entorno-de-trabajo-verificado-el-2026-11-05).
- [x] T01-008. Registrar el enlazador efectivo y las opciones relevantes de compilación de NSC. Evidencia: [línea base de la tanda 01](LINEA-BASE-TANDAS.md#tanda-01-entorno-de-trabajo-verificado-el-2026-11-05).
- [x] T01-009. Identificar cambios preexistentes en NSC antes de editar cualquier archivo. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T01-010. Identificar cambios preexistentes en el fork de LLVM antes de editar cualquier archivo. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T01-011. Definir directorios en A: para construcción, temporales, fixtures y resultados. Evidencia: [línea base de la tanda 01](LINEA-BASE-TANDAS.md#tanda-01-entorno-de-trabajo-verificado-el-2026-11-05).
- [x] T01-012. Revisar las instrucciones vigentes de los directorios que se vayan a modificar. Evidencia: [línea base de la tanda 01](LINEA-BASE-TANDAS.md#tanda-01-entorno-de-trabajo-verificado-el-2026-11-05).
- [x] T01-013. Guardar la línea base y sus fuentes de evidencia junto a este tracking. Evidencia: [línea base de la tanda 01](LINEA-BASE-TANDAS.md#tanda-01-entorno-de-trabajo-verificado-el-2026-11-05).

## 02. Construcción opcional de la integración con Clang

- [x] T02-001. Añadir a `CMakeLists.txt` la opción propuesta `NSC_ENABLE_CPP_INTEROP`, desactivada por defecto. Evidencia: [línea base](LINEA-BASE-TANDAS.md) y código.
- [x] T02-002. Hacer que la configuración desactivada no busque el paquete CMake de Clang. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T02-003. Buscar el paquete CMake de Clang únicamente con la opción activada. Evidencia: [línea base](LINEA-BASE-TANDAS.md) y código.
- [x] T02-004. Comprobar que la versión descubierta de Clang sea exactamente 23.1.0. Evidencia: [configuración válida y fixture de versión incorrecta](LINEA-BASE-TANDAS.md).
- [x] T02-005. Comprobar la versión del paquete LLVM empleado por NSC. Evidencia: [tanda 02](TANDA-02-CONSTRUCCION.md).
- [x] T02-006. Resolver las rutas reales de las instalaciones LLVM y Clang seleccionadas. Evidencia: [tanda 02](TANDA-02-CONSTRUCCION.md).
- [x] T02-007. Rechazar una combinación de paquetes LLVM y Clang de instalaciones incompatibles. Evidencia: [prueba negativa](LINEA-BASE-TANDAS.md).
- [x] T02-008. Emitir un diagnóstico de configuración que identifique la versión de Clang incorrecta. Evidencia: [fixture de versión incorrecta](LINEA-BASE-TANDAS.md).
- [x] T02-009. Emitir un diagnóstico de configuración que identifique paquetes de instalaciones incompatibles. Evidencia: [prueba negativa](LINEA-BASE-TANDAS.md).
- [x] T02-010. Crear el target aislado propuesto `nsc_cpp_interop`. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [x] T02-011. Incorporar al target los encabezados públicos y fuentes privadas de la sesión. Evidencia: [tanda 02](TANDA-02-CONSTRUCCION.md).
- [x] T02-012. Enlazar los componentes de Clang realmente utilizados por el target. Evidencia: [sesión real](LINEA-BASE-TANDAS.md); la prueba inicial enlaza `clangTooling`.
- [x] T02-013. Comprobar que las dependencias de Clang no crean un ciclo con el frontend de NSC. Evidencia: [prueba combinada de sesión y frontend](LINEA-BASE-TANDAS.md).
- [x] T02-014. Mantener la configuración de enlace estático del runtime del compilador anfitrión. Evidencia: [tanda 02](TANDA-02-CONSTRUCCION.md).
- [x] T02-015. Configurar la construcción nueva en A: sin reutilizar el caché asociado al Clang de C:. Evidencia: [línea base](LINEA-BASE-TANDAS.md).
- [x] T02-016. Comprobar que el enlazador usado por la construcción procede de la configuración elegida. Evidencia: [tanda 02](TANDA-02-CONSTRUCCION.md).
- [x] T02-017. Registrar las dependencias opcionales de LLVM que se incorporan efectivamente. Evidencia: [tanda 02](TANDA-02-CONSTRUCCION.md).
- [x] T02-018. Registrar las opciones efectivas de RTTI y excepciones del compilador anfitrión. Evidencia: [tanda 02](TANDA-02-CONSTRUCCION.md).
- [x] T02-019. Verificar que activar la integración no añada automáticamente una sesión a compilaciones normales. Evidencia: [tanda 02](TANDA-02-CONSTRUCCION.md).
- [x] T02-020. Documentar los comandos reproducibles para configurar con la integración activada y desactivada. Evidencia: [tanda 02](TANDA-02-CONSTRUCCION.md).

## 03. Contrato público de una sesión Clang

- [x] T03-001. Definir en `CppSessionOptions.h` el modo de lenguaje C o C++ de una sesión.
- [x] T03-002. Definir la versión de lenguaje solicitada entre las opciones de la sesión.
- [x] T03-003. Definir el triple objetivo entre las opciones de la sesión.
- [x] T03-004. Definir el archivo principal o contenido de entrada entre las opciones de la sesión.
- [x] T03-005. Definir la representación de los directorios de inclusión.
- [x] T03-006. Definir la representación de macros configuradas por el usuario.
- [x] T03-007. Definir cómo se proporciona el directorio de recursos de Clang.
- [x] T03-008. Definir cómo se proporciona el entorno de headers del sistema.
- [x] T03-009. Definir una identidad de configuración reproducible para distinguir sesiones.
- [x] T03-010. Definir en `CppSessionDiagnostic.h` las severidades que NSC recibe.
- [x] T03-011. Definir el archivo, línea y columna de cada ubicación de diagnóstico.
- [x] T03-012. Definir el mensaje y la identificación del diagnóstico transportado.
- [x] T03-013. Definir cómo conservar notas y ubicaciones adicionales asociadas a un diagnóstico.
- [x] T03-014. Definir el resultado de apertura y sus causas de fallo en `CppInteropSession.h`.
- [x] T03-015. Definir los estados observables de una sesión.
- [x] T03-016. Definir una operación explícita de cierre y su resultado.
- [x] T03-017. Definir el comportamiento de cerrar dos veces una sesión.
- [x] T03-018. Definir el mecanismo de cancelación `CppCancellationToken`.
- [x] T03-019. Definir la propiedad y vida de los datos expuestos por la sesión.
- [x] T03-020. Definir en `CppInteropSessionFactory.h` la operación de apertura.
- [x] T03-021. Mantener tipos internos de Clang fuera de la interfaz pública propuesta de NSC.
- [x] T03-022. Documentar las restricciones de concurrencia del contrato público.

Evidencia conjunta: [tanda 03](TANDA-03-CONTRATO.md).

## 04. Implementación interna de la sesión

- [x] T04-001. Crear la implementación privada propuesta `ClangSession.cpp`.
- [x] T04-002. Crear el constructor de invocación propuesto `ClangInvocationBuilder.cpp`.
- [x] T04-003. Traducir el modo C o C++ a los argumentos correspondientes de Clang.
- [x] T04-004. Traducir la versión de lenguaje a la invocación de Clang.
- [x] T04-005. Traducir el triple objetivo a la invocación de Clang.
- [x] T04-006. Incorporar cada directorio de inclusión a la invocación.
- [x] T04-007. Incorporar cada definición de macro a la invocación.
- [x] T04-008. Incorporar el directorio de recursos explícito a la invocación.
- [x] T04-009. Incorporar las rutas configuradas de headers del sistema a la invocación.
- [x] T04-010. Rechazar una configuración que no identifique los recursos necesarios.
- [x] T04-011. Crear el consumidor de diagnósticos privado `ClangDiagnosticConsumer.cpp`.
- [x] T04-012. Copiar la severidad de cada diagnóstico Clang al formato de sesión.
- [x] T04-013. Copiar el mensaje y sus ubicaciones al formato de sesión.
- [x] T04-014. Conservar las notas asociadas con su diagnóstico principal.
- [x] T04-015. Asegurar que el consumidor viva más que el motor de diagnósticos que lo utiliza.
- [x] T04-016. Crear el `CompilerInstance` con la configuración construida.
- [x] T04-017. Inicializar los diagnósticos de `CompilerInstance` con propiedad explícita.
- [x] T04-018. Crear la acción de frontend propuesta `ClangSessionAction.cpp`. Evidencia: [tanda 04](TANDA-04-SESION.md).
- [x] T04-019. Llamar a `BeginSourceFile` una sola vez por apertura de unidad.
- [x] T04-020. Omitir `EndSourceFile` si `BeginSourceFile` falla.
- [x] T04-021. Ejecutar la acción de frontend sin repetir la inicialización de la unidad.
- [x] T04-022. Consultar los diagnósticos de error además del resultado de ejecución de Clang.
- [x] T04-023. Mantener vivos el AST y el estado semántico durante la vida útil necesaria de la sesión.
- [x] T04-024. Cerrar la unidad exactamente una vez después de una apertura exitosa.
- [x] T04-025. Liberar los recursos de sesión en el orden compatible con sus dependencias.
- [x] T04-026. Evitar una finalización global de LLVM al cerrar una sesión individual.
- [x] T04-027. Comprobar la cancelación antes de iniciar el análisis de una unidad.
- [x] T04-028. Propagar la cancelación durante los puntos de trabajo que Clang permita interrumpir.
- [x] T04-029. Evitar que dos ejecutores accedan simultáneamente al mismo contexto sin coordinación.
- [x] T04-030. Mantener independientes las opciones y los recursos de sesiones distintas.

## 05. Incorporación al flujo de NSC

- [x] T05-001. Localizar la creación actual de `ProjectCompilation` en cada ruta de compilación. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).
- [x] T05-002. Añadir el punto de inyección opcional de sesión en `ProjectCompilation.h`. Evidencia: [prueba de inyección](LINEA-BASE-TANDAS.md).
- [x] T05-003. Implementar en `ProjectCompilation.cpp` el almacenamiento de la sesión inyectada. Evidencia: [prueba de inyección](LINEA-BASE-TANDAS.md).
- [x] T05-004. Añadir un acceso controlado a la sesión desde las fases que la requieran. Evidencia: [prueba de inyección](LINEA-BASE-TANDAS.md).
- [x] T05-005. Definir el orden de destrucción entre `ProjectCompilation` y la sesión. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).
- [x] T05-006. Integrar la configuración opcional en la ruta correspondiente de `DriverExecution.cpp`. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).
- [x] T05-007. Revisar la creación de compilaciones desde el servicio de lenguaje. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).
- [x] T05-008. Preservar la vida útil de la sesión cuando existan snapshots de `ProjectCompilation`. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).
- [x] T05-009. Crear `CppDiagnosticAdapter.cpp` en la capa frontend, sin dependencia inversa del target interop. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).
- [x] T05-010. Convertir las severidades de sesión a severidades de NSC. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).
- [x] T05-011. Convertir ubicaciones y notas de sesión a diagnósticos de NSC. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md); la presentación en el servicio de lenguaje queda pendiente.
- [x] T05-012. Conservar el comportamiento actual si no se solicita una sesión C/C++. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).
- [x] T05-013. Informar un error claro si se solicita interop en una construcción que lo tiene desactivado. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).
- [x] T05-014. Evitar crear una sesión por un fallo anterior que impida su uso. Evidencia: [tanda 05](TANDA-05-INTEGRACION.md).

## 06. Verificación de la primera tanda

- [x] T06-001. Registrar el módulo de pruebas de sesión en la infraestructura de pruebas que corresponda. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-002. Asegurar que los temporales de esas pruebas se crean dentro de A:. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-003. Comprobar que una sesión C abre una unidad con `bridge_probe` y usa los recursos de Clang. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [x] T06-004. Comprobar que una sesión C++ abre una unidad con una clase de prueba. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [x] T06-005. Comprobar el diagnóstico de una versión de Clang incompatible. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-006. Comprobar que dos configuraciones diferentes producen identidades distintas. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-007. Comprobar el fallo por recursos de Clang ausentes. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [x] T06-008. Comprobar que una unidad inválida entrega ubicación y mensaje de error. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [x] T06-009. Comprobar que un fallo de `BeginSourceFile` no intenta finalizar una unidad no abierta. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-010. Comprobar que cerrar dos veces no libera recursos dos veces. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [x] T06-011. Comprobar la cancelación y la liberación posterior de recursos. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-012. Comprobar que dos sesiones con opciones distintas no se contaminan. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-013. Comprobar la inyección de una sesión en `ProjectCompilation`. Evidencia: [pruebas con sesión simulada y sesión Clang real](LINEA-BASE-TANDAS.md).
- [x] T06-014. Comprobar una compilación NativeSharp normal con la integración desactivada. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-015. Comprobar una compilación NativeSharp normal con la integración activada y sin sesión solicitada. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-016. Inspeccionar importaciones normales y diferidas de los ejecutables producidos. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).
- [x] T06-017. Registrar resultados, comandos y limitaciones de la primera tanda. Evidencia: [tanda 06](TANDA-06-VALIDACION.md).

## 07. Entrada de bibliotecas y modelo de importación

- [x] T07-001. Auditar cómo `ProjectLoader.cpp` lee actualmente referencias de bibliotecas. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-002. Auditar cómo `Project.h` conserva las opciones del proyecto. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-003. Auditar cómo el grafo del proyecto propaga dependencias entre módulos. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-004. Auditar qué datos de entrada conserva el manifiesto de compilación actual. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-005. Diseñar cómo declarar que una dependencia expone headers C o C++. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-006. Diseñar cómo indicar el header principal de una dependencia. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-007. Diseñar cómo especificar sus directorios de inclusión. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-008. Diseñar cómo especificar sus macros y opciones de lenguaje. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-009. Diseñar cómo especificar bibliotecas estáticas de enlace. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-010. Diseñar cómo especificar bibliotecas de importación y DLL de ejecución. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
  - Avance parcial: `CppLibraryReference` permite declarar ambas y una biblioteca NativeSharp estática puede registrar y empaquetar las dependencias declaradas. Se valida la correspondencia para entradas COFF cortas y se probó un archivo que importa dos DLL encadenadas, con consumidor ON y OFF. Pendientes los formatos de importación larga, otras topologías multidll y el cierre completo de dependencias transitivas. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-incremental-archivo-de-importación-con-varias-dll-2026-11-05).
  - Importación larga: se lee el nombre del descriptor en el caso AMD64 probado, pero se rechaza el enlace porque el PE de NSC omitió la DLL. La barrera también cubre el fixture mixto de entradas largas y cortas; falta resolver el enlace. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-incremental-importación-coff-larga-de-estilo-ms-2026-11-05).
  - Caché multidll: se observó un acierto y se rechazaron la DLL alterada o ausente y el `.lib` alterado. Tras actualizar la DLL, la biblioteca de importación y NSMETA, se observó una clave nueva y el comportamiento cambió de 42 a 47; quedan otras topologías por verificar. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-incremental-actualizar-dll-y-biblioteca-de-importación-multidll-2026-11-05).
- [x] T07-011. Validar las opciones de importación antes de crear la sesión Clang. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-012. Conservar la procedencia de cada declaración importada. Alcance demostrado: categorías actualmente representadas en los inventarios; no cubre todo C++. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-013. Hacer disponibles los diagnósticos del header en la compilación NativeSharp. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-014. Registrar los archivos incluidos para detectar cambios en dependencias. Se registran los archivos observados; el cierre completo de dependencias para caché sigue pendiente. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-015. Comprobar la carga de dos bibliotecas con configuraciones independientes. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).
- [x] T07-016. Comprobar que cambiar un header invalida el resultado que dependa de él. Verificado mediante reanálisis con caché C/C++ deshabilitada; invalidación de caché activa pendiente. Evidencia: [tanda 07](TANDA-07-REFERENCIAS.md).

## 08. Declaraciones, nombres y tipos C

**Decisión vigente:** NativeSharp aún no está liberado. Se trabaja con un
único compilador y el formato NSMETA vigente; no se introducen nuevas versiones
ni se exige compatibilidad con versiones históricas. Las notas anteriores sobre
esa compatibilidad quedan como historial y no son requisitos activos.

[Ver seguimiento detallado por casillas](TRACKING-TANDA-08-DETALLADO.md). La vista
detallada conserva los ítems originales y distingue muestras verificadas de
trabajo y validación pendientes.

**Cierre 2026-11-10.** La tanda 8 queda cerrada con limitaciones. El estado vigente es el del
[seguimiento detallado](TRACKING-TANDA-08-DETALLADO.md#estado-de-cierre-2026-11-10).

- T08-002 a T08-020 y las decisiones T08-D01 a T08-D26 están resueltas, con D21 y T08-GATE-05 cerradas por
  decisión del usuario.
- Los gates histórico y complementario dieron código 0 sobre el código de `0ead9a1f6`. El ctest seleccionado dio
  28 de 29.
- `nsc.stage48_2.library-metadata` sigue sin aprobar: dos de sus 444 comprobaciones dependen del fixture histórico
  1.46 ausente. La prueba se conserva intacta y no se introduce compatibilidad histórica de NSMeta.
- El ctest ampliado (696 pruebas: 131 aprobadas, 457 no ejecutadas y 108 fallidas) no se presenta como aprobado y
  no todos sus fallos tienen una causa demostrada.
- Límites aceptados: D23 (dirección de elementos de arreglos de punteros calificados), D26 (record opaco frente a
  completo) y D11/D12 (avisos que no cubren todas las formas de expresión).
- Historial: se conservan `development-cpp-clang`, las dos propuestas agrupadas y los respaldos. La rama agrupada
  de cierre es `propuesta/t08-agrupado-cierre`.
- La tanda 9 no se empezó.

Las notas de esta sección anteriores a esta fecha quedan como historial.

[Cierre técnico](registro-commits/t08-cierre-tecnico-20261110.md),
[D26](registro-commits/t08-d26-identidad-tipos-c-productores.md),
[propuesta D21](registro-commits/t08-d21-propuesta-agrupacion.md).

El validador integral incluye los scripts de la tanda inventariados. La última
ejecución integral registrada terminó con cuatro fallos en controles TLS de DLL
C; desde entonces se añadieron controles dirigidos sin repetir el gate. La tanda
sigue abierta y la siguiente ejecución integral queda pendiente tras agrupar
las correcciones. [Resultado](registro-commits/t08-validacion-link-lib-auto.md),
[cobertura](registro-commits/cobertura-validador-tanda-08.md).

Actualización 2026-11-07: se repitió el gate agrupado con los controles TLS
atómicos precompilados. Terminó con código 1 y cuatro fallos visibles de TLS
de DLL; una repetición dirigida también identificó el fallo silencioso de enum
mixto. T08 sigue abierta. [Resultado](registro-commits/t08-validacion-integral-20261107.md).
Posteriormente se corrigió el caso de enum C `unsigned int` con `int`, que
pasó su control dirigido O0–O3; falta repetir el gate integral tras agrupar
las correcciones pendientes. [Evidencia](registro-commits/t08-enum-c-uint-int.md).

- [x] T08-001. Auditar la representación y búsqueda de nombres actual de NSC. Evidencia: [auditoría de tipos C](TANDA-08-TIPOS-C.md).
- [x] T08-002. Definir una identidad estable para cada declaración C importada.
  - Validación agrupada vigente: pasaron los controles dirigidos de identidad,
    los consumidores de la matriz actual y la restauración de cinco variantes
    de productor en O0–O3. La evidencia y las exclusiones están en
    [validación agrupada](registro-commits/t08-identidad-validacion-agrupada-actual.md).
    Las casillas detalladas describen muestras; aún no demuestran identidad
    estable para cada declaración C importada, por lo que T08-002 sigue abierta.
  - Una variable de `catch` que oculta una global C dejó de publicarla en la
    prueba de biblioteca precompilada O0–O3. Se ejecutó el filtro al lanzar
    una excepción con `CALL` al validador en O0–O3.
    [Resultado](registro-commits/t08-h3-catch-resultado.md).
    [Filtro](registro-commits/t08-h3-filtro-catch-ejecucion.md).
    Un inicializador de desestructuración conserva la función C homónima en
    O0–O3. [Límites de ámbito](registro-commits/t08-h3-limites-ambito-control.md).
  - La desestructuración simple y anidada, incluido el descarte `_`, no captura
    las declaraciones C homónimas en los casos comprobados. La biblioteca y el
    consumidor pasaron O0–O3; otras formas de ámbito y la tanda siguen abiertas.
    [Evidencia](registro-commits/t08-h3-desestructuracion-descarte-resultado.md).
  - H3 sombras: los casos probados de locales en bloques, parámetros de métodos
    y lambdas, y variables de `for` ya no publican la global C homónima; el
    local de bloque tampoco emite la función inline C homónima. Los consumidores
    precompilados y controles de metadata pasaron
    O0–O3; también se conservaron llamadas C en inicializadores y fuera de los
    ámbitos sombreados. Otras formas de ámbito siguen pendientes. [Evidencia
    inicial](registro-commits/t08-h3-sombra-local-validacion.md), [parámetros y
    bucles](registro-commits/t08-h3-sombras-parametro-bucle-validacion.md),
    [lambdas e inicializadores](registro-commits/t08-h3-inicializadores-lambda-validacion.md).
    Una variable foreach sobre arreglo también pasó O0–O3 en el consumidor
    precompilado. [Control](registro-commits/t08-h3-foreach-control.md).
  - NSMETA 1.47: el control de tolerancia de versiones pasó 17 comprobaciones.
    El control general pasó 430 de 434; faltan las muestras históricas 1.45 y
    1.46 que el control exige. Esos cuatro fallos no se consideran aprobados.
    [Evidencia](registro-commits/t08-nsmeta-147-compatibilidad.md).
  - Se recuperó una muestra 1.45 producida antes de la introducción de 1.46 y
    el lector actual la verificó. Falta una muestra independiente 1.46 y volver
    a ejecutar la suite. [Procedencia](registro-commits/t08-nsmeta-145-fixture-recuperado.md).
  - Se conservaron muestras 1.47 y 1.48 generadas antes de introducir sus
    respectivas versiones sucesoras; el lector actual las verificó. La 1.48
    procede de la suite del escritor histórico. Falta una muestra independiente
    1.46 y repetir la suite completa de metadata. [1.47](registro-commits/t08-nsmeta-147-fixture-recuperado.md),
    [1.48](registro-commits/t08-nsmeta-148-fixture-recuperado.md).
  - Actualización H3: el catálogo 1.46 publica dependencias candidatas de alias,
    funciones externas y `static inline`, y globales externas e internas usadas
    por plantillas. Los controles específicos de esas cinco categorías pasaron
    en O0–O3. El validador de identidad pasó con sus expectativas actualizadas;
    inicialmente conservaba como rechazo conocido el record C local. Ese caso
    ya compila y se ejecuta cuando el record está también en la superficie del
    productor. Un fixture separado pasó en O0–O3 cuando el record solo aparece
    en la firma de una función C utilizada en la plantilla.
    [Evidencia](registro-commits/t08-h3-funcion-record-publicado.md). Esas
    pruebas no cubren todas las declaraciones ni cierran T08-002. Los apuntes
    H3 anteriores de este bloque describen estados históricos. [Evidencia](registro-commits/t08-h3-identidad-expectativas.md).
  - Un alias de enum C conciliado entre encabezado y biblioteca precompilada ya
    no se rechaza como tipo nativo distinto. El parser recibe los nombres del
    catálogo precompilado antes del análisis; se comprobó su visibilidad con
    proveedor simulado y el consumo real solo con metadata. La persistencia de
    funciones, globales y alias usados por plantillas sigue pendiente.
    [Conciliación](registro-commits/t08-enum-identidad-precompilada.md),
    [parser](registro-commits/t08-parser-tipos-c-precompilados.md),
    [visibilidad](registro-commits/t08-visibilidad-parser-metadata.md).
  - H5: los campos de records C con la misma identidad y definiciones
    estructuralmente compatibles se seleccionan aunque sus encabezados difieran.
    La matriz comprobó lecturas O0–O3 y mantuvo el rechazo de records distintos.
    Un control precompilado adicional verificó lectura y escritura O0–O3 con y
    sin encabezado. [Evidencia](registro-commits/t08-h5-records-c-precompilado.md).
  - H8: los catálogos C 1.42 con solo enums se decodifican como formato
    heredado usando el minor declarado. Pasaron las variantes verificadas de
    1.42–1.45 y un consumidor real 1.42. El ítem sigue abierto por otros casos.
    [Evidencia](registro-commits/t08-h8-catalogo-142.md).
  - H3: el observador captura demandas de cuerpos genéricos y la compilación
    las conserva asociadas a su plantilla. El control pasó con función, global
    y local sombreado: una demanda no identifica por sí sola una declaración C.
    El codec interno conserva también funciones C `static inline` seleccionadas
    y su símbolo particular. Todavía faltan la resolución de sombras, tipos y
    conflictos, y la publicación y restauración de las dependencias; no se
    modificó el formato NSMETA ni se cierra el ítem.
    [Demandas](registro-commits/t08-h3-demandas-por-plantilla.md),
    [inline](registro-commits/t08-h3-captura-inline-c.md),
    [análisis](T08-002-PROPUESTA-METADATA-C.md#revisión-de-la-ruta-actual--2026-11-07).
  - H1: se aceptan typedef C equivalentes observados en encabezados con nombres
    distintos; la matriz conserva los rechazos por tipos y records incompatibles.
    La validación dirigida de identidad pasó, sin cerrar el ítem.
    [Evidencia](registro-commits/t08-h1-typedef-c-equivalentes.md).
  - Para enumeradores anónimos no basta que dos archivos tengan bytes idénticos:
    `GenIdentical.h` es otra ruta y la matriz exige ambigüedad; con el mismo
    `Gen.h` exige ejecución. La prueba anterior pasó ambas expectativas.
    [Evidencia](registro-commits/t08-h3-enum-anonimo-archivo-identico.md).
  - H7: las constantes de enum C dejan de publicarse como estados de
    NativeSharp. Los productores y consumidores ordinarios antes rechazados
    pasaron el validador dirigido; algunas plantillas siguen sin resolver la
    constante cuando el catálogo no la contiene. [Evidencia](registro-commits/t08-h7-constantes-c-publicacion.md).
  - La validación dirigida del carril integrado pasó tras actualizar siete
    expectativas de enum de campo ya resueltas en esta rama. El validador aún
    comprueba rechazos esperados de capacidades pendientes; no cierra el ítem.
    [Evidencia](registro-commits/t08-identidad-enum-campo-expectativas.md).
  - El cuarto conjunto del carril aportó reproducciones y propuestas para
    constantes de enum en bibliotecas (H7), compatibilidad de catálogo (H8)
    y formas adicionales de USR de typedef. El parche H7 está sin aplicar;
    estos resultados requieren revisión y validación en la rama actual.
    [Resultados](registro-commits/t08-002-resultados-identidad-c-4.md),
    [H7](T08-002-H7-CONSTANTES.md), [H8](T08-002-H8-CATALOGO.md).
  - El tercer conjunto del carril aportó una matriz de typedef, casos de
    plantillas con dependencias C y diagnósticos H4–H8. Son resultados del
    carril y propuestas sujetos a revisión frente al estado actual de NSC.
    [Resultados](registro-commits/t08-002-resultados-identidad-c-3.md),
    [enum de campo](T08-002-PROPUESTA-ENUM-CAMPO.md),
    [global static](T08-002-GLOBAL-STATIC.md).
  - El segundo conjunto del carril reportó colisiones de USR de declaraciones
    internas en encabezados homónimos y restauración de cinco productores por
    la ruta del driver O0–O3. Dejó propuestas para typedef equivalentes y
    dependencias C de plantillas. Hallazgos pendientes de revisión en esta rama:
    [resultados](registro-commits/t08-002-resultados-identidad-c-2.md),
    [typedef](T08-002-PROPUESTA-TYPEDEF.md), [metadata](T08-002-PROPUESTA-METADATA-C.md).
  - El carril de identidad reportó USR estables entre sesiones y restauración
    de records, unions, campos, enums y enumeradores desde tres productores en
    O0–O3. Funciones, globales y alias no viajan en ese catálogo; el caso de
    typedef idéntico en encabezados distintos queda pendiente. Evidencia del
    carril aún por validar en esta rama. [Auditoría](T08-002-IDENTIDAD-AUDITORIA.md),
    [resultados](registro-commits/t08-002-resultados-identidad-c.md).
  - H4 partes A y B: enums declarados dentro de records C se capturan y los
    campos escalares de enum anónimo se proyectan como el entero subyacente.
    El consumo directo y el enum nombrado precompilado pasaron O0–O3. La
    publicación del enum anónimo permanece rechazada sin metadata nueva.
    [Resultado](registro-commits/t08-h4-enum-campo-validacion.md).
  - Un productor NSC real conservó en su NSMETA los USR de Clang del registro
    C y todos sus campos; la comparación con Clang y el consumidor separado
    pasaron O0–O3. Esto no demuestra que el consumidor restaure cada USR ni
    cubre las demás categorías. [Evidencia](registro-commits/t08-usr-productor-consumidor-real.md).
  - Redeclaraciones adelantadas adicionales de struct y union opacos en el
    mismo header pasaron O0–O3 con el consumidor directo y precompilado; se
    mantuvieron los rechazos de origen distinto y header modificado. No cubre
    todas las redeclaraciones. [Evidencia](registro-commits/redeclaraciones-opacas-c.md).
  - Declaraciones adelantadas de struct y union del mismo header se
    reconciliaron entre productor precompilado y consumidor en O0–O3. Una
    copia en otra ruta y el header modificado fueron rechazados. Catálogos
    anteriores y la extensión de procedencia pasaron la suite de sesión.
    Faltan otros casos de identidad. [Evidencia](registro-commits/prueba-opacos-header-compartido.md).
  - La reconciliación del mismo encabezado C en biblioteca precompilada y
    consumidor compiló, enlazó y ejecutó de O0 a O3. Un encabezado modificado
    se rechazó; la suite de sesión pasó. No acredita todas las identidades ni
    cierra la tanda. [Evidencia](registro-commits/firmas-record-c-reconciliado.md).
  - Preparados el transporte y la validación del destino reconciliado; aún no
    afectan la resolución de nombres y esperan validación agrupada.
    [Detalle](registro-commits/validacion-destino-identidad-c.md).
  - Preparada la búsqueda de correspondencias entre records Clang y catálogos
    visibles, con comprobación de origen y dependencias. Aún no cambia las
    conversiones ni se ha validado. [Detalle](registro-commits/mapa-reconciliacion-c.md).
  - La comparación de origen y dependencias de records C pasó la suite de
    sesión, incluidos ciclos y enums. No está conectada a la resolución y no
    cierra el caso compartido. [Detalle](registro-commits/comparacion-cierre-c.md).
  - Preparada comparación del record raíz que verifica contrato, archivo,
    offset y huella vigente. Falta validarla y comprobar dependencias transitivas;
    aún no se habilita la conciliación. [Detalle](registro-commits/origen-compartido-record-c.md).
  - El productor real conserva procedencia en metadata 1.43; la suite y el
    consumidor con encabezado independiente pasaron. El encabezado compartido
    sigue fallando por conflicto de alias. [Evidencia](registro-commits/orden-version-productor-c.md).
  - La suite pasó con propagación del origen al snapshot y un codec versionado
    preparado que preserva 1.42. No está conectado a las bibliotecas ni une
    identidades. [Evidencia](registro-commits/codec-procedencia-c.md).
  - Clang captura la huella del buffer y el offset de records con ubicación
    directa; la suite pasó. Los nombres pueden ser virtuales y estos datos aún
    no se serializan ni unen identidades. [Evidencia](registro-commits/procedencia-buffer-record-c.md).
  - Existe una comparación de contratos completos entre módulos, validada por
    la suite de sesión. No prueba procedencia común ni habilita conversiones;
    el caso de encabezado compartido sigue fallando. [Evidencia](registro-commits/comparacion-contratos-record-c.md).
  - Un consumidor con biblioteca NS precompilada y un encabezado C independiente
    pasó de O0 a O3 tras instalar el catálogo en la observación preliminar. La
    variante que importa el mismo encabezado terminó en O0 con conflicto de
    alias `CRecord`; falta conciliar identidades y la batería sigue en error.
    [Evidencia](registro-commits/observacion-c-encabezados-independientes.md).
  - Con `0e980fb65`, un proyecto que usa `var` para records C de 8 y 12 bytes
    sin nombrar los tipos compiló, enlazó y ejecutó de O0 a O3; la matriz de
    metadata existente también pasó. No acredita todas las formas de inferencia.
  - Con los cambios hasta `5da7cf5b6`, la matriz de metadata C compiló, enlazó
    y ejecutó de O0 a O3, incluidos retorno directo de record de 8 bytes y
    parámetro/retorno indirectos de record de 12 bytes. La suite de sesión pasó
    con código 0; persistieron 29 advertencias en su compilación. No demuestra
    todos los layouts, ABI ni completa la validación de la tanda.
  - Preparados `b2e5033ec`, `971340701` y `2ec390076`: proyección de records C
    por valor con layout comprobado, controles semánticos y fixture directo e
    indirecto. Aún no construidos ni ejecutados; no se acredita el ABI.
  - Con `81b6851c9`, el productor/consumidor con enum C en firma, campo y
    constante pasó O0–O3. La variante que devuelve record C por valor falló
    en O0 al importar `get_c_record_value`; `mapFunctionType` limita records a
    punteros. La matriz completa termina con error y el caso sigue pendiente.
  - Comprobación real con `20d0a28e0`: NSC construyó el productor con tipo C en
    su API, el consumidor enlazó por LibraryReference y su ejecutable terminó
    con código 0 en O0. La suite de sesión pasó después de corregir el fixture
    1.41 en `96aaac9df`, incluidos los controles de lectura/escritura 1.42.
    Persisten advertencias de compilación, la reconciliación entre sesión y
    catálogo, más variantes y la batería completa de la tanda.
  - Cambio posterior a las notas históricas de abajo: `7c9668a9f`, `b2d88f6f7`
    y `2fbe2256f` habilitan NSMETA 1.42 y conectan el productor y consumidor de
    catálogos C. `4c399a685` incorpora el fixture real a la batería. Los cambios
    aún no se han construido ni ejecutado; falta compatibilidad y validación de
    extremo a extremo, además de conciliar identidades compartidas.
  - Comprobación agrupada tras `1a29352cc`: construcción incremental del objetivo
    `nsc_cpp_session_tests` y ejecución de la suite terminaron con código 0. En
    la primera ejecución hubo cinco fallos de fixtures; se corrigieron en
    `e5a47ea7f` y `1a29352cc`. La compilación emitió advertencias de campos sin
    inicializador. Esto no sustituye la validación completa ni cierra el ítem.
  - Preparados `0760bfe48` y `3fcdb2f21`: comprobación de tipos C en el
    proveedor del cierre externo y controles de módulo ausente y destino
    incompatible. Sin construir ni ejecutar; el recorrido real sigue abierto.
  - Preparados `96ac0e1f1`, `db982963e` y `7fc29af1d`: reconocimiento del
    catálogo C validado al verificar referencias NSMETA, con controles de tipos
    locales, colisiones, destino y dependencias externas. Sin construir ni
    ejecutar; 1.42 y la conexión al productor permanecen deshabilitados.
  - Preparado `667ec969e`: control sintético que configura el selector y los
    proveedores combinados en un consumidor, con declaración NativeSharp, tipo,
    campo y enumerador C. No se ha construido ni ejecutado; no prueba NSMETA real
    ni enlace y no habilita la ruta pública.
  - Preparados `0356e4fd1` y `3f8e408c0`: proveedores combinados de campos y
    valores, con filtros por visibilidad y controles de colisiones. Sin ejecutar
    ni conectar al driver; falta conciliar identidades compartidas y comprobar
    la interacción con snapshots de miembros precompilados.
  - Preparados `0cd9159d5`, `0e15a9d9e`, `3d0ccfb7b` y `caa3f7f89`:
    reconciliación de declaraciones adelantadas de enum y captura de enums
    usados por firmas o campos en el proveedor, con controles. Sin construir
    ni ejecutar; el proveedor no está instalado y 1.42 sigue deshabilitado.
  - Preparados `aa79163fd` y `393008d20`: selector combinado y control de
    sesiones activas. Sin conectar ni ejecutar; la reconciliación de una misma
    identidad entre sesión y biblioteca sigue pendiente.
  - Preparados `220abbd7c` y `2dcc699de`: combinación de campos y constantes
    desde catálogos precompilados, con controles de duplicados y conflictos.
    Sin ejecutar ni instalar en el consumidor; visibilidad sigue pendiente.
  - Preparados `bdf28f089` y `2e408143a`: reconstrucción desde catálogos de
    módulos precompilados, propietarios y control de duplicados. Sin ejecutar
    ni conectar a la resolución de nombres o miembros del consumidor.
  - Preparados `4d0d4b293` y `7b716d446`: transporte del catálogo al módulo
    precompilado y control de preservación en poda. Sin ejecutar ni instalar
    los tipos en la resolución del consumidor.
  - Preparados `e6fc30b8a` y `d39aa7eaa`: proyección de campos y constantes
    desde capturas, con controles basados en fixtures. Sin ejecución ni
    instalación; visibilidad, colisiones y consumo de NSMETA siguen pendientes.
  - Preparados `8e4fe3bb5` y `0f84790a7`: reconstrucción inicial de
    declaraciones record y enum desde el catálogo, con control de destino y
    layout. Sin ejecutar ni instalar campos o constantes en el consumidor.
  - Preparados `a52a02cb2`, `fc3d27fe1`, `3aebb434c`, `7aac61c66` y
    `861afd467`: identidad estructurada y capacidad de layout por valor en
    captura y codec. Sin ejecutar; la reconstrucción del consumidor sigue pendiente.
  - Preparados `e66e9a8b7`, `acfae78ac` y `735b69a7e`: codec conjunto de
    records y enums, verificación de destino y controles de ida/vuelta. No se
    ha ejecutado; el proveedor todavía no incorpora enums y falta reconstrucción.
  - Preparados `af91e8174` y `bed297cca`: captura reconciliada de enums C y
    control de valores e identidades. Sin ejecución; faltan codec, integración
    al proveedor, reconstrucción y casos adicionales como declaraciones incompletas.
  - Preparados `ee5fe5111` y `7d0b9f55b`: proveedor que selecciona records C
    referidos por la API y sus dependencias. No instalado ni ejecutado; enums
    publicados y reconstrucción del consumidor siguen pendientes.
  - Preparados `5671a4dd7`, `d9af38b62` y `7bd1a92c9`: proveedor opcional
    de catálogo y conexión al productor, con controles. El driver no lo instala,
    1.42 sigue deshabilitado y la ejecución agrupada permanece pendiente.
  - Preparados `093caf820` y `6fcce1baf`: sección delimitada del catálogo C
    y contribución a la huella ABI, con controles internos. El minor 1.42 sigue
    deshabilitado; falta reconstrucción y ejecutar la validación agrupada.
  - Preparados `f0a1b43b3` y `3ff58dd98`: cierre transitivo de records C y
    enumeración de dependencias enum. Control pendiente de ejecución; falta
    conectar el cierre a la superficie pública y persistir enums y metadata.
  - Preparados `937da338d`, `424867364`, `fbfc505da` y `cbaf3ea05`:
    preservación en el modelo, fusión y poda; rechazo en versiones antiguas y
    verificación inicial del destino. Sin ejecutar; 1.42, lectura/escritura,
    huella ABI, referencias y reconstrucción permanecen pendientes.
  - Preparados `8024630e9`, `525c5e6f7`, `8beeeab07` y `16cf5df8b`:
    control de tipos anidados y preservación del modelo volatile en captura y
    codec. Falta ejecutar y conectar la reconstrucción desde metadata.
  - Preparados `fc640a3dd`, `450f82c32`, `dd083f39d` y `2fede90da`:
    validaciones iniciales del catálogo y controles de duplicados, layouts
    incompletos y campos fuera de límites. Todavía sin ejecutar; la validación
    semántica completa y su integración en NSMETA siguen pendientes.
  - Preparados `50dc5bc`, `a6ae824d7`, `e5ccee5ae` y `9967412b0`:
    codec interno y controles iniciales de ida/vuelta y truncamiento. Aún sin
    compilar ni ejecutar; faltan validación semántica, integración en NSMETA,
    huella ABI, fusión, reconstrucción y pruebas completas.
  - Preparados `aa52dce2b` y `0923dc3a0`: catálogo ordenado de capturas C,
    con rechazo de duplicados y grupos inválidos. Aún sin ejecución agrupada,
    selección integrada de grupos ni serialización en metadata.
  - Preparados `98897f1ba` y `c0ee0d29b`: reconciliación de USR de tipos
    anidados en campos capturados y control de que no se alteren las
    observaciones. Construcción y ejecución agrupadas aún pendientes.
  - Preparados `8ca16bf34` y `02934b968`: captura estructurada de records C y
    controles de struct, union, declaración incompleta y conflicto. Pendientes
    de construcción y ejecución agrupadas; no hay serialización ni reconstrucción
    de esta captura en metadata, por lo que la publicación sigue sin resolverse.
  - Preparados `6c30db324` y `3b6b56a3c`: procedencia struct/union/enum C o
    record C++ y triple consensuado en el frontend, con controles de origen y
    conflicto. Aún no se serializan ni se han construido o ejecutado.
  - La suite de sesión pasó sobre `3e6405f55`, incluida la recuperación del
    USR en el proveedor y conflictos de tipos anidados. Persistencia y cierre
    siguen pendientes. [Evidencia](TANDA-08-TIPOS-C.md#validación-dirigida-de-visibilidad-e-identidad).
  - Diagnóstico dirigido: el productor aislado llega a verificación de metadata,
    que rechaza `CRecord` como tipo local no declarado. La variante con llamada
    C falla antes por identificador desconocido. [Evidencia](TANDA-08-TIPOS-C.md#tipo-c-en-api-de-biblioteca-diagnóstico-dirigido).
  - `7b3b0f0c5` y `2a7179d0d` preparan un productor con struct C en superficie
    pública y un consumidor por LibraryReference que debe leer el campo en
    ejecución. El fixture no se ha ejecutado.
  - Preparados `7052f2176` y `c5cc29c44`: comparación de USR de tipos
    record/enum anidados entre todas las definiciones y controles con una
    observación inicial sin USR. Pendientes de compilación y ejecución.
  - `495878320` prepara la prueba integrada que recupera el USR de campo de
    una segunda sesión hasta el símbolo. Pendiente de construcción y ejecución.
  - La construcción incremental y suite de sesión pasaron con los controles
    agrupados de identidad hasta `b8bb920ef`. Falta probar la recuperación de
    USR en el proveedor completo y resolver persistencia. [Evidencia](TANDA-08-TIPOS-C.md#validación-dirigida-agrupada-identidades-y-_bool).
  - Preparados `42fbf7a22`, `0232e6924` y `303592a34`: conciliación
    transitiva del USR de campo y recuperación cuando la primera definición
    no lo contiene. Prueba de conflicto preparada; falta probar el proveedor
    completo y ejecutar la validación agrupada.
  - Preparados `7d20c1b0f` y `bcbcff4d1`: conciliación de USR de
    enumeradores también al seleccionar el tipo enum y control con tres
    observaciones. Pendientes de construcción y ejecución agrupadas.
  - Preparados `7a67b43f5` y `54012a7d3`: proyección y conciliación del USR de
    enumerador C y prueba de transporte presente/ausente. Aún sin construir ni
    ejecutar; falta comprobar persistencia. `d28c6ff68` prepara la prueba de
    selección contradictoria con una observación intermedia sin USR; pendiente
    de construcción y ejecución agrupadas.
  - Preparados `847a71ccb` y `13a872950`: la búsqueda de alias concilia USR
    conocidos, incluso con observaciones intermedias sin identidad, y añade
    pruebas de coincidencia y contradicción. Aún no construidos ni ejecutados;
    persisten las tareas de cobertura y persistencia de T08-002.
  - Preparados `9eaa91af1` y `f19f64f1d`: transporte y conciliación del USR
    de funciones agrupadas, con prueba de contradicción. Sin construir ni
    validar; persistencia y cobertura general siguen pendientes.
  - Actualización: la suite de sesión construida incrementalmente pasó con
    controles de USR de función y alias. Persistencia y T08-002 siguen abiertos.
  - Preparados `b8f581cdc`, `3125e3dda` y `a72a8157e`: selección y transporte
    del USR de global C al símbolo, con controles de conflicto y alias. Sin
    construir ni ejecutar; la persistencia continúa pendiente.
  - Actualización: la suite de sesión construida incrementalmente pasó con los
    controles de USR de global, contradicciones y alias. No acredita persistencia
    ni cierra T08-002.
  - Preparados `f2d136227` y `52a45b8b5`: selección de miembros repetidos
    rechaza USR de campo conocidos y contradictorios, con controles para USR
    iguales y ausentes. Sin construir ni ejecutar.
  - Preparados `b16bc21b2`, `3932eca4a`, `8f773abfb` y `1f869f545`: USR
    opcional de campo hasta metadato y símbolo, con controles de presencia y
    ausencia. Sin construir ni validar; selección y persistencia siguen abiertas.
  - Actualización: la construcción incremental y la suite de sesión pasaron;
    comprobaron el transporte del USR al símbolo y la selección ante USR de
    campo iguales, distintos y ausentes. Persistencia y demás categorías siguen
    pendientes; esta evidencia no cierra T08-002.
  - En `dfe590633` la sesión conserva el USR opcional de Clang para las
    declaraciones y miembros capturados; la batería agrupada pasó. Todavía falta
    integrar y verificar su uso en selección y persistencia antes de cerrar el
    ítem. [Evidencia](TANDA-08-TIPOS-C.md#identificadores-usr-de-clang).
  - En `92d212d3e` pasó la batería con USR en tipos record/enum referidos y
    detección de funciones de un mismo USR con símbolos incompatibles. La
    persistencia y otras categorías de declaraciones siguen pendientes.
    [Evidencia](TANDA-08-TIPOS-C.md#referencias-y-conflictos-de-usr).
  - Validación dirigida en `248a77b0e`: la suite de sesión Clang pasó con
    controles de USR para enums, records, aliases y dependencias de campos.
    Falta la batería completa de la tanda y persisten contratos sin cubrir.
    [Evidencia](TANDA-08-TIPOS-C.md#prueba-dirigida-de-identidades-en-tipos).
  - En `52d502125` pasó la suite dirigida con selección de globales por USR,
    aliases transitivos y aislamiento entre proyectos. No acredita persistencia
    ni la validación completa de la tanda.
  - La suite de sesión comparó tres USR de campos obtenidos de Clang con los
    reconstruidos desde el catálogo C codificado y decodificado; coincidieron.
    Esto no verifica por sí solo la escritura y lectura de un NSMETA real ni
    cubre funciones, globales o aliases; T08-002 sigue abierto.
    [Evidencia](registro-commits/t08-catalogo-usr-campos-sesion.md).
    Además, un NSMETA producido por el fixture contiene los bytes del USR de
    `CRecord.value`; la búsqueda no demuestra que el consumidor restaure ese
    dato concreto. [Observación](registro-commits/t08-usr-campo-nsmeta-observado.md).
    La suite después escribió y autenticó un NSMETA propio con el catálogo de
    la sesión Clang y comprobó los mismos USR en los campos reconstruidos.
    No demuestra todavía el ciclo completo entre productor y consumidor
    independientes ni la identidad de otras declaraciones. [Prueba](registro-commits/t08-usr-campos-nsmeta-ruta.md).
- [x] T08-003. Importar declaraciones de funciones C desde el AST de Clang.
  - La opción ThinLTO con funciones C `static inline` pasó en O0–O3 tanto
    directamente como con dos productores precompilados. En O0 NSC usa código
    normal; en O1–O3 ThinLTO consume bitcode entre bibliotecas. Los sidecars v3
    transportan los objetos C generados y el
    consumidor usó ambos sidecars en O1–O3 sin perder el estado independiente.
    Se conservan todas las definiciones si hay objetos nativos. Faltan otras
    referencias y variantes de corrupción. [Resultado](registro-commits/t08-inline-thinlto-sidecar-v3-validacion.md).
    Un objeto inline C también llamó a un símbolo `export extern C` de
    NativeSharp en O0–O3, también desde un sidecar consumido en O1–O3. Un
    consumidor conservó su comportamiento ante uno o dos sidecars corruptos
    mediante fallback nativo. [Resultado](registro-commits/t08-inline-thinlto-v3-integracion-validacion.md).
    También se rechazó antes del backend un objeto COFF inválido con huella de
    sidecar recalculada; el consumidor conservó la biblioteca nativa y pasó.
    [Resultado](registro-commits/t08-thinlto-objeto-invalido-validacion.md).
  - Una definición C11 con parámetros de estilo antiguo recibió `byte` y
    `float` desde NativeSharp con promociones predeterminadas y pasó O0–O3.
    Clang emitió advertencias de obsolescencia. Es una muestra, no el cierre
    de todas las convenciones antiguas. [Evidencia](registro-commits/t08-definicion-c-estilo-antiguo.md).
  - Funciones C `static inline` de un encabezado se llamaron desde NSC en
    O0–O3, directamente y mediante dos bibliotecas NativeSharp precompiladas;
    en la muestra cada productor conservó su propio estado estático. La misma
    dependencia binaria declarada por ambos provocó NSL1021; tras ajustar su
    recopilación, el caso pasó en O0–O3. ThinLTO y otras combinaciones siguen
    pendientes. La muestra no cierra el
    ítem. [Evidencia](registro-commits/prueba-inline-c.md).
  - La sesión selecciona un prototipo C disponible entre redeclaraciones
    compatibles. El fixture pasó llamadas válidas O0–O3 y rechazó aridad
    incorrecta tanto si el prototipo aparece antes como después de `f()`.
    [Evidencia](registro-commits/prueba-redeclaraciones-c.md).
  - Declaraciones C11 sin prototipo `f()` admiten argumentos con promociones
    en la muestra Windows x64; `f(void)` rechaza argumentos adicionales. El
    fixture pasó O0–O3 y la sesión Clang pasó. Faltan otras convenciones,
    definiciones antiguas y conciliación general; no cierra el ítem.
    [Evidencia](registro-commits/prueba-prototipos-c.md).
  - La matriz de cinco llamadas pasó tras `20b3a49c9`; tres controles de
    sombras pasaron en O0. [Evidencia](TANDA-08-TIPOS-C.md#validación-dirigida-de-visibilidad-e-identidad).
  - Preparados `20b3a49c9` y `beb379767`: búsqueda de funciones C/C++
    globales como último recurso dentro de namespaces y controles de prioridad
    y namespace anidado. Pendientes de construcción y ejecución agrupadas.
  - Matriz `96de93e42`: llamadas escalares y con puntero pasan en ámbito
    global y fallan dentro de namespace por nombre desconocido. Es necesario
    resolver visibilidad sin romper sombras. [Evidencia](TANDA-08-TIPOS-C.md#tipo-c-en-api-de-biblioteca-diagnóstico-dirigido).
- [x] T08-004. Importar declaraciones de variables globales C.
  - **Precisión sobre validadores:** la ejecución directa desde la herramienta
    informó salida cero en varios fallos, pero al invocar los scripts
    anteriores con CALL, como hace el gate, tres muestras propagaron el error.
    No se demostró un falso éxito del gate. Se conservaron los ajustes que
    hacen explícitas las salidas y sus pruebas. La ejecución directa debe
    comprobar marcadores o usar un llamador que capture el error.
    [Corrección](registro-commits/t08-validadores-precision-call.md).
  - Globales `static` de encabezado: la ruta anterior excluía nombres sin enlace
    externo. El control C pasó y NSC reprodujo el rechazo.
    [Análisis](T08-004-GLOBAL-STATIC-INVESTIGACION.md).
    Corrección dirigida: una global C `static int` comparte estado con funciones
    inline de la misma sesión y también funciona sin ellas en O0–O3. Dos
    productores precompilados y el consumidor directo conservaron instancias
    independientes en O0–O3; dos referencias con el mismo nombre se rechazaron
    como ambiguas. Las otras formas siguen pendientes.
    [Resultado](registro-commits/t08-global-static-validacion.md).
    [Identidad](registro-commits/t08-global-static-identidad-validacion.md).
    Una matriz de enteros, flotantes y puntero estáticos sin calificadores pasó
    O0–O3 con inicializadores y funciones inline; la regresión de identidad
    pasó. [Resultado](registro-commits/t08-global-static-matriz-validacion.md).
    Un record, un arreglo de dos enteros y escalares const/volatile estáticos
    pasaron el control mixto O0–O3; se rechazó escritura const y se comparó el
    IR volatile con Clang. [Resultado](registro-commits/t08-global-static-agregados-validacion.md).
    Una global C `static _Thread_local int` compartió estado con sus funciones
    inline y conservó aislamiento de otro hilo en O0–O3.
    [Resultado](registro-commits/t08-global-static-tls-validacion.md).
    Una matriz de tipos TLS internos pasó en O0–O3 y verificó IR atómico;
    no cubre aislamiento por hilo para cada tipo.
    [Resultado](registro-commits/t08-global-static-tls-tipos-validacion.md).
    Una matriz adicional pasó O0–O3 para escalares, enum, atomic int y arreglos
    de varias dimensiones; comprobó IR atómico y regresiones dirigidas.
    [Resultado](registro-commits/t08-global-static-tipos-validacion.md).
    La selección ya detecta dos orígenes internos aunque uno tenga un tipo
    aún no proyectado. [Resultado](registro-commits/t08-global-static-ambiguedad-mixta-validacion.md).
    Const unsigned int, volatile short y punteros internos a const, const y a
    volatile int pasaron O0–O3, controles negativos e IR frente a Clang.
    [Resultado](registro-commits/t08-global-static-qualified-validacion.md).
    Puntero doble, union y arreglos internos de record, enum, const y volatile
    pasaron O0–O3 con comprobación del rechazo const e IR volatile.
    [Resultado](registro-commits/t08-global-static-complejas-validacion.md).
    Punteros internos a record, enum y función pasaron O0–O3; el callback
    conserva una función interna como inicializador.
    [Resultado](registro-commits/t08-global-static-punteros-validacion.md).
    La selección por contrato y el origen TLS interno se comprobaron con
    referencia estática y DLL en O0–O3; se contrastó el IR con Clang y se
    ejecutaron regresiones dirigidas. No se cierra el ítem ni la tanda.
    [Resultado](registro-commits/t08-global-static-contratos-validacion.md).
    Lecturas y escrituras de punteros atómicos internos a struct C y a _Bool
    pasaron O0–O3 y controles IR, con referencias estática y DLL; también
    pasaron regresiones de punteros atómicos anteriores. No acredita otras
    formas ni operaciones compuestas para los nuevos apuntados.
    [Resultado](registro-commits/t08-atomic-punteros-generales-validacion.md).
    La suma y resta compuestas de punteros atómicos a struct y bool también
    pasaron O0–O3; el IR verifica los tamaños de elemento en ambas direcciones.
    [Resultado](registro-commits/t08-atomic-punteros-generales-compuestos-validacion.md).
    Punteros atómicos internos a enum, void y const struct pasaron O0–O3;
    el enum se selecciona también cuando aparece dentro de Atomic. El IR
    comprueba cada variable y se rechaza una escritura al struct const.
    [Resultado](registro-commits/t08-atomic-punteros-tipos-calificados-validacion.md).
    Una biblioteca NativeSharp precompilada usó esas globales internas; su
    consumidor ejecutó O0–O3 con el encabezado C del productor oculto. El
    record también se seleccionó sin invocar funciones que lo retornan.
    [Resultado](registro-commits/t08-atomic-contratos-precompilado-validacion.md).
    La misma biblioteca se enlazó en una DLL NativeSharp y su consumidor pasó
    O0–O3 con el header oculto y un índice TLS no nulo. El validador detectó
    un fallo inducido con salida distinta de cero.
    [Resultado](registro-commits/t08-atomic-contratos-precompilado-dll-validacion.md).
  - El global C `_Atomic(int)` sin TLS fue rechazado como tipo no mapeado.
    Una corrección limitada de su proyección pasó O0–O3, comprobación de IR
    atómico y regresión TLS. [Resultado](registro-commits/t08-atomic-global-int-validacion.md).
    Un hilo C y el hilo NativeSharp incrementaron el mismo global con el
    resultado esperado O0–O3. [Detalle](registro-commits/t08-atomic-global-hilos-validacion.md).
    El control ahora sincroniza ambos hilos durante 100 rondas y verifica el
    IR atómico; no demuestra ejecución simultánea de instrucciones.
    [Resultado](registro-commits/t08-atomic-global-hilos-sincronizados.md).
    Un productor precompilado y su consumidor, directo o mediante DLL, pasaron
    O0–O3 con el header oculto. [Detalle](registro-commits/t08-atomic-global-precompilado-validacion.md).
    El acceso directo al global atómico exportado por DLL C pasó O0–O3 con
    control C e IR atómico. [Detalle](registro-commits/t08-atomic-global-dll-c-validacion.md).
    Otros tipos atómicos no TLS y la validación integral siguen pendientes.
  - La matriz de enteros, flotantes, bool y puntero atómicos no TLS pasó
    lecturas y escrituras O0–O3; se comprobó IR y regresión TLS. Faltan otras
    variantes y operaciones. [Resultado](registro-commits/t08-atomic-global-matriz-validacion.md).
    Las operaciones compuestas de la misma matriz pasaron O0–O3 con IR
    atómico y observación C. [Resultado](registro-commits/t08-atomic-global-compuestos-validacion.md).
    `long` y `unsigned long` C de 32 bits pasaron en globales no TLS O0–O3,
    con lectura, escritura y operaciones compuestas. [Resultado](registro-commits/t08-atomic-global-clong-validacion.md).
    Sus variantes TLS pasaron O0–O3 y controles de IR. [Resultado](registro-commits/t08-atomic-long-tls-validacion.md).
  - Enteros estrechos y punteros atómicos globales adicionales pasaron O0–O3,
    IR y rechazos de pérdida de calificadores; la regresión TLS pasó.
    [Resultado](registro-commits/t08-atomic-global-extra-validacion.md).
    Los apuntados calificados adicionales reconocidos en TLS pasaron O0–O3,
    IR y rechazos dirigidos también como globales no TLS; regresión TLS pasó.
    [Resultado](registro-commits/t08-atomic-global-calificados-validacion.md).
    Punteros atómicos globales a float, long y unsigned long C pasaron O0–O3,
    IR y regresión TLS. [Resultado](registro-commits/t08-atomic-global-mas-punteros-validacion.md).
    Sus variantes TLS sin apuntado calificado pasaron O0–O3 e IR; un segundo
    hilo comprobó aislamiento. Las variantes calificadas pasaron O0–O3, IR y
    rechazos dirigidos. [Resultado](registro-commits/t08-atomic-tls-calificados-validacion.md).
    Sus apuntados const y volatile pasaron O0–O3, controles de IR y rechazos
    dirigidos. [Resultado](registro-commits/t08-atomic-global-mas-calificados-validacion.md).
    Punteros atómicos a `signed char`, `unsigned char`, `unsigned short` y
    `unsigned long long` pasaron controles globales y TLS en O0–O3 e IR; pasó
    la regresión TLS atómica. No se acreditaron apuntados calificados.
    [Resultado](registro-commits/t08-atomic-pointer-widths-validacion.md).
    Un segundo hilo C comprobó aislamiento de los cuatro punteros TLS en O0–O3.
    [Resultado](registro-commits/t08-atomic-pointer-widths-hilos-validacion.md).
  - Reproducido rechazo de `|=` en `_Bool` C escalar volatile; Clang acepta el
    contrato y emite accesos de byte ordenados. Corrección pendiente.
    [Evidencia](registro-commits/t08-bool-scalar-compuesto-reproduccion.md).
    Preparada la ruta escalar en análisis, HIR y LLVM; pendientes controles y
    validación. [Detalle](registro-commits/t08-bool-scalar-compuesto-implementacion.md).
    Preparada la matriz escalar O0–O3 y su verificación IR, sin ejecutar.
    [Detalle](registro-commits/t08-bool-scalar-compuesto-fixture.md).
    Validación posterior: construcción incremental y matrices de escalar y
    arreglo pasaron O0–O3 con IR comprobado; la regresión atómica pasó.
    [Resultado](registro-commits/t08-bool-scalar-compuesto-validacion.md).
  - Reproducida discrepancia para global escalar C `volatile _Bool`: NSC usa
    i1 volatile y Clang i8 acquire. Corrección pendiente.
    [Evidencia](registro-commits/t08-bool-scalar-reproduccion.md).
    Preparada la declaración i8 y los accesos acquire/release; pendientes
    construcción y validación. [Detalle](registro-commits/t08-bool-scalar-implementacion.md).
    Preparado control mixto C/NativeSharp y comprobación IR; sin ejecutar.
    [Detalle](registro-commits/t08-bool-scalar-fixture.md).
    Validación posterior: construcción incremental, control mixto O0–O3,
    IR de byte acquire/release y regresiones TLS y atómica aprobados. Falta
    cobertura de otras formas y la validación integral posterior.
    [Resultado](registro-commits/t08-bool-scalar-validacion.md).
    Preparado control específico de identidad MIR para el indicador de byte;
    aún sin ejecutar. [Detalle](registro-commits/t08-bool-scalar-cache-fixture.md).
    Preparado productor precompilado y consumidor sin header para el global C
    bool volatile, pendiente de ejecución. [Detalle](registro-commits/t08-bool-scalar-precompilado-fixture.md).
    Validación posterior: consumidor sin header pasó O0–O3 con acceso cruzado
    C/NativeSharp; el control aislado de identidad pasó. El caso DLL y la caché
    de artefactos activa siguen pendientes. [Resultado](registro-commits/t08-bool-scalar-precompilado-validacion.md).
    La variante con DLL NativeSharp intermedia pasó O0–O3 con el header oculto.
    [Resultado](registro-commits/t08-bool-scalar-precompilado-dll-validacion.md).
    El control independiente de global C bool exportado por DLL pasó O0–O3;
    incorporado a la validación agrupada. [Resultado](registro-commits/t08-bool-escalar-dll-c-validacion.md).
    El productor con referencia C rechaza la caché por encabezados transitivos;
    el consumidor precompilado sin referencia C directa pasó con caché y header
    oculto en O0–O3. [Resultado](registro-commits/t08-bool-cache-validacion.md).
  - Por autorización del usuario, el gate usa el control C PE y el acceso NSC
    directo existentes, y variantes de escalares con importación y con carga
    dinámica. Los cuatro controles originales siguen disponibles como
    diagnósticos separados; la configuración sin importaciones y el acceso C
    directo a TLS de otra DLL no quedan acreditados. La validación integral
    ajustada terminó con código 0; no cierra los ítems pendientes de la tanda.
    [Detalle](registro-commits/t08-controles-tls-ajuste-autorizado.md).
  - Punteros TLS atómicos a `short`, `long long` y `double` pasaron acceso y
    aritmética O0–O3; se verificaron desplazamientos y atomicidad en IR.
    [Evidencia](registro-commits/t08-tls-atomico-punteros-tamanos.md).
    También pasaron sus apuntados `const` y `volatile`, con orden y alineación
    contrastados con Clang y rechazo de pérdida de calificadores.
    [Evidencia](registro-commits/t08-tls-atomico-punteros-tamanos-calificados.md).
    La suite de sesión se actualizó tras el cambio del clasificador: construyó
    y pasó con controles de las tres alineaciones.
    [Evidencia](registro-commits/t08-tls-clasificador-suite.md).
  - El mismo objeto C TLS se contrastó en DLL sin arranque y con arranque CRT:
    el instrumento C observó índice cero y datos distintos de la plantilla en
    la primera, e índice uno y datos coincidentes en la segunda. Esto delimita
    el fallo observado, sin resolver los controles originales.
    [Evidencia](registro-commits/t08-tls-dll-contraste-indice-crt.md).
    Un segundo contraste conservó `/noentry` y el mismo objeto C: forzar una
    importación produjo índice uno y datos coincidentes con la plantilla.
    [Evidencia](registro-commits/t08-tls-dll-contraste-importacion.md).
    Un contraste adicional aisló la presencia del directorio de importación:
    una tabla vacía bastó para observar índice asignado y lectura cero en cinco
    procesos; sin directorio, los cinco conservaron el centinela. Esto no
    corrige las DLL ni autoriza sustituir sus controles.
    [Evidencia](registro-commits/t08-tls-directorio-importacion-vacio.md).
  - `_Thread_local _Atomic(unsigned int*)` pasó acceso y aritmética O0–O3
    frente al control C de Clang, con operaciones atómicas comprobadas en IR.
    [Evidencia](registro-commits/t08-tls-atomico-puntero-uint.md).
    Los apuntados `const` y `volatile` también pasaron controles directos e
    indexados, rechazos de pérdida de calificador y orden acquire/release en IR.
    [Evidencia](registro-commits/t08-tls-atomico-puntero-uint-calificado.md).
  - Una biblioteca NativeSharp precompilada accedió a TLS C atómico `int` y
    punteros atómicos a `const int` y `volatile int`; su consumidor independiente
    pasó O0–O3 con el header original oculto. [Evidencia](registro-commits/t08-tls-atomico-precompilado.md).
    También pasó la cadena con DLL NativeSharp intermedia y directorio TLS PE,
    con dependencias CRT declaradas. [Evidencia](registro-commits/t08-tls-atomico-precompilado-dll.md).
    Un segundo hilo creado por C observó su valor inicial independiente y lo
    cambió sin alterar el valor visto por el hilo NativeSharp, en ambas rutas
    y O0–O3. [Evidencia](registro-commits/t08-tls-atomico-precompilado-hilos.md).
    El segundo hilo también llamó a NativeSharp y comprobó desde ambos lenguajes
    su propia instancia atómica, sin alterar la del hilo principal.
    [Evidencia](registro-commits/t08-tls-atomico-precompilado-callback-hilos.md).
  - Se añadieron controles de punteros TLS atómicos a `const int` y a
    `volatile int`: lectura y escritura aplicable, conservación del calificador
    y comparación con C; NSC pasó O0–O3. La validación integral y las demás
    variantes de TLS siguen pendientes. [Const](registro-commits/t08-tls-atomico-puntero-const-pointee.md),
    [volatile](registro-commits/t08-tls-atomico-puntero-volatile-pointee.md).
  - El TLS C `_Atomic(int)` también pasó asignaciones bit a bit, multiplicación,
    desplazamientos, división y residuo en los controles preparados O0–O3;
    se comprobó `cmpxchg` y una prueba de concurrencia, además de divisor cero
    con y sin `volatile`. El alcance sigue limitado y no cierra el ítem.
    [Operaciones](registro-commits/t08-tls-atomico-bitwise.md),
    [multiplicación](registro-commits/t08-tls-atomico-multiplicacion.md),
    [desplazamientos](registro-commits/t08-tls-atomico-desplazamientos.md),
    [división](registro-commits/t08-tls-atomico-division-validacion.md),
    [divisor cero](registro-commits/t08-tls-atomico-divisor-cero.md).
  - Un global C `_Thread_local _Atomic(int)` pasó lectura, escritura, suma,
    resta y pre/post incremento en O0–O3, con IR `seq_cst` y contención entre
    hilos; también pasaron los casos `const` y `volatile` preparados. Faltan
    otros tipos, operaciones, escapes y la validación integral de la tanda.
    [Evidencia](registro-commits/t08-tls-atomico-int-resultado.md).
    [Trabajo pendiente](T08-TLS-ATOMICOS.md).
  - Captura diferenciada de `_Atomic(T)` desde Clang y controles para el tipo
    y el rechazo del codec anterior; la proyección está limitada al global TLS
    `int` anterior. El resto de la semántica sigue pendiente.
  - La construcción agrupada posterior y la suite de sesión de Clang pasaron;
    el control focalizado de caché MIR también pasó. La ejecución completa de
    `nsc_project_oop_lowering_tests` reportó 25 fallos de 652 comprobaciones;
    están pendientes de diagnóstico y no se atribuyen aún a estos cambios.
    [Resultado](registro-commits/t08-tls-atomico-construccion-agrupada.md).
  - Un global C TLS estático `int` firmado de 32 bits se leyó y escribió desde
    NativeSharp y C con aislamiento de otro hilo en O0–O3. Se usaron LLD y CRT
    declarados explícitamente; el acceso directo desde DLL sigue pendiente.
    [Evidencia](registro-commits/global-c-tls-estatico.md).
  - Un control C y NS ejercitaron `unsigned int`, `short`, `double`, `_Bool`,
    `const int` y `volatile int` TLS estáticos con dos hilos en O0–O3. Se
    verificaron `volatile` en IR y el rechazo de escritura `const`. La colisión
    `_fltused` con el CRT se corrigió con enlace débil en el runtime; una
    regresión con NSLink pasó. [Evidencia](registro-commits/tanda08-tls-escalares-evidencia.md).
  - NSC emitió y ejecutó O0–O3 una biblioteca C con TLS estático usando LLD
    seleccionado explícitamente y `libcmt.lib` declarada; se verificó el
    directorio TLS del PE en O0. `nsc build` con LLD explícito y CRT declarado
    pasó O0–O3 sin caché; se comprobó un cambio de header transitivo. El enlace
    automático y TLS directo siguen pendientes. [Evidencia](registro-commits/tls-build-nsc-lld.md).
    El fixture original `pruebas-tanda-08-tls-c/validar.cmd` también pasó
    O0–O3 con NSC `b151b46f0be04227` y LLD seleccionado mediante `NSC_LINKER`;
    esa selección aún no es automática. [Evidencia](registro-commits/tls-estatico-control-original.md).
    `nsc build --linker LLD` sin `NSC_LINKER` pasó en un proyecto TLS, una
    biblioteca estática y una DLL consumida desde NativeSharp; también pasó
    un acierto del enlace incremental. El proyecto TLS pasó O0–O3 sin esa
    variable; el cambio de NSLink a LLD invalidó la caché y la ausencia de LLD
    produjo error sin sustituir el ejecutable previo. La detección automática
    completa de TLS sigue pendiente. [Evidencia](registro-commits/lld-sin-entorno-prueba.md).
    Para `nsc build` sin selección explícita ni sidecar, un `_tls_index`
    indefinido en una biblioteca estática selecciona LLD de forma conservadora.
    Pasaron TLS interno O0–O3, un consumidor precompilado y controles de
    precedencia; miembros no extraídos pueden producir selección adicional y
    una lectura negativa no certifica ausencia de TLS. [Evidencia](registro-commits/tls-seleccion-automatica-matriz.md).
  - Actualización posterior: `nsc build` selecciona LLD para bitcode ThinLTO y
    LTO completo, con TLS y sin TLS; la ausencia de `build.linker` en un
    sidecar parcial permite esa selección, y las selecciones explícitas se
    conservan. Las pruebas dirigidas ejecutaron los binarios en A:. Un sidecar
    que contiene `linker: NSLink`, incluso si fue generado por defecto, cuenta
    como selección explícita. La última batería completa terminó con cuatro
    fallos de TLS de DLL y es anterior al ajuste del sidecar parcial; la tanda
    permanece abierta. [Bitcode](registro-commits/t08-seleccion-bitcode-sin-tls.md),
    [sidecar](registro-commits/t08-sidecar-linker-implicito.md),
    [fallos](registro-commits/t08-validacion-20261106-bitcode.md).
    Después se corrigió la serialización: los proyectos nuevos y los sidecars
    sin elección conservan la selección automática al agregar o quitar paquetes;
    los valores explícitos se preservan. El proyecto TLS nuevo pasó la prueba
    dirigida. Los sidecars antiguos con `NSLink` no se migran automáticamente.
    [Evidencia](registro-commits/t08-sidecar-linker-serializacion.md).
    La validación agrupada posterior ejecutó también la prueba de proyecto
    nuevo y confirmó que pasa; el resultado global sigue fallando en cuatro
    controles TLS de DLL. [Resultado](registro-commits/t08-validacion-proyecto-nuevo.md).
    La emisión directa de ejecutables también selecciona LLD para referencias
    estáticas con TLS o bitcode cuando no hay elección explícita. Pasaron los
    controles dirigidos O0–O3, prioridades y caché; la DLL directa y la batería
    posterior permanecen pendientes. [Evidencia](registro-commits/t08-emision-directa-auto-linker.md).
    La emisión directa de DLL se corrigió después: la selección automática
    conserva el directorio TLS y el productor pasó O0–O3 con consumidores
    Python y NativeSharp. Las opciones explícitas se respetan; falta comprobar
    la caché de bundle de DLL y los cuatro fallos C originales siguen abiertos.
    [Evidencia](registro-commits/t08-emision-dll-auto-tls.md).
    El selector directo también inspecciona bibliotecas proporcionadas con
    `--link-lib`: pasaron ejecutable y DLL O0–O3, consumidor NS O0, bitcode
    sin TLS y controles de selección. [Evidencia](registro-commits/t08-link-lib-auto-tls.md).
    También se inspeccionan dependencias estáticas transitivas de bibliotecas
    NativeSharp precompiladas: la emisión directa pasó O0–O3 y `nsc build`
    seleccionó LLD y ejecutó el consumidor. El TLS de DLL sigue abierto.
    La DLL NativeSharp con la misma dependencia transitiva pasó también O0–O3,
    con directorio TLS y consumidor; `--linker NSLink` se respetó.
    Pasaron además el productor precompilado sin TLS con selección NSLink, las
    selecciones directas y por entorno, y la dependencia ausente diagnosticada.
    Al reconstruir el productor cambiando la dependencia de COFF sin TLS a
    COFF con TLS, el mismo consumidor pasó a LLD y ejecutó dos construcciones;
    la segunda informó un acierto de caché de objetos y conservó el directorio TLS.
    Una cadena de dos bibliotecas NS precompiladas también detectó el TLS en
    su dependencia C: pasaron emisión directa y `nsc build`; una cadena sin
    TLS conservó NSLink. [Evidencia](registro-commits/t08-tls-cadena-estatica-resultado.md).
    La misma selección pasó cuando las dependencias de la biblioteca intermedia
    se resuelven desde `LibraryReferenceRoot`, en `nsc build` y emisión directa.
    [Evidencia](registro-commits/t08-tls-raiz-biblioteca-resultado.md).
    `NS_LIB_ROOT` se propaga a la clausura de `nsc build`; pasaron controles de
    respaldo, selección con y sin TLS y prioridad de raíz explícita.
    [Evidencia](registro-commits/t08-tls-raiz-respaldo-resultado.md).
    Un productor NativeSharp precompilado también accedió al TLS de una DLL C
    con importaciones reales; su consumidor sin header pasó O0–O3 y aislamiento.
    La repetición ocultó la copia temporal del header durante cada compilación
    del consumidor y la restauró después; también pasó O0–O3.
    La cadena DLL C → biblioteca NS precompilada → DLL NS → ejecutable NS
    también pasó O0–O3 sin header durante la compilación de los consumidores.
    [Evidencia](registro-commits/t08-tls-dll-cadena-resultado.md).
    [Evidencia](registro-commits/t08-tls-dll-precompilado-resultado.md).
    [Evidencia](registro-commits/t08-tls-transitivo-gate.md),
    [DLL](registro-commits/t08-tls-transitivo-dll-resultado.md).
    La batería confirmó el control nuevo y terminó con los cuatro fallos
    originales de DLL C. [Resultado](registro-commits/t08-validacion-link-lib-auto.md).
    La batería posterior confirmó el control nuevo, pero volvió a fallar en los
    cuatro controles originales de DLL C. [Resultado](registro-commits/t08-validacion-dll-emision-auto.md).
    La batería posterior ejecutó correctamente ese control, pero terminó con
    los cuatro fallos conocidos de DLL. La tanda sigue abierta.
    [Resultado](registro-commits/t08-validacion-emision-auto.md).
    La DLL no solicita un accessor inexistente para el global C importado.
    [Evidencia](registro-commits/tls-dll-exportacion-externa.md).
    Su productor y consumidor NativeSharp pasaron O0–O3 con LLD sin
    `NSC_LINKER`. [Evidencia](registro-commits/tls-dll-consumidor-optimizaciones.md).
    La declaración estándar C11 `_Thread_local` pasó el control C y acceso
    NativeSharp de dos hilos en O0–O3. [Evidencia](registro-commits/tls-estatico-c11.md).
    Un productor NativeSharp precompilado y dos consumidores, con y sin
    encabezado C, compartieron el TLS y comprobaron aislamiento en O0–O3.
    [Evidencia](registro-commits/tls-precompilado-preparacion.md).
  - Clang rechaza `thread` con `dllimport`/`dllexport`; al exportar por el
    enlazador, el consumidor C pide `SECREL tls_exported` y la import library
    solo ofrece `__imp_tls_exported`. La declaración actual sin atributo
    `dllimport` se trató como TLS estático y el enlace NSLink falló por
    `_tls_index`. NSC ahora distingue referencias con runtime y generó acceso
    directo de lectura y escritura al TLS de la DLL en O0–O3; conserva una
    barrera para orígenes ambiguos. Faltan otros casos y validación integral.
    [Evidencia C](registro-commits/tls-c-dll-directo.md),
    [condición en Clang 23](registro-commits/t08-clang-tls-dll-c-oraculo.md),
    [observación NSC](registro-commits/tls-dll-observacion-nsc.md),
    [barrera previa](registro-commits/tls-dll-guardia-evidencia.md),
    [acceso generado](registro-commits/tls-dll-acceso-generado-evidencia.md).
    Un callback NativeSharp leyó y escribió el mismo TLS en un segundo hilo;
    el valor del hilo principal permaneció independiente en O0–O3.
    [Evidencia](registro-commits/tls-dll-nativesharp-dos-hilos.md).
    Un `int*` TLS mutable pasó asignación, desreferencia y aislamiento entre
    hilos con biblioteca estática y DLL en O0–O3; arreglos y punteros con
    calificadores y tipos adicionales se evalúan por separado. [Evidencia](registro-commits/tls-puntero-evidencia.md).
    `double*`, `const int*`, `volatile int*`, `int* volatile` e `int* const`
    TLS pasaron O0–O3 en estático y DLL, con controles de calificadores.
    [Evidencia](registro-commits/tls-punteros-calificadores-evidencia.md).
    Los globales TLS `int**` y `void*` pasaron conversiones, lectura y
    aislamiento en estático y DLL O0–O3. [Evidencia](registro-commits/tls-punteros-anidados-evidencia.md).
    Un struct TLS, su puntero y arreglo, un enum TLS y su puntero pasaron en
    biblioteca estática y DLL O0–O3, con aislamiento de otro hilo.
    [Evidencia](registro-commits/tls-records-enums-evidencia.md).
    Struct, arreglo de struct y enum TLS `volatile`, además de struct
    `const volatile`, pasaron O0–O3 en estático y DLL; se comprobó el IR y el
    rechazo de escritura `const`. [Evidencia](registro-commits/tls-record-enum-volatile-evidencia.md).
    Un callback C almacenado en TLS se asignó e invocó desde NativeSharp y C,
    con aislamiento entre hilos, en estático y DLL O0–O3.
    [Evidencia](registro-commits/tls-callback-evidencia.md).
    Arreglos TLS de `int*` y de callbacks `int(int)` pasaron asignación,
    lectura, invocación y aislamiento en estático y DLL O0–O3.
    [Evidencia](registro-commits/tls-arreglos-punteros-callbacks-evidencia.md).
    Los escalares TLS `volatile unsigned int`, `long long`, `double`, `_Bool`
    y `const volatile int` pasaron en estático y DLL O0–O3; el IR estático y
    el rechazo de escritura `const volatile` se verificaron.
    [Evidencia](registro-commits/tls-escalares-volatile-evidencia.md).
    Arreglos TLS `int[2]`, `int[3]` e `int[2][3]` pasaron lectura, escritura
    y aislamiento con biblioteca estática y DLL en O0–O3; otros elementos y
    calificadores siguen pendientes. [Evidencia](registro-commits/tls-arreglo-evidencia.md).
    El mismo conjunto pasó con arreglos TLS de `unsigned int`, `double`,
    `_Bool`, `const int` y `volatile int`; se comprobó el rechazo de escritura
    `const` y el IR `volatile`. [Evidencia](registro-commits/tls-arreglo-escalares-evidencia.md).
    Arreglos TLS `volatile` de `unsigned int`, `long long`, `double` y `_Bool`
    pasaron en estático y DLL O0–O3, con comprobación del IR por tipo.
    [Evidencia](registro-commits/tls-arreglos-volatile-evidencia.md).
    Una DLL de escalares sin importaciones falla ya en el control C de TLS;
    una variante con importación Win32 pasó en NSC O0–O3. Ambas permanecen en
    el gate mientras se investiga la diferencia. [Evidencia](registro-commits/tls-dll-escalares-cero-investigacion.md).
    Una comparación posterior aisló la entrada PE del directorio de importación:
    con la DLL original el consumidor NS O0 devolvió 8; una copia que solo
    cambia esa entrada para apuntar a un descriptor vacío devolvió 0. Es un
    diagnóstico del entorno, no una corrección ni autorización para modificar
    bibliotecas ajenas. [Evidencia](registro-commits/t08-tls-dll-directorio-importacion.md).
    Un consumidor C con arranque estándar del CRT reprodujo el mismo contraste
    (8 con la DLL original y 0 con la copia). El punto de entrada se contrastó
    con el mapa del enlazador. [Evidencia](registro-commits/t08-tls-directorio-control-crt.md).
    Con el arranque normal del CRT estático, el control C y NativeSharp pasaron
    esos escalares O0–O3; las variantes de arranque mínimo fallidas se conservan.
    [Evidencia](registro-commits/tls-dll-escalar-crt.md).
    Se corrigió además el límite del acceso NSC para incluir `SizeOfZeroFill`;
    el control C y NS O0–O3 pasaron con una DLL de contraste con relleno cero.
    [Evidencia](registro-commits/tls-dll-relleno-cero-correccion.md).
  - Un experimento C separado obtiene la DLL desde la dirección importada del
    dato y usa su directorio PE TLS y los slots del hilo para leer y escribir
    el valor en dos hilos. Depende de detalles internos de Windows x64; no es
    todavía implementación NSC ni corrige la prueba original.
    [Evidencia](registro-commits/tls-dll-directo-experimento-pe.md),
    [dirección importada](registro-commits/tls-dll-pe-import-address.md).
  - NativeSharp también leyó y escribió ese TLS mediante un adaptador C de
    prueba en O0–O3, con NSLink sin cambios. Todavía debe generarse el acceso
    desde NSC, sin la llamada manual. [Evidencia](registro-commits/tls-dll-bridge-native.md).
  - Una DLL C con directorio TLS real aisló el estado de dos hilos al ser
    llamada desde NS O0–O3. El acceso directo al global y el TLS estático en
    NSLink siguen pendientes. [Evidencia](registro-commits/tls-c-dll.md).
  - El acceso desde namespace y dos controles de sombras pasaron en O0 tras
    `1753aa723`. Cobertura general y TLS siguen pendientes. [Evidencia](TANDA-08-TIPOS-C.md#validación-dirigida-de-visibilidad-e-identidad).
  - Preparados `1753aa723` y `0cb1b4300`: búsqueda de valores C globales
    desde namespaces como último recurso y controles de acceso y sombras;
    pendientes de construcción y ejecución agrupadas.
- [x] T08-005. Importar alias de tipos declarados con `typedef`.
  - El puntero `const CMatrix*` y su variante mutable llegaron a funciones C y
    ejecutaron O0–O3; se rechazaron `const` y dimensiones incompatibles.
    La suite de sesión pasó. Otras clases de matrices siguen pendientes.
    [Evidencia](registro-commits/proyeccion-puntero-matriz-c.md).
  - Un proyecto real pasó de O0 a O3 con alias escalares encadenados, matriz
    local, enum, struct y callback. Una función `const CMatrix*` sigue sin
    importarse y tiene prueba separada. [Evidencia](registro-commits/proyecto-typedef-c.md).
- [x] T08-006. Mapear los tipos enteros C con su tamaño y signo efectivos.
  - Preparada la proyección de campos C `volatile` de enum con identidad
    nominal preservada. Se agregaron controles directo y precompilado para
    struct, struct empaquetado y union, con comparación de IR frente a Clang.
    Construcción y validación pendientes; no se cierra el ítem.
    [Detalle](registro-commits/t08-campo-enum-volatile.md).
  - Actualización: el validador de campos `volatile` pasó con NSC build
    `5c1e6c7fa15d8a3e`: ejecución O0–O3 en directo, Shared y metadata, y
    comparación IR de enum alineado, empaquetado y union con Clang. Esto no
    acredita aliases de punteros ni cierra el ítem.
    [Resultado](registro-commits/t08-campo-enum-volatile-validacion.md).
  - Las funciones C libres que reciben o retornan `_Bool` llevan `zeroext` en
    declaraciones y llamadas LLVM. IR contrastado con Clang y ejecución O0–O3;
    muestra. También pasaron una función C++ libre, un método estático y otro
    de instancia. Un puntero C invocado desde NS y un callback NS invocado
    desde C pasaron O0–O3 con atributos de ABI comparados contra Clang.
    Dos DLL con el mismo símbolo también pasaron el reenvío dinámico en
    O0–O3; otras firmas y variantes siguen sin validación específica.
    [C](registro-commits/t08-bool-abi-c-implementacion.md) ·
    [C++](registro-commits/t08-bool-abi-cpp-prueba.md).
    [Callbacks](registro-commits/t08-bool-callback-c-implementacion.md).
    [Importación dinámica](registro-commits/t08-import-dinamico-declaracion.md).
  - Las globales C _Bool no volátiles escalares y de arreglo usan byte en
    declaraciones y accesos LLVM. Pasaron lectura y escritura cruzadas O0–O3,
    regresiones y controles de identidad/restauración de caché. Los punteros
    que escapan y otras formas de almacenamiento siguen pendientes.
    [Resultado](registro-commits/t08-bool-global-c-validacion.md).
  - La prueba mixta `_Bool` de campo, arreglo y llamada pasó O0–O3. Es una
    muestra, no el cierre del ítem. [Evidencia](TANDA-08-TIPOS-C.md#validación-dirigida-agrupada-identidades-y-_bool).
  - `8be3c138f` prepara un control mixto de campo y arreglo `_Bool`, con
    tamaño, alineación, offsets, lectura y escritura. No ejecutado todavía.
  - `fe2fddfca` añade argumentos y resultados `_Bool` en llamada C; pendiente
    de ejecución, no demuestra aún su ABI.
  - Control de campos volátiles C contra Clang (orden MS): la ejecución pasa O0–O3 en
    directo, `Shared` y `Metadata`, pero 28 de 34 accesos no llevan `acquire`/`release`
    (todos los escalares volátiles alineados, no sólo `_Bool`). Reproducción previa.
    [Control](registro-commits/t08-volatile-campos-c-control.md).
  - Causa: en campos, el orden MS sólo existía para punteros a función. El parche
    se integró y en esta rama 34 de 34 accesos coinciden con Clang; las regresiones
    dirigidas pasaron. Enum y record volátiles siguen sin importarse.
    [Diagnóstico](registro-commits/t08-volatile-campos-c-diagnostico.md).
    [Integración](registro-commits/t08-volatile-campos-c-integracion.md).
- [x] T08-007. Mapear los tipos de punto flotante según el objetivo.
  - `long double` IEEE binary64 pasó en uso directo y biblioteca precompilada
    O0–O3. La suite pasó controles de catálogo, versión y fingerprint. Otras
    representaciones y contratos siguen pendientes.
    [Evidencia](registro-commits/metadata-long-double-c.md).
  - Registro histórico previo a la validación: preparada una extensión versionada del catálogo para conservar el formato
    IEEE de `long double`, compararlo y reflejarlo en el fingerprint sin la
    procedencia. La construcción y los controles están en curso; no se afirma
    compatibilidad hasta ejecutarlos. [Detalle](registro-commits/catalogo-ieee64-versionado.md).
  - Registro histórico del fallo corregido: el caso directo de `long double` pasó O0 después de capturar el formato
    IEEE; el consumidor precompilado falló al reconstruir el campo y detuvo
    la matriz. [Evidencia](registro-commits/metadata-long-double-c.md).
  - Registro histórico anterior a la corrección: Clang confirmó `long double` IEEE binary64 y ABI `double` en el fixture
    Windows x64; NSC rechazó el typedef. Se preparó captura explícita y mapeo
    condicionado, con construcción en curso. La metadata aún no persiste el
    dato. [Evidencia](registro-commits/prueba-long-double-c.md).
- [x] T08-008. Mapear el tipo `void` en parámetros y resultados donde corresponda.
  - La validación agrupada del fixture de records pasó los casos preparados de
    globales y arreglos C `volatile void*`, con los controles negativos incluidos.
    [Detalle](registro-commits/validacion-agrupada-volatile-08.md).
  - La muestra C11 pasó retorno `void`, parámetro y retorno `void*` y prototipo
    de cero argumentos en O0–O3, con rechazo de argumento extra. No cubre
    todas las posiciones o combinaciones. [Evidencia](registro-commits/prueba-prototipos-c.md).
- [x] T08-009. Mapear enumeraciones y sus valores representables.
  - Enum C atómico: lectura, escritura, operaciones compuestas e incremento
    se contrastaron con Clang O0–O3 en muestras de 8, 16, 32 y 64 bits,
    incluyendo global y TLS. También pasaron casos de enlace interno,
    biblioteca NS precompilada y DLL, y un control de dos hilos. Los casos
    dirigidos no acreditan todas las combinaciones ni cierran el ítem.
    [Operaciones con signo](registro-commits/t08-enum-atomico-operaciones-signo.md),
    [tipos estrechos](registro-commits/t08-enum-atomico-estrecho.md),
    [64 bits](registro-commits/t08-enum-atomico-64.md),
    [precompilado](registro-commits/t08-enum-atomico-static-precompilado.md),
    [DLL C](registro-commits/t08-enum-atomico-dll-c.md) y
    [dos hilos](registro-commits/t08-enum-atomico-contencion.md).
  - Una comparación mixta enum C unsigned / int signed difiere entre las
    promociones NS y C; pendiente decisión sobre semántica de la expresión.
    [Evidencia](registro-commits/enum-c-mixto-referencia.md).
  - Aritmética, desplazamiento y operadores bitwise/unarios pasaron muestras
    signed, unsigned y enum de 8 bits O0–O3; AND/OR de enum NS se preservó.
    La validación de la tanda y otras combinaciones siguen pendientes.
    [Evidencia](registro-commits/proyeccion-aritmetica-enum-c.md).
  - Valores de enum C se convirtieron a int, long y otro enum C en O0–O3;
    Clang verificó las bases signed/unsigned y el IR conserva sus extensiones.
    Enum NS mantiene su rechazo. [Evidencia](registro-commits/conversion-enum-c-entero.md).
  - Los enteros se convirtieron a enum C en inicializaciones, asignaciones y
    argumentos O0–O3, incluidos truncamiento y extensión de signo; un enum NS
    conservó su rechazo. Esto resuelve la limitación de enumeradores descrita
    abajo para esas formas, sin cerrar el ítem. [Evidencia](registro-commits/conversion-entero-enum-c.md).
  - Pasó `nsc.stage51.enum-oop-support`, incluido el rechazo existente de
    comparación de enum nativo con entero no nulo. No cubre todas sus reglas.
  - La matriz de metadata C pasó O0–O3 para comparaciones enum-entero,
    incluyendo enum estrecho con signo y enum de 32 bits con base unsigned
    explícita; también pasó `nsc.cpp-interop.session`. La promoción aplica las
    reglas numéricas de NativeSharp. La cobertura de enums sigue incompleta.
    [Evidencia](registro-commits/comparacion-enum-c-numerico.md).
  - Enum compartido y enumerador resueltos en O0–O3. La comparación directa
    entre enum C y enumerador entero falla en O0; requiere promoción numérica
    en análisis y generación. [Detalle](registro-commits/prueba-promocion-enum-c.md).
  - La prueba del enum compartido detectó rechazo de locales inferidos y
    ambigüedad de enumeradores. Se prepararon correcciones; aún no se han
    validado. [Detalle](registro-commits/prueba-enum-c-compartido.md).
  - La suite dirigida pasó para identidad, tipo de enumerador y conflicto de
    destino; no constituye cierre. [Evidencia](TANDA-08-TIPOS-C.md#validación-dirigida-agrupada-identidades-y-_bool).
  - Preparados `dad00a35d` y `63cce8a8d`: detectar destinos contradictorios
    al seleccionar constantes C, con un control de igual identidad y valor.
    Pendientes de construcción y ejecución agrupadas.
  - Preparados `3d98466c5` y `d07e26b4a`: la conciliación de enums compara el
    tipo efectivo de sus enumeradores y añade un control de conflicto con el
    mismo valor. Pendientes de construcción y ejecución agrupadas.
  - Preparados `225bc09d9`, `3e031af96` y `6ec5e377e`: proyección C de
    campos y arreglos enum empaquetados, casos mixtos, direcciones e IR.
    Pendientes de construcción y validación agrupada.
  - Actualización: la construcción incremental y layout finalizaron con código
    0 en O0–O3; el fixture verificó campo y arreglo enum, IR y sus rechazos de
    dirección. Es una comprobación dirigida, no el cierre de T08-009.
- [x] T08-010. Importar la declaración y campos de un `struct` C.
  - `var` sobre miembros flexibles C ahora infiere puntero: pasaron lectura y
    escritura en directo, Shared y solo metadata O0–O3, y rechazos de escritura
    const y miembro subalineado. El IR del alias volatile conserva `volatile`,
    pero no reproduce acquire/release de Clang en modo Microsoft: sigue pendiente.
    [Evidencia](registro-commits/t08-flexible-var-validacion.md).
    Un control aislado confirmó que el modo ISO coincide con Clang y el modo
    Microsoft difiere en lectura y escritura; no está conectado como gate.
    [Comparación](registro-commits/t08-flexible-var-volatile-modos-resultado.md).
    El alias reasignado reproduce la misma diferencia en O0–O3; en ISO coincide.
    [Control](registro-commits/t08-volatile-alias-reasignado-control.md).
    Se identificaron los puntos de configuración, HIR y caché que debe cubrir
    la solución de modo del consumidor. [Ruta](registro-commits/t08-volatile-consumidor-ruta.md).
    El modelo por proyecto, cargador y consulta HIR pasaron sus controles
    internos; la generación con modo explícito sigue protegida y el acceso por
    alias continúa sin corregir. [Validación](registro-commits/t08-volatile-consumidor-modelo-validacion.md).
    Se corrigió la pérdida de configuración al reconstruir el grafo; el control
    de proyectos y la protección temporal HIR pasaron. [Resultado](registro-commits/t08-volatile-consumidor-grafo-validacion.md).
    La comparación ampliada confirmó el mismo resultado en O0–O3 para ambos
    modos. [Resultado](registro-commits/t08-flexible-var-volatile-opt-resultado.md).
    Con dos sesiones del mismo encabezado en modos distintos, NSC emite acceso
    directo y por alias sin orden atómico en O0–O3. Coincide con la referencia
    ISO y difiere de Microsoft; falta definir el modo de la unidad consumidora.
    [Resultado](registro-commits/t08-flexible-var-modos-mixtos-resultado.md).
  - La conversión de `const int[]` a `int const*` y el paso de `volatile int[]`
    a un parámetro C `volatile int*` pasaron O0–O3; perder cada calificador se
    rechazó. Son casos directos, sin acreditar otras rutas ni cerrar el ítem.
    [Evidencia](registro-commits/t08-flexible-decay-calificadores-validacion.md).
  - La conversión de miembro flexible a puntero pasó O0–O3 para `int`, `double`,
    record, enum y `_Bool` en consumo directo. El caso `int` también pasó con
    productor precompilado, con header y solo metadata; el caso subalineado se
    rechaza. Faltan calificadores y otras rutas; no cierra el ítem.
    [Evidencia](registro-commits/t08-flexible-decay-tipos-validacion.md).
  - Un struct C con union y struct anónimos pasó el caso de escritura cruzada con C
    en consumo directo, Shared y solo metadata, O0–O3. Quedan otras formas y la
    validación completa. [Evidencia](registro-commits/t08-anonimos-c-validacion-precompilado.md).
  - Pasaron anónimos de dos niveles, `pack(1)` y campos de bits en los tres modos,
    O0–O3; un proyecto con dos sesiones del mismo encabezado también pasó. Un
    control rechazó declaraciones incompatibles observadas desde el mismo archivo.
    Son casos dirigidos: siguen pendientes otras combinaciones y el gate completo.
    [Niveles](registro-commits/t08-anonimos-c-validacion-niveles.md) ·
    [bits](registro-commits/t08-anonimos-c-bitfields-validacion.md) ·
    [sesiones](registro-commits/t08-anonimos-c-sesiones-validacion.md) ·
    [conflicto](registro-commits/t08-anonimos-c-conflicto-validado.md).
  - Arreglos flexibles C de `int const` y `int volatile`: lectura, escritura
    permitida y controles negativos de const validados en consumo directo y
    precompilado O0–O3; IR volatile contrastado con Clang. Quedan pendientes
    otras combinaciones y la conversión implícita del arreglo a puntero.
    [Evidencia](registro-commits/t08-flexible-c-calificados.md).
  - Los campos de bits enteros no volátiles en structs C se proyectan con
    coordenadas de Clang. El fixture mixto de lectura y escritura, incluidos
    campos con signo y empaquetados, pasó en O0–O3. También pasó con productor
    precompilado, tanto con encabezado compartido como con tipos inferidos desde
    la metadata. Quedan pendientes unions, volatile y otras formas de uso. Es cobertura dirigida,
    no cierre del ítem. [Evidencia](registro-commits/validacion-bitfields-c.md).
  - Records compartidos por valor de 8 y 12 bytes pasaron O0–O3 en el caso
    productor/consumidor. [Evidencia](registro-commits/prueba-record-c-compartido-valor.md).
  - Preparado en `ac71663a7` y `1454f719f`: propagación de alineación por
    campos anidados y caso MIR que distingue valores puntero. Sin ejecutar;
    los records anidados subalineados aún no se proyectan.
  - Actualización: `fb1796115`, `6670efd01` y `2627824b6` habilitan y
    prueban el record C anidado empaquetado. El fixture de layout pasó en
    O0–O3, con IR O0 y rechazo de dirección; la suite interna de sesión pasó.
    La tanda sigue abierta.
  - `6497c64f2` reprodujo una dirección indebidamente aceptada en un arreglo
    dentro de record subalineado; `2f97e22b8` la corrige. Con `aa662dd8f`
    pasaron acceso y rechazo en layout O0–O3; `f02e37235` comprobó lectura y
    escritura con alineación 1 en el IR ya generado. No cierra T08-010.
  - Preparado en `40a0062b8`, `84549e59f` y `35e9cf099`: proyección C de
    arreglos fijos de records empaquetados y controles de acceso, dirección e
    IR. No se han construido ni ejecutado estos cambios; el ítem sigue abierto.
  - `pruebas-tanda-08-layout-clang` (rama `interop-t08-claude-layout`) comparó con Clang structs naturales,
    `pack(1/2/4)`, anidados en dos niveles, arreglos de records en `pack(2)`, `_Alignas` y typedefs alineados:
    pasaron O0–O3 directo y precompilado, con y sin encabezado. Los miembros anónimos C11 y el acceso a un
    miembro de arreglo flexible no se proyectan. La línea base del validador de layout con build propio pasó
    O0–O3, lo que supera la nota anterior sobre `40a0062b8`, `84549e59f` y `35e9cf099`. No cierra el ítem.
    [Comparación](registro-commits/t08-layout-comparacion-clang.md) y
    [formas pendientes](registro-commits/t08-layout-formas-pendientes.md).
  - Miembro de arreglo flexible: proyección como `T[*]` integrada para los tipos y alineaciones
    comprobados. El control pasó O0–O3 en directo, Shared y Metadata. La clasificación ABI indirecta
    corrigió los fallos de argumento y retorno por valor de los casos natural y empaquetado; la
    conversión implícita a puntero sigue rechazada y la tanda permanece abierta.
    [Proyección](registro-commits/t08-flexible-c-integracion.md) y
    [ABI](registro-commits/t08-flexible-c-abi.md).
  - El miembro flexible `int` desalineado de un struct `pack(1)` ya admite
    lecturas y escrituras en directo, Shared y Metadata O0–O3; su IR conserva
    alineación 1 como Clang. El retorno y argumento por valor del caso empaquetado
    pasaron también en los consumidores precompilados.
    [Evidencia](registro-commits/t08-flexible-c-packed.md).
  - Miembros anónimos C11: diseño con carriers y aliases del punto 326 y catálogo C versión 5; no
    implementado porque requiere NSMETA. [Diseño](registro-commits/t08-anonimos-c-diseno.md).
- [x] T08-011. Importar la declaración y campos de una `union` C.
  - Campos de bits con y sin signo superpuestos a entero y bytes pasaron
    lectura y escritura cruzada con C en O0–O3. Es una muestra dirigida;
    otros layouts siguen pendientes. El caso volatile y la publicación
    precompilada se comprobaron en la muestra adicional de T08-015.
    [Evidencia](registro-commits/prueba-union-bitfields-c.md).
  - Las unions de ocho bytes con miembro double o struct anidado pasaron
    O0–O3 para retorno y paso por valor y acceso anidado.
    [Evidencia](registro-commits/prueba-union-c-compuesta.md).
  - Un productor precompilado y un consumidor con el mismo encabezado C
    conciliaron una union por puntero y por valor en O0–O3.
    [Evidencia](registro-commits/prueba-union-c-metadata.md).
  - Lectura y escritura superpuestas entre C y NativeSharp y paso y retorno por
    valor de unions de 4 y 12 bytes pasaron O0–O3 en Windows x64. Es evidencia
    dirigida; faltan layouts y formas de uso. [Detalle](registro-commits/prueba-union-c-por-valor.md).
  - Un caso dirigido de miembro escrito desde NativeSharp y byte superpuesto
    leído desde C compiló, enlazó y ejecutó en O0. Faltan O1–O3 y otros
    contratos. [Evidencia](registro-commits/prueba-union-c-superpuesta.md).
  - `pruebas-tanda-08-layout-clang` pasó O0–O3 directo y precompilado con union natural, union `pack(1)`,
    union dentro de struct empaquetado, struct dentro de union y bitfields en union. Las unions anónimas
    C11 dentro de un struct no se proyectan. No cierra el ítem.
    [Comparación](registro-commits/t08-layout-comparacion-clang.md) y
    [formas pendientes](registro-commits/t08-layout-formas-pendientes.md).
  - Unions anónimas dentro de structs: cubiertas por el mismo diseño, no implementado.
    [Diseño](registro-commits/t08-anonimos-c-diseno.md).
- [x] T08-012. Obtener de Clang tamaño, alineación y desplazamiento de cada campo.
- [x] T08-013. Representar tipos declarados pero incompletos sin inventar su tamaño.
  - Un puntero a struct C incompleto se usó en retorno, parámetros y global
    con biblioteca separada O0–O3. El local por valor se rechaza con un
    diagnóstico específico de tipo incompleto; faltan otros usos.
    [Evidencia](registro-commits/prueba-punteros-opacos-c.md).
  - El mismo fixture pasó O0–O3 con un global `Opaque**` y con una union C
    opaca en retorno, parámetros y global; la union por valor se rechazó con
    diagnóstico de tipo incompleto. No cubre todos los contratos de punteros.
    [Evidencia](registro-commits/prueba-punteros-opacos-c.md).
  - Un productor precompilado publicó punteros a struct y union opacos; su
    consumidor con o sin el mismo header los usó en O0–O3; otros casos de
    identidad y calificación siguen pendientes.
    [Evidencia](registro-commits/prueba-opacos-header-compartido.md).
- [x] T08-014. Mapear punteros y su tipo apuntado.
  - Una muestra `_Bool*` devuelta por C, almacenada en una global y elegida
    junto a un `bool*` nativo pasó O0–O3 con acceso de byte en IR; hubo acceso
    cruzado desde ambos lenguajes. Otras formas y calificadores siguen abiertos.
    [Resultado](registro-commits/t08-bool-puntero-c-implementacion.md).
- [x] T08-015. Conservar los calificadores de tipo que afecten al acceso.
  - Los campos C de enum y de record declarados `volatile` se proyectan con su
    calificador. Una muestra de record anidado se comprobó directa y con solo
    metadata O0–O3, con comprobación de accesos `volatile` en el IR. No cubre
    todos los layouts ni concurrencia.
    [Enum](registro-commits/t08-campo-enum-volatile.md),
    [record](registro-commits/t08-record-anidado-volatile.md),
    [metadata](registro-commits/t08-record-volatile-precompilado.md).
  - `CppLibraryReference` admite `VolatileMode` Default, Microsoft o ISO.
    La matriz de acceso directo pasó IR y ejecución O0–O3 en los modos
    predeterminado e ISO; Microsoft explícito pasó el control de IR y un valor
    inválido se rechazó. La suite de sesión pasó con 32 advertencias al compilar.
    Los accesos por locales o parámetros no quedan cubiertos.
    [Resultado](registro-commits/t08-volatile-modo-validacion.md).
  - Arreglos globales C de punteros a `_Bool` y siete tipos escalares volatile
    pasaron IR y ejecución O0–O3; la muestra es de una dimensión y no cubre
    transporte por locales o parámetros.
    [Resultado](registro-commits/t08-volatile-arreglo-punteros-c-implementacion.md).
  - Una matriz de siete tipos escalares mediante globales C puntero a volatile
    pasó O0–O3 y contrastó acquire/release por tipo contra Clang. El alcance es
    acceso directo; locales y parámetros siguen pendientes.
    [Resultado](registro-commits/t08-volatile-punteros-c-matriz-implementacion.md).
  - El acceso directo a global C `volatile _Bool*`, por índice y por `*`,
    conserva byte acquire/release y pasó O0–O3. La propagación por locales,
    parámetros y otras variantes sigue pendiente.
    [Resultado](registro-commits/t08-volatile-bool-puntero-implementacion.md).
  - Clang confirmó conversiones flotantes y `fcmp une` en dos asignaciones
    compuestas `_Bool` C volatile; falta implementar y comprobar NSC.
    [Referencia](registro-commits/t08-bool-compuesto-flotante-referencia.md).
    Preparada la implementación para cuatro operadores y ambos anchos;
    construcción y validación pendientes. [Detalle](registro-commits/t08-bool-compuesto-flotante-implementacion.md).
    Preparados controles de flotantes, cero y NaN; pendientes de ejecución.
    [Detalle](registro-commits/t08-bool-compuesto-flotante-fixture.md).
    Validación posterior: construcción incremental y fixture record-c pasaron
    O0–O3, incluyendo NaN y `fcmp une`. [Resultado](registro-commits/t08-bool-compuesto-flotante-validacion.md).
  - Clang confirmó promociones de bool e enteros de 8/16 bits y el ancho del
    valor desplazado frente a un contador ancho; falta implementarlas y
    validarlas. [Referencia](registro-commits/t08-bool-compuesto-promociones-referencia.md).
    Preparada la cadena de análisis, HIR y LLVM; faltan controles y ejecución.
    [Detalle](registro-commits/t08-bool-compuesto-promociones-implementacion.md).
    Preparados controles de promociones y desplazamientos con contadores
    anchos; pendientes de ejecución. [Detalle](registro-commits/t08-bool-compuesto-promociones-fixture.md).
    Validación posterior: construcción incremental y fixture record-c pasaron
    O0–O3 con los operandos pequeños y contadores anchos preparados.
    [Resultado](registro-commits/t08-bool-compuesto-promociones-validacion.md).
  - Preparada referencia Clang de `_Bool` C compuesto con `uint`, `long` y
    `ulong`: i32 para el primero e i64 para los otros. Falta implementación y
    validación. [Detalle](registro-commits/t08-bool-compuesto-enteros-referencia.md).
    Preparada la ruta semántica y LLVM; pendientes controles y ejecución.
    [Detalle](registro-commits/t08-bool-compuesto-enteros-implementacion.md).
    Preparados controles para valores anchos, signo y órdenes de memoria; aún
    sin ejecutar. [Detalle](registro-commits/t08-bool-compuesto-enteros-fixture.md).
    Validación posterior: construcción incremental y fixture record-c pasaron
    O0–O3, incluidos los anchos y signos comprobados en IR. Otros operandos,
    desplazamientos anchos y la validación integral siguen pendientes.
    [Resultado](registro-commits/t08-bool-compuesto-enteros-validacion.md).
  - Clang confirmó división, residuo y desplazamientos compuestos de `_Bool` C
    volatile en ejemplos válidos; falta implementar y validar en NSC.
    [Referencia](registro-commits/t08-bool-compuesto-div-shift-referencia.md).
    Implementación preparada en análisis, HIR y LLVM; pendiente construir y
    validar. [Detalle](registro-commits/t08-bool-compuesto-div-shift-implementacion.md).
    Preparados controles positivos y de IR; pendientes de ejecución.
    [Detalle](registro-commits/t08-bool-compuesto-div-shift-fixture.md).
    Validación posterior: construcción incremental y fixture record-c pasaron
    O0–O3, con operaciones i32 y accesos de byte comprobados en IR.
    [Resultado](registro-commits/t08-bool-compuesto-div-shift-validacion.md).
  - Preparada la admisión de seis operadores compuestos con `int` en el mismo
    arreglo C `volatile _Bool`; pendientes HIR, LLVM y validación agrupada.
    [Detalle](registro-commits/t08-bool-compuesto-int-analisis.md).
    HIR conserva el operando `int`; LLVM y las pruebas siguen pendientes.
    [Detalle](registro-commits/t08-bool-compuesto-int-hir.md).
    LLVM prepara la operación en i32 y normalización posterior; construcción y
    validación siguen pendientes. [Detalle](registro-commits/t08-bool-compuesto-int-llvm.md).
    Preparados controles para las seis operaciones y el IR de promoción;
    pendientes de ejecución. [Detalle](registro-commits/t08-bool-compuesto-int-fixture.md).
    Validación posterior: construcción incremental y fixture record-c pasaron
    O0–O3, con comprobaciones de once accesos y siete promociones en IR; el bool
    nativo conserva su rechazo. [Resultado](registro-commits/t08-bool-compuesto-int-validacion.md).
  - Preparada la admisión semántica de `&=`, `|=` y `^=` entre bool sobre
    arreglos globales C `volatile _Bool` en modo MS. Faltan el descenso y las
    pruebas agrupadas; no se considera soporte hasta ejecutarlas.
    [Detalle](registro-commits/t08-bool-compuesto-analisis.md).
    Preparado el descenso LLVM con acceso i8 acquire/release; la construcción y
    los controles positivos siguen pendientes. [Detalle](registro-commits/t08-bool-compuesto-llvm.md).
    Preparado el fixture positivo O0–O3 para las tres operaciones; conserva el
    control negativo del bool nativo y está pendiente de ejecución.
    [Detalle](registro-commits/t08-bool-compuesto-fixture.md).
    Actualización: construcción incremental y fixture record-c pasaron, con
    O0–O3 para las tres operaciones y rechazo de bool nativo. IR O0 verificó
    cuatro lecturas acquire y escrituras release de byte con alineación 1.
    La validación completa posterior y demás operaciones siguen pendientes.
    [Resultado](registro-commits/t08-bool-compuesto-validacion.md).
  - Clang confirmó para `volatile _Bool` compuesto la promoción a `int`,
    conversión del resultado a cero/no cero y carga acquire/escritura release
    de i8. NSC aún rechaza el caso; el bool nativo mantiene su propia regla.
    [Referencia](registro-commits/t08-bool-compuesto-referencia.md).
  - Clang confirmó que el orden del acceso por `volatile int*` cambia con el modo de la unidad consumidora; queda pendiente definir cómo lo selecciona NSC sin inferirlo de la procedencia del puntero. [Comparación](registro-commits/procedencia-puntero-volatile-local.md#comparación-del-modo-de-la-unidad-consumidora).
  - Validación agrupada posterior: compilación incremental, sesión 1/1 y fixture mixto O0–O3 con comprobaciones de IR y diagnósticos negativos; el caso booleano compuesto permanece rechazado. [Resultado y límites](registro-commits/validacion-agrupada-volatile-escalares-bool.md).
  - Clang confirmó acquire/release en arreglos globales C de enteros de
    8–64 bits y float/double. La validación agrupada posterior comprobó los
    controles preparados. `_Bool` usa una ruta de almacenamiento i8 separada.
    [Detalle](registro-commits/control-orden-array-escalar-ms.md).
  - Preparadas lectura y asignación simple de arreglo global C `_Bool`
    volatile mediante i8 acquire/release y conversión a bool i1. Aún no
    validadas; las operaciones compuestas siguen pendientes.
    [Detalle](registro-commits/control-array-volatile-bool.md).
  - El análisis de código indica que NSC rechaza asignaciones compuestas
    numéricas con bool. Preparados controles negativos para bool NS y `_Bool`
    importado; no se consideran soporte de la operación.
    [Detalle](registro-commits/control-bool-compuesto.md).
  - Construcción incremental, sesión Clang y fixture mixto O0–O3 pasaron para
    los casos preparados de globales y arreglos volatile; el IR confirmó los
    órdenes directos comprobados. Locales y punteros obtenidos de arreglos
    conservan una discrepancia de orden. No cierra el ítem.
    [Evidencia](registro-commits/validacion-agrupada-volatile-08.md).
  - En la referencia Clang, desreferenciar `volatile int*` usa acquire/release;
    un IR NS anterior solo marca volatile. Falta conservar procedencia y
    alineación de apuntados. [Evidencia](registro-commits/ir-puntero-volatile-ms.md).
  - Preparada la corrección para desreferencia directa de un global C
    `volatile int*` con modo MS conocido. Los controles aún no se ejecutan;
    locales, parámetros y arreglos de punteros siguen pendientes.
    [Detalle](registro-commits/control-puntero-directo-volatile-ms.md).
  - Preparada la extensión para desreferenciar el puntero obtenido directamente
    de un arreglo o matriz global C, con controles para distinguir el slot del
    objeto apuntado. Aún no validada; los demás orígenes siguen pendientes.
    [Detalle](registro-commits/control-puntero-array-volatile-ms.md).
  - Preparado el acceso `global[indice]` para un global C `volatile int*` con
    controles de IR; falta validación agrupada y no cubre otros orígenes.
    [Detalle](registro-commits/control-indice-puntero-volatile-ms.md).
  - Preparada la combinación `arreglo[i][j]` para un puntero obtenido
    directamente de un arreglo global C. El control aún no se ejecuta.
    [Detalle](registro-commits/control-indice-puntero-array-volatile.md).
  - Mapeada la pérdida de procedencia de modo MS al guardar el puntero en un
    local, y el requisito de manejar reasignaciones y ramas antes de emitir
    órdenes. [Detalle](registro-commits/procedencia-puntero-volatile-local.md).
  - La referencia Clang emitió lectura `atomic volatile` con `acquire` para
    `volatile_array[1]`; un IR NS anterior contiene `load volatile` sin ese
    orden. Pendiente reproducir con el compilador actualizado y corregir el
    contrato concreto. [Evidencia](registro-commits/ir-global-array-volatile-ms.md).
  - Preparada una corrección restringida a arreglos globales C `volatile int`
    con origen, modo MS y alineación acreditados. Los controles incluyen
    orden en IR y casos negativos de alineación; aún no están ejecutados.
    [Detalle](registro-commits/control-orden-array-volatile.md).
  - Preparada la propagación de punteros a `volatile` en arreglos globales C,
    con controles para matriz, `const volatile`, descarte del calificador y
    dirección del slot. La implementación y las pruebas aún esperan la
    validación agrupada; no cierran este ítem.
    [Detalle](registro-commits/prueba-arreglo-punteros-volatile.md).
  - La matriz volatile de 8, 16, 32 y 64 bits pasó O0–O3 y su control IR:
    en las actualizaciones C importadas NSC ahora emite dos cargas y una
    escritura, como Clang. La alineación i8 de NSC sigue siendo menor y
    conservadora. Otros layouts y contratos permanecen pendientes. No cerrar.
    [Evidencia](registro-commits/prueba-matriz-volatile-bitfields.md).
  - Campos de bits `volatile` enteros C en structs normales, empaquetados y
    una union: ejecución cruzada O0–O3 y comparación de IR con Clang para seis
    accesos pasaron. Con productor precompilado también pasaron dos consumidores
    y el control de su IR. Faltan otros layouts; no cierra el ítem.
    [Evidencia](registro-commits/prueba-volatile-bitfields-c.md) y
    [metadata](registro-commits/prueba-metadata-volatile-bitfields.md).
  - Diferencia reproducida en `8c6ddf489`: al acceder por puntero a un enum
    volatile alineado dentro de un arreglo C, Clang emite release/acquire y
    NSC emite volatile ordinario. El nuevo control falla en NSC; falta corregir
    el orden sin cambiar la semántica de los campos que Clang deja ordinarios.
  - Preparados `270816da5` y `758458cb1`: cada record observado conserva el
    modo efectivo de su sesión y existe un control de procedencia. Falta
    ejecutar la suite y transportar o conciliar el modo hasta los accesos;
    la generación de código todavía no cambia y el control anterior sigue fallando.
  - Preparados `06e256fd9` y `a09ae6ff7`: conciliación del modo entre
    observaciones y transporte al símbolo del campo, con prueba de procedencia.
    Falta ejecutar la prueba y conectar HIR, MIR y generación; sigue fallando
    el control de orden del enum alineado.
  - Preparado `70b42a663`: transporte del modo al campo HIR y direcciones de
    arreglos fijos, sin propagación desde punteros cargados. No se ha construido
    ni validado; aún no aplica orden atómico y el defecto permanece abierto.
  - Preparados `54def11ca`, `b271b6494` y `ff6a989`: identificación del enum
    en MIR, comprobación de alineación y emisión acquire/release. Construcción
    incremental en curso; falta ejecutar el control comparativo y la batería.
  - Actualización: construcción incremental de NSC y de la suite de sesión
    terminaron con código 0; la suite pasó. Layout pasó O0–O3 y el control de
    IR confirmó acquire/release para el caso alineado y preservó controles de
    alineación 1. Es evidencia dirigida; T08-015 y la tanda siguen abiertos.
  - Preparados `f301a1a16` y `03830c483`: lectura, IR y rechazo de escritura
    para arreglos C de record y enum const volatile. Sin ejecutar.
  - Actualización: layout pasó en O0–O3 con lecturas, IR volatile alineado a 1
    y rechazos de escritura y dirección. `610c4b097` corrigió el control de
    diagnóstico y la repetición pasó. La tanda e ítem siguen abiertos.
  - Preparados `9ce8f6a8d`, `cec4a8c6f` y `b312fc47a`: arrays C de record y
    enum volatile, con acceso mixto, direcciones e IR. Sin construir ni validar.
  - Actualización: construcción incremental y layout pasaron en O0–O3; se
    comprobaron acceso compartido, IR volatile con alineación 1 y rechazo de
    direcciones. La evidencia es dirigida y la tanda permanece abierta.
  - Preparados `656034022`, `0e4bf14bb` y `8d4417cfd`: mapeo y pruebas de
    arreglos C const de records y enums empaquetados, con lectura y rechazos
    de escritura y dirección. Todavía no construidos ni validados.
  - Actualización: NSC se construyó incrementalmente y layout pasó O0–O3;
    se verificaron lecturas y rechazos de escritura y dirección. La primera
    ejecución encontró expectativas de diagnóstico erróneas, corregidas en
    `a519938a0`. La validación completa de la tanda y el ítem siguen pendientes.
  - Preparados en `ed66fe2a1`: controles de lectura y direcciones const con
    alineación natural, pendientes de ejecución agrupada.
  - Actualización: layout pasó O0–O3 con lectura de ambos arreglos alineados y
    rechazo de direcciones que pierden const. No cierra T08-015.
  - Preparado, pendiente de validación agrupada: parámetros con enlace C de un
    nivel a void, escalares, structs y enums volatile. La función C realiza los
    accesos y se conserva el tipo de Clang; las pruebas incluyen const y
    ejecución mixta. Siguen pendientes los punteros volatile que NativeSharp
    accede directamente y los demás contratos. [Detalle](TANDA-08-TIPOS-C.md).
  - Trabajo de acceso directo al objeto apuntado: modelos de
    expresiones y globales, locales inferidos, operadores, HIR y controles
    iniciales de asignación y conversión. Se prepararon contratos de llamadas
    ordinarias, barreras provisionales para llamadas indirectas y variádicas y
    controles internos. Se conectaron globales C de puntero a escalar, struct
    o enum volatile y resultados C a los mismos tipos; faltan cobertura completa de
    propagación, auditoría de escapes y validación. Los demás agregados y
    niveles anidados permanecen pendientes.
  - Validación parcial: la batería agrupada pasó en `0231c6ecb`, incluidos los
    casos preparados de aliases a apuntado volatile escalar, struct y enum.
    No cubre todos los calificadores ni los niveles anidados; el ítem sigue
    abierto. [Evidencia](TANDA-08-TIPOS-C.md#validación-agrupada-tras-correcciones).
  - Hallazgo inicial: Clang puede emitir accesos `volatile` atómicos
    acquire/release en Windows; la primera validación no acreditó ese orden.
    [Evidencia](TANDA-08-TIPOS-C.md#diferencia-de-orden-en-volatile-de-clang-para-windows).
  - Validado parcialmente en `2939a7210`: global directo int volatile de 32
    bits bajo la opción MS y alineación comprobada, con acquire/release en IR
    y ejecución O0–O3. El resto de formas sigue pendiente.
    [Evidencia](TANDA-08-TIPOS-C.md#validación-agrupada-con-orden-ms-de-global-directo).
  - Validación agrupada posterior a `4d2176569`: globales directos enteros de
    8, 16 y 64 bits con y sin signo, además del caso previo de 32 bits; IR y
    ejecución mixta O0–O3. La restauración MIR preservó las alineaciones
    opcionales. Otros tipos y formas de acceso siguen pendientes.
    [Evidencia](TANDA-08-TIPOS-C.md#validación-de-globales-volatile-enteros).
  - En `f8f3d49ab` pasó la batería agrupada con identidad de código sensible al
    modo y la alineación volatile, y comprobación de anchos y alineaciones
    exactos en IR. [Evidencia](TANDA-08-TIPOS-C.md#validación-de-identidad-y-anchos-volatile).
  - En `d14fab758` pasó el caso de almacenamiento de puntero global volatile
    de 64 bits: C y NativeSharp emiten acquire/release y el objeto apuntado
    conserva su calificación. [Evidencia](TANDA-08-TIPOS-C.md#validación-del-puntero-global-volatile).
  - Preparado en `ae43f0699` y `94cf8a6aa`: globales C `volatile float` y
    `volatile double` con orden MS y prueba mixta de IR y ejecución. Clang
    confirmó el orden en ejemplos aislados; la prueba NSC sigue pendiente.
    [Detalle](TANDA-08-TIPOS-C.md#globales-volatile-flotantes-preparados).
  - Preparado en `f0c570a7c` y `212bb5e48`: orden acquire/release para `??=`
    sobre puntero global C volatile con modo MS y alineación acreditados; prueba
    de ambas ramas e IR pendiente de ejecución agrupada.
  - Validación dirigida posterior a `831f998d3` y `db2a2a352`: el caso record C
    pasó IR y ejecución O0–O3 con globales flotantes y `??=`. La batería
    completa y las demás formas de volatile siguen pendientes.
- [x] T08-016. Mapear arreglos de tamaño conocido.
  - Punteros C a arreglos fijos de struct y enum pasaron lectura y mutación
    O0–O3; se rechazaron argumentos const e identidades distintas. El enum se
    inicializó desde C por otra limitación pendiente. [Evidencia](registro-commits/proyeccion-arreglo-agregado-c.md).
  - `int * const[2][2]` y `const int * const[2]` pasaron lectura y modificación
    permitida en O0–O3; escritura y escapes indebidos se rechazaron. Persisten
    otras formas y la validación completa. [Evidencia](registro-commits/prueba-matriz-puntero-fijo.md).
  - Un global C `int * const[2]` permitió modificar los datos apuntados en
    O0–O3; reasignación de slot, escape por dirección y argumento mutable se
    rechazaron. La suite de sesión pasó. Quedan otras formas de arreglos y
    calificadores. [Evidencia](registro-commits/proyeccion-global-puntero-fijo.md).
  - Una matriz global C `const int *[2][2]` pasó lectura, reasignación y
    verificación C en O0–O3; se rechazaron escritura al dato y pérdida de
    const hacia `int*`. La regresión de layout pasó. Quedan otras dimensiones,
    rutas de escape y tipos. [Evidencia](registro-commits/prueba-matriz-global-puntero-const.md).
  - Un arreglo global C `const int *[2]` pasó lectura y reasignación de slots
    en O0–O3; se rechazaron escritura al dato, conversión a `int*` y dirección
    del slot. La regresión de layout pasó. Faltan otras dimensiones, tipos y
    rutas de escape; no cierra el ítem. [Evidencia](registro-commits/prueba-global-puntero-const.md).
  - Preparado en `4a18050a6` y `3f88e82e0`: transporte de alineación de
    elementos indexados desde campos y prueba MIR sintética. Aún sin ejecutar;
    arreglos empaquetados y protección de sus direcciones siguen pendientes.
  - Validación dirigida en `2343368d8`: matriz C `int[2][3]` con `pack(1)`
    pasó en O0–O3, con alineación 1 en IR y rechazo de dirección. La suite de
    sesión y el cálculo interno también pasaron. Falta la batería completa y
    otras formas de arreglos y calificadores.
  - Validación dirigida en `a0dc6fc71`: arreglos C empaquetados `const`,
    `volatile` y `int*[2]` pasaron IR O0, ejecución O0–O3 y casos negativos.
    Quedan otras combinaciones y la batería completa de la tanda.
  - Preparados `764d93ba0` y `aa48d2539`: matriz empaquetada C de records,
    acceso mixto, dirección e IR. Pendiente de construcción y validación.
  - Actualización: la construcción incremental y layout pasaron en O0–O3 para
    el arreglo y la matriz de records empaquetados, con comprobación de IR O0
    y rechazo de direcciones subalineadas. T08-016 sigue abierto.
- [x] T08-017. Definir la representación de punteros a función C.
  - Un callback con `volatile` heredado del record usa accesos volátiles simples;
    el declarado `volatile` mantiene acquire/release cuando está alineado. Se
    comprobó ejecución O0–O3 y el IR optimizado contra el control de Clang.
    No cubre todas las rutas precompiladas.
    [Corrección](registro-commits/t08-callback-volatile-heredado.md),
    [IR optimizado](registro-commits/t08-callback-volatile-ir-optimizado.md).
  - Productor y consumidores precompilados para callback almacenado con enum pasaron O0–O3 con y sin referencia explícita al header; se ocultó solo la copia de salida. [Evidencia](registro-commits/enum-c-reconciliado-campo.md).
  - El callback almacenado que recibe y devuelve enum C pasó el fixture de campo/global O0–O3 y la suite de sesión. [Evidencia](registro-commits/callback-enum-precompilado.md).
  - Un callback global con record usado solo en su firma se importó y permitió
    resolver un campo mediante `var`; ejecutó O0–O3 en el gate ajustado con los
    cuatro marcadores de éxito. [Detalle](registro-commits/callback-global-record-inferido.md).
  - Un callback C cuyo resultado es un puntero a record pasó O0–O3 en retorno de función, campo y global. [Detalle](registro-commits/callback-retorno-puntero-record.md).
  - Un callback NativeSharp sin capturas que recibe un puntero a record C se pasó y almacenó para llamadas C y pasó O0–O3. [Detalle](registro-commits/callback-record-nativesharp-a-c.md).
  - Un consumidor precompilado sin referencia explícita al header usó el callback con record en O0–O3 con la copia usada por el productor temporalmente ausente. [Detalle](registro-commits/callback-record-precompilado.md).
  - El productor y consumidor precompilados de un callback que recibe un puntero a record C pasaron O0; O1–O3 quedan pendientes. [Detalle](registro-commits/callback-record-precompilado.md).
  - La llamada directa a callback escalar en campo y global pasó O0–O3. [Detalle](registro-commits/callback-c-invocacion-directa.md).
  - El callback almacenado con puntero a record C pasó O0–O3 para retorno, campo y global; la prueba de sesión pasó con controles de identidad. [Detalle](registro-commits/callback-c-puntero-record.md).
  - La lectura y escritura de callback volatile en global y campo pasaron
    O0–O3; el IR conserva acquire/release para alineación 8 y volatile
    ordinario para `pack(1)`, como Clang. [Detalle](registro-commits/ir-callback-volatile.md).
  - El callback almacenado volatile aún se rechaza como campo y global. Clang
    emitió una carga atómica acquire de puntero alineado para ambos; falta
    preservar y comprobar el orden completo antes de habilitar el acceso.
    [Detalle](registro-commits/callback-c-volatile.md).
  - Los callbacks vectorcall y C variádico retornados desde biblioteca
    precompilada se invocaron O0–O3; NSMETA conserva sus identidades.
    [Detalle](registro-commits/convenciones-callback-metadata.md).
  - La materialización inicial de `fn* C` en metadata permitió que el
    productor precompilado y su consumidor ejecutaran retorno y campo callback
    de O0 a O3. [Evidencia](registro-commits/materializacion-callback-c.md).
  - Productor precompilado con retorno callback genera metadata, pero el
    consumidor no encuentra el método en O0; queda por investigar su
    materialización. [Evidencia](registro-commits/callback-c-metadata.md).
  - Escrituras a campo y global de callback e invocación desde C pasaron
    O0–O3; escrituras a sus variantes const se rechazaron en O0.
    [Detalle](registro-commits/callback-c-escritura-const.md).
  - Tras el mapeo de campo/global y el ajuste de layout, los callbacks
    `int(int)` almacenados en ambos lugares pasaron O0–O3 junto con el caso
    retornado. [Evidencia](registro-commits/callback-c-campo-global.md).
  - La prueba O0 identificó que un callback en campo de struct C no se
    proyecta y otro en global C se rechaza por tipo no mapeado. El lanzador
    devolvió 0 pese a detenerse sin los marcadores finales; validar también el
    contenido del resultado. [Detalle](registro-commits/callback-c-campo-global.md).
  - Un callback C `int(int)` retornado, llamado desde NativeSharp y pasado de
    vuelta a C compiló y ejecutó O0–O3. No cubre todas las firmas ni
    convenciones. [Evidencia](registro-commits/callback-c-retornado.md).
- [x] T08-018. Resolver colisiones entre nombres importados y nombres NativeSharp.
  - Un caso real de sombra local de global y enumerador C pasó O0, con comprobación de resolución después del bloque y del valor de C. [Detalle](registro-commits/sombra-local-valores-c.md).
- [x] T08-019. Presentar diagnósticos útiles al usar una declaración incompleta de forma inválida.
  - Los diagnósticos para `+`, `-`, `+=`, incremento e índice sobre records C incompletos pasaron sus casos negativos; el control `Opaque**` pasó O0–O3. [Detalle](registro-commits/aritmetica-puntero-incompleto-c.md).
  - El local C incompleto por valor produce un diagnóstico específico; la
    prueba anterior de `sizeof` también pasó. Faltan otras operaciones y
    combinaciones. [Evidencia](registro-commits/diagnostico-local-incompleto-c.md).
- [x] T08-020. Comparar tamaño, alineación y offsets importados con los calculados por Clang.
  - El consumidor con alias a record y enum en firmas públicas pasó O0–O3 con
    el mismo header y con el header oculto; un alias incompatible se rechazó.
    La conciliación del enum independiente usa contrato serializado y USR;
    no acredita procedencia física por separado ni cierra la tanda.
    [Evidencia](registro-commits/t08-enum-conciliado-seleccion-validacion.md).
  - Preparado en `ad9b0a694` y `03cd47d50`: alineación explícita en el IR de
    accesos directos a campos y prueba mixta de lectura y escrituras. Falta
    ejecutarla y transportar la alineación efectiva de campos empaquetados.
  - El proyecto layout pasó con NSC construido desde `99716724c`: tamaños,
    offsets y alineación del fixture, acceso de campo y comprobaciones de IR.
    Los campos empaquetados siguen pendientes.
  - Preparado en `6b065035e` y `9de1a7bd5`: transporte de alineación
    importada en el modelo y casos internos de alineación. Aún sin ejecutar;
    el proyector sigue rechazando campos empaquetados.

  - Preparado en `ec8ff30ee`, `c8eb0e3cd` y `5c0835d96`: campos escalares C
    empaquetados, protección de su dirección y prueba mixta con `pack(1)`;
    aún sin ejecutar. Los agregados, punteros y campos C++ empaquetados siguen
    pendientes.
  - Validación dirigida en `291bc4cdb`: suite de sesión Clang y proyecto layout
    pasaron; este último verificó `int` con `pack(1)` en O0, IR con alineación
    1 y rechazo de dirección. La batería completa y el resto de formas siguen
    pendientes. [Detalle](TANDA-08-TIPOS-C.md#validación-dirigida-de-campo-c-empaquetado).

  - Preparado en `988b339e4` y `86eee7601`: campo puntero C empaquetado,
    protección de la dirección `T**` y prueba mixta; todavía sin ejecutar.
  - Validación dirigida en `e6f4cb130`: fixture layout pasó O0–O3 con el campo
    puntero `pack(1)` y campos `short`, `int` y `double` en `pack(2)`; IR O0
    y diagnósticos de dirección incluidos. La batería completa sigue pendiente.
  - `pruebas-tanda-08-layout-clang` (rama `interop-t08-claude-layout`) comparó 55 valores de layout,
    31 escrituras byte a byte, 31 lecturas cruzadas y la alineación IR contra Clang: O0–O3 directo, con
    encabezado y sólo metadata pasaron sin diferencias. En el consumidor sólo-metadata `sizeof(TipoC)` no
    se acepta y se midió con `sizeof(*p)`. Batería completa pendiente.
    [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
  - Nombres C sólo por metadata: el parser no los recibe (`ProjectCompilation::parseSyntaxUnits`); fallan
    declaración con puntero, `sizeof(T)` y cast, y funciona `T x = ...`. Diseño y prueba de aceptación
    preparados, sin implementar. [Diseño](registro-commits/t08-nombres-metadata-diseno.md).

## 09. Llamadas, enlace y datos C

**Tanda 9 en curso.** Se retiro el cierre prematuro: los controles disponibles
aportan evidencia parcial y falta contrastar cada requisito con su alcance y la
validacion necesaria. La auditoria y su evidencia estan en
[TANDA-09-AUDITORIA-INICIAL.md](TANDA-09-AUDITORIA-INICIAL.md); el alcance y
las restricciones estan en [SCOPE-LOCK-TANDA-09.md](SCOPE-LOCK-TANDA-09.md),
y las instrucciones de prueba en [TANDA-09-VALIDACION.md](TANDA-09-VALIDACION.md).

Se prepararon controles nuevos de [llamadas y globales](pruebas-tanda-09-llamadas/),
[agregados](pruebas-tanda-09-agregados/), [vectorcall](pruebas-tanda-09-vectorcall/),
[biblioteca ausente](pruebas-tanda-09-biblioteca-ausente/),
[simbolo ausente](pruebas-tanda-09-simbolo-ausente/) y
[global const](pruebas-tanda-09-global-const/),
[biblioteca NS precompilada](pruebas-tanda-09-precompilado/),
[cache de encabezados](pruebas-tanda-09-cache/) y
[cache de biblioteca](pruebas-tanda-09-cache-biblioteca/). Las comparaciones C y
NativeSharp de llamadas, agregados y vectorcall pasaron en estatica y DLL en
O0–O3; tambien pasaron los casos negativos y las pruebas de cache descritas en
la auditoria. La validacion agrupada seleccionada paso 6/6 y la identidad C
paso por separado. Se conserva pendiente el cierre integral de la tanda y no se
modificaron las pruebas existentes.

Las 16 casillas originales se conservan. Las pruebas nuevas aportan evidencia
para enlace estatico y DLL, sin extrapolar a todas las formas de metadatos o
cache. Las muestras no certifican interoperabilidad general C o C++. La suite
de sesiones Clang no se ejecuto por faltar su fixture, como se indica en la
validacion; su ausencia no debe confundirse con el fixture historico NSMeta.

- [x] T09-001. Auditar las representaciones HIR y MIR que usa NSC para llamadas externas. [Auditoria y volcados](TANDA-09-AUDITORIA-INICIAL.md).
- [x] T09-002. Auditar la emisión actual de llamadas C en el generador de código. [Auditoria y volcados](TANDA-09-AUDITORIA-INICIAL.md).
- [ ] T09-003. Conservar el nombre de símbolo y la convención de llamada elegidos por Clang. Verificado para cdecl y vectorcall en Windows x64; otras convenciones no se certifican. [Evidencia](TANDA-09-AUDITORIA-INICIAL.md).
  - Avance parcial: llamadas C y C++ `regcall` con registros flotantes anidados o con arreglo, directas y por puntero, validadas en O0-O3 con enlace estático y DLL; también pasan consumidores precompilados con y sin encabezado. [Evidencia y límites](TANDA-09-REGCALL-ANIDADOS.md).
  - Avance parcial: dos registros flotantes del mismo tipo y un entero pasan muestras C y C++ directo, por puntero y precompilado, estática y DLL O0–O3. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#dos-registros-expandidos-plan-en-preparación).
  - Avance parcial: dos registros C y C++ de tipos distintos pero igual forma flotante pasan muestras directas, por puntero y precompiladas; otras formas siguen pendientes. [Evidencia C](TANDA-09-REGCALL-ANIDADOS.md#dos-registros-c-de-tipos-distintos), [evidencia C++](TANDA-09-REGCALL-ANIDADOS.md#dos-registros-c-de-tipos-distintos-1).
  - Avance parcial: registros C y C++ flotantes de 12 y 8 bytes pasan muestras directas, por puntero y precompiladas en estática y DLL. [Evidencia C](TANDA-09-REGCALL-ANIDADOS.md#registros-c-de-tamaños-distintos), [evidencia C++](TANDA-09-REGCALL-ANIDADOS.md#registros-c-de-tamaños-distintos-1).
  - Avance parcial: las muestras C y C++ también pasan cuando el primer registro tiene tipo y tamaño distintos del retorno, incluyendo consumidores precompilados. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#primer-registro-distinto-del-retorno).
  - Avance parcial: un entero intercalado entre dos records flotantes pasa muestras C y C++ directas, por puntero y precompiladas; otras combinaciones siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#escalar-entre-registros).
  - Avance parcial: un entero antes de dos records flotantes pasa muestras C y C++ directas, por puntero y precompiladas; otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#escalar-antes-de-los-registros).
  - Avance parcial: dos records flotantes y un entero con retorno `int` pasan muestras C y C++ directas, por puntero y precompiladas; otros retornos siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-expandidos-con-retorno-entero).
  - Avance parcial: records flotantes con retorno `double` pasan muestras C y C++ directas, por puntero y precompiladas; otros retornos siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-expandidos-con-retorno-double).
  - Avance parcial: records flotantes con retorno `void` pasan muestras C y C++ directas, por puntero y precompiladas con efectos observables; otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-expandidos-con-retorno-void).
  - Avance parcial: records flotantes con retorno `float` pasan muestras C y C++ directas, por puntero y precompiladas; otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-expandidos-con-retorno-float).
  - Avance parcial: records flotantes con retorno de 64 bits pasan muestras C y C++ con y sin signo y consumidores precompilados, incluido un valor sin signo con el bit alto; otras firmas y valores siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-expandidos-con-retorno-entero-de-64-bits).
  - Avance parcial: records flotantes con retorno `int*` pasan muestras C y C++ directas, por puntero y precompiladas, con identidad y escritura verificadas; otros punteros y firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-expandidos-con-retorno-de-puntero).
  - Avance parcial: el retorno `float*` pasa muestras C y C++ directas, por puntero y precompiladas con identidad y escritura; otros punteros y firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#retorno-de-puntero-a-flotante).
  - Verificación adicional: retornos `short*` y `double*` pasan muestras C y C++ directas, por puntero y desde consumidores precompilados que usan el alias. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#retornos-de-puntero-a-short-y-double).
  - Avance parcial: retornos enteros de 8 y 16 bits con y sin signo pasan muestras C y C++ directas, por puntero y precompiladas; otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-expandidos-con-retorno-entero-estrecho).
  - Avance parcial: el retorno booleano pasa muestras C y C++ directas, por puntero y precompiladas, con `zeroext` y valores verdadero y falso verificados. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-expandidos-con-retorno-booleano).
  - Avance parcial: retorno `unsigned int` con bit alto, `long` y `unsigned long` de Win64 pasan muestras C y C++ directas, por puntero y precompiladas. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-expandidos-con-retorno-entero-de-32-bits-sin-signo).
  - Avance parcial: un registro flotante expandido junto a un registro mixto compacto pasa muestras C y C++ directas, por puntero y precompiladas; otras formas siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registro-expandido-junto-a-registro-mixto-compacto).
  - Avance parcial: registros compactos de dos y cuatro bytes junto a un registro expandido pasan muestras C y C++ directas, por puntero y precompiladas. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registros-compactos-de-dos-y-cuatro-bytes).
  - Avance parcial: un registro expandido junto a otro de 24 bytes pasado por copia indirecta pasa muestras C y C++ directas, por alias y precompiladas; otros layouts siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registro-expandido-junto-a-registro-pasado-indirectamente).
  - Verificación adicional: un registro de tres bytes y alineación uno pasado por copia junto al expandido pasa muestras C y C++ directas, por alias y desde consumidores precompilados que usan el alias. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#registro-expandido-junto-a-registro-pasado-indirectamente).
  - Avance parcial: un retorno de registro flotante con argumento compacto y otro expandido pasa muestras C y C++ directas, por alias y precompiladas; otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#retorno-de-registro-flotante-con-argumento-compacto).
  - Verificación adicional: el retorno de registro flotante también pasa con un argumento de 24 bytes por copia indirecta, en C y C++, con llamadas directas, por alias y desde consumidores precompilados. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#retorno-de-registro-con-argumento-indirecto).
  - Avance parcial: un retorno compacto de dos enteros junto a un argumento flotante expandido pasa en C y C++ directo y por puntero, y desde consumidores precompilados en estática y DLL, con y sin encabezado. Otras formas siguen pendientes. [Prueba dirigida](TANDA-09-REGCALL-ANIDADOS.md#retorno-compacto-con-argumento-flotante-expandido-pendiente).
  - Pendiente confirmado: el retorno indirecto de un registro de 24 bytes junto a un argumento flotante expandido funciona en el control C, pero NSC aún rechaza la firma. Se verificó el plan físico sin acreditar ejecución. [Prueba](TANDA-09-REGCALL-ANIDADOS.md#retorno-indirecto-con-argumento-flotante-expandido-pendiente).
  - Avance parcial: firmas C y C++ `regcall` con registro flotante anidado e `int` pasan en llamadas directas y por puntero O0-O3 con estática y DLL, también en consumidores precompilados con y sin encabezado. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#firma-mixta-en-investigación).
  - Avance parcial: se validó una secuencia de ocho enteros tras el registro, con presión de registros y valores sensibles a la posición, en C y C++ directo, por puntero y precompilado. También pasaron incrementos sucesivos como argumentos; otras clases de efectos y argumentos siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#varios-enteros-y-presión-de-registros).
  - Avance parcial: una cola de `int`, `float` y `double` tras el registro pasa en C y C++, directa, por puntero y precompilada, con validación de tipos contra el plan. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#enteros-y-flotantes-tras-el-registro).
  - Avance parcial: la cola admite también enteros de 32/64 bits con y sin signo e `int*` sin calificadores en las muestras C y C++, directas, indirectas y precompiladas. [Evidencia y límites](TANDA-09-REGCALL-ANIDADOS.md#enteros-de-64-bits-y-puntero).
  - Avance parcial: enteros de 8/16 bits y booleano pasan en C y C++, con `zeroext` en el índice físico verificado, también por puntero y precompilados. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#enteros-estrechos-y-booleanos).
  - Avance parcial: `long` y `unsigned long` de 32 bits se concilian y ejecutan en Win64 para las muestras C y C++ directas, indirectas y precompiladas. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#long-de-32-bits-en-win64).
  - Avance parcial: las muestras de punteros simples y dobles sin calificadores a escalares pasan en C y C++ directas, indirectas y precompiladas. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#punteros-dobles-a-escalares).
  - Avance adicional: la muestra `const int*` pasa en C y C++, incluso precompilada; se rechazan las conversiones incompatibles probadas. Otros calificadores y casos siguen pendientes. [Evidencia y límites](TANDA-09-REGCALL-ANIDADOS.md#puntero-a-escalar-const).
  - Avance adicional: se corrigió y probó `const float*` en C y C++, incluyendo consumidores precompilados. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#puntero-a-float-const).
  - Avance adicional: `const double*` y `const void*` pasan las muestras C y C++ directas, indirectas y precompiladas; otras conversiones y calificadores siguen pendientes. [Evidencia](TANDA-09-REGCALL-ANIDADOS.md#punteros-const-double-y-const-void).
  - Avance parcial: un puntero sin calificar a record completo pasa en las muestras C y C++ triviales, directas, indirectas y precompiladas, con identidad conciliada. [Evidencia y límites](TANDA-09-REGCALL-ANIDADOS.md#puntero-a-record-en-la-firma-expandida).
  - Avance adicional: funciones C escalares `preserve_most` y `preserve_all`, y una funcion C++ `preserve_most`, conservan convencion y simbolo en las muestras directas y precompiladas. Otras firmas siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md).
  - Avance adicional: firmas mixtas C con flotantes, enteros, booleano y `int*` pasan en ambas convenciones; la muestra C++ `preserve_most` equivalente tambien pasa. Otras firmas siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md#firmas-escalares-mixtas).
  - Avance adicional: punteros C `preserve_most` y `preserve_all` con parametros y retorno `int` funcionan en las muestras directas y precompiladas; otras firmas indirectas siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md#punteros-preserve-devueltos-por-c).
  - Avance adicional: firmas mixtas indirectas C con flotantes, enteros, booleano y `int*` pasan en ambas convenciones y desde plantilla precompilada. Otras firmas indirectas siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md#firmas-mixtas-por-puntero).
  - Verificacion adicional: una funcion C++ `preserve_most` se convierte a un alias de puntero en namespace y se llama con firma mixta, tambien desde plantilla precompilada. [Evidencia](TANDA-09-PRESERVE.md#alias-c-preserve_most-por-puntero).
  - Avance adicional: retornos `void` e `int*` funcionan en muestras C directas e indirectas de ambas convenciones preserve, tambien precompiladas. Otros retornos siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md#retornos-void-y-puntero).
  - Avance adicional: retornos booleanos C preserve pasan en llamadas directas e indirectas y desde plantilla precompilada; el IR conserva `zeroext`. Otros retornos siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md#retornos-booleanos).
  - Avance adicional: enteros C estrechos con y sin signo pasan como parametros y retornos preserve, directamente, por puntero y precompilados, en ambas convenciones. Otras firmas siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md#enteros-estrechos).
  - Avance adicional: retornos y parametros `float`, `uint` y `ulong` pasan en muestras preserve C directas, indirectas y precompiladas, en ambas convenciones. [Evidencia](TANDA-09-PRESERVE.md#retornos-escalares-adicionales).
  - Avance adicional: `void*`, `byte*` y `short*` pasan en muestras C preserve directas, indirectas y precompiladas, con escritura e identidad verificadas. Otros tipos apuntados siguen pendientes de prueba. [Evidencia](TANDA-09-PRESERVE.md#punteros-a-otros-tipos).
  - Verificacion adicional: un puntero a record C y `int**` pasan y retornan en muestras preserve directas, indirectas y precompiladas, con escritura e identidad verificadas. [Evidencia](TANDA-09-PRESERVE.md#punteros-a-record-y-punteros-dobles).
  - Avance adicional: records C de 8 y 24 bytes se pasan y retornan por valor en llamadas preserve directas, indirectas y precompiladas, conservando la copia y el ABI observado. Otros layouts siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md#records-c-por-valor).
  - Verificacion adicional: records C de 3 y 4 bytes pasan por valor y se retornan en ambas convenciones, tambien por puntero y precompilados. Otros layouts siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md#records-de-3-y-4-bytes).
  - Dependencia parcial: un record C++ trivial por valor con `preserve_most` pasa en las muestras directa y por puntero, tambien precompiladas con y sin encabezado. Otros records y operaciones de T11 siguen pendientes. [Evidencia](TANDA-09-PRESERVE.md#record-c-trivial-precompilado).
  - Avance previo de esa dependencia: NSC observa rasgos de copia, layout, construccion y destruccion de records C++ desde Clang. [Evidencia y limites](TANDA-09-PRESERVE.md#dependencia-record-c-trivial-por-valor).
  - Avance previo de esa dependencia: la sesion consulta la seleccion de copia const/no const y destructor, incluidos acceso declarado, eliminacion y ambiguedad. [Evidencia](TANDA-09-PRESERVE.md#dependencia-record-c-trivial-por-valor).
  - Avance previo de esa dependencia: los tipos C++ seleccionados conservan las observaciones de su sesion y USR en el modelo vivo; la serializacion se incorporo despues para la muestra trivial. [Evidencia](TANDA-09-PRESERVE.md#dependencia-record-c-trivial-por-valor).
  - Verificacion adicional: un record C++ trivial de 24 bytes pasa y retorna por valor en llamadas `preserve_most` directas y por puntero, tambien desde plantillas precompiladas. [Evidencia](TANDA-09-PRESERVE.md#record-c-trivial-de-24-bytes).
  - Avance adicional: un record C++ trivial de ocho bytes se pasa y retorna por valor en una funcion ordinaria `cdecl`, directa y por puntero, tambien precompilada. Otros layouts y tipos siguen pendientes. [Evidencia](TANDA-09-CPP-CDECL.md).
  - Verificacion adicional: un record C++ trivial de 24 bytes pasa y retorna por valor en llamadas ordinarias directas y por puntero, tambien precompiladas. [Evidencia](TANDA-09-CPP-CDECL.md#record-trivial-de-24-bytes).
  - Avance adicional: una llamada C directa `regcall` con tres parametros y retorno `int` conserva el simbolo y la convencion de Clang en estatica, DLL y plantilla precompilada. Otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md).
  - Avance adicional: un puntero `regcall` devuelto por C y una funcion compatible convertida a puntero se ejecutan con tres `int`, tambien desde plantilla precompilada. Otras firmas indirectas permanecen pendientes. [Evidencia](TANDA-09-REGCALL.md#puntero-regcall-devuelto-por-c).
  - Avance adicional: retornos `int*` y `void` funcionan en llamadas C `regcall` directas y por puntero, tambien desde plantillas precompiladas; otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#retornos-void-y-puntero-en-regcall).
  - Avance adicional: `short*` y punteros a enteros de ocho bits con y sin signo pasan en llamadas C `regcall` directas y por puntero, tambien precompiladas; otras formas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#punteros-a-otros-enteros-en-regcall).
  - Verificacion adicional: punteros a record C e `int**` pasan en llamadas C `regcall` directas y por puntero, tambien precompiladas, conservando identidad y escritura. [Evidencia](TANDA-09-REGCALL.md#punteros-a-record-y-punteros-dobles-en-regcall).
  - Avance parcial: records C de ocho y 24 bytes pasan y retornan por valor en llamadas `regcall` directas y por puntero, tambien precompiladas, con plan de Clang validado; los records con clasificacion expandida y otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#records-c-regcall-precompilados).
  - Avance parcial: se extraen y conservan piezas y desplazamientos de argumentos `regcall` expandidos para las muestras de flotantes; en esa etapa faltaba su emision y ejecucion. [Evidencia](TANDA-09-REGCALL.md#plan-de-argumentos-regcall-expandidos).
  - Avance parcial: el record C de dos `float` ejecuta llamadas `regcall` directas y por puntero, tambien precompiladas. El rechazo se conserva para firmas mixtas aun no implementadas; otras formas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#ejecucion-del-record-de-dos-flotantes).
  - Avance parcial: un record C++ trivial de dos `float` funciona en llamadas `regcall` directas y por puntero, tambien desde plantillas precompiladas, con estatica y DLL. Otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#alias-c-regcall-precompilado-con-record-flotante).
  - Avance parcial: records planos C y C++ triviales de tres y cuatro `float` funcionan en llamadas `regcall` directas y por puntero, tambien precompiladas, en las muestras verificadas. Otras firmas y layouts siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#records-planos-de-tres-y-cuatro-flotantes).
  - Avance adicional: enteros estrechos con y sin signo se pasan y retornan en muestras C `regcall` directas e indirectas, tambien precompiladas. Otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#enteros-estrechos-en-regcall).
  - Avance adicional: booleanos C `regcall` pasan en llamadas directas e indirectas y booleanos C++ en llamadas directas e indirectas, tambien precompiladas, conservando `zeroext`. Otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#alias-c-de-puntero-regcall-precompilado).
  - Avance adicional: una firma directa `regcall` combina enteros de 32 y 64 bits con y sin signo y retorno de 64 bits, tambien desde plantilla precompilada. Otros tipos y firmas permanecen pendientes. [Evidencia](TANDA-09-REGCALL.md).
  - Avance adicional: una firma directa `regcall` con `float`, parametro `int*` y retorno `float` pasa frente a C y C++, tambien desde plantillas precompiladas. Otros tipos y firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#float-y-puntero-en-regcall).
  - Avance adicional: una firma `regcall` con `double`, `int*` y retorno `double` funciona en C y C++ directa y por puntero, tambien desde plantillas precompiladas. Se conservan controles negativos para firmas no admitidas; otras firmas siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#double-escalar-en-regcall).
  - Avance adicional: una funcion libre C++ `regcall` dentro de un namespace se llama en estatica y DLL, tambien desde plantilla precompilada con o sin encabezado; el control `cdecl` equivalente tambien pasa. Otras firmas y dependencias C++ quedan pendientes. [Evidencia](TANDA-09-REGCALL.md).
  - Avance adicional: dos sobrecargas C++ de una funcion en namespace, distinguidas por `int` y `long long`, se seleccionan desde plantilla precompilada con o sin encabezado. Otras sobrecargas y tipos siguen pendientes. [Evidencia](TANDA-09-REGCALL.md#sobrecargas-c-en-plantilla-precompilada).
  - Avance adicional: llamadas C variadicas SysV con parametros escalares y records C planos por valor pasan muestras directas y `byval`, incluso al agotarse registros enteros o flotantes, tambien desde plantillas precompiladas sin encabezado. Un record `packed` con campo desalineado pasa por valor y por puntero en esa ruta; atributos combinados, otras formas y convenciones siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Avance adicional: un record variadico mixto puede combinarse con `int*` fijo, `short*` variadico y retorno `int` o `int*`, tambien desde una plantilla precompilada sin encabezado. [Evidencia](TANDA-09-SYSV-ABI.md).
  - Avance adicional: un record mixto variadico por valor puede combinarse con un puntero variadico a otro record C plano, o con puntero fijo y retorno del mismo record, tambien desde plantilla precompilada sin encabezado. [Evidencia](TANDA-09-SYSV-ABI.md).
  - Avance adicional: un record variadico C con un arreglo fijo de dos `double` pasa desde plantilla precompilada sin encabezado, conservando las dos piezas observadas por Clang. [Evidencia](TANDA-09-SYSV-ABI.md).
  - Avance adicional: un record variadico C que contiene otro record nombrado pasa desde plantilla precompilada sin encabezado, con identidad y layout de ambos comprobados. [Evidencia](TANDA-09-SYSV-ABI.md).
  - Avance adicional: pasan muestras con arreglos de dos o tres records interiores y una union nombrada anidada, incluida copia `byval` y plantilla precompilada sin encabezado. [Evidencia](TANDA-09-SYSV-ABI.md).
  - Avance adicional: pasan campos con `typedef` escalares encadenados y de records en la muestra variadica precompilada; aliases con atributos aun no estan admitidos por esta ruta. [Evidencia](TANDA-09-SYSV-ABI.md).
  - Avance adicional: pasan muestras variadicas precompiladas con campos de bits C nombrados y con campos anonimos y de ancho cero `unsigned int`; otras formas siguen pendientes en esta ruta. [Evidencia](TANDA-09-SYSV-ABI.md).
  - Avance adicional: pasa una muestra precompilada con campos de bits `_Bool` y un campo `_Bool` ordinario; otros layouts booleanos siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md).
  - Avance adicional: punteros a funciones `sysv_abi` devueltos por biblioteca C se invocan con muestras de escalares, incluidos booleanos y enteros estrechos con sus extensiones, punteros y retorno `void`, tambien desde plantilla precompilada; otras firmas y creacion de callbacks siguen pendientes. [Reproduccion, pruebas y limites](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: una funcion C `sysv_abi` importada se convierte a un puntero compatible y conserva su direccion, en estatica, DLL y plantilla precompilada; las conversiones de convenciones incompatibles se rechazan. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: llamadas variadicas indirectas SysV con argumentos escalares promovidos y puntero pasan en estatica, DLL y plantilla precompilada; se probo una firma con agotamiento de registros y paso por pila. Agregados y otras firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance parcial: un puntero SysV con record de tres enteros por valor y retorno `int` paso frente a C en estatica y DLL O0-O3 y desde una plantilla precompilada sin encabezado. La prueba negativa anterior se conserva con variantes aun no admitidas y pasa. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: una muestra con cuatro enteros y dos piezas `i64` paso en llamada indirecta y plantilla precompilada, con estatica y DLL O0-O3. Otras firmas siguen abiertas. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: una muestra indirecta con record `double`/`int`, piezas `double`/`i32` y retorno `int` paso directamente y desde plantilla precompilada, estatica y DLL O0-O3. Otras formas mixtas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: el record mixto entre dos argumentos `int` paso directamente y desde plantilla precompilada, estatica y DLL O0-O3; se comprobo el orden observable de los argumentos escalares en la muestra directa. Otras firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: dos records mixtos del mismo tipo y un `int` pasaron directamente y en plantilla precompilada, estatica y DLL O0-O3. El descenso recorre los parametros mediante el plan de Clang; solo se acreditan las piezas y firmas probadas. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record mixto y otro de tres enteros, de tipos distintos, pasaron junto a un `int` directamente y desde plantilla precompilada, estatica y DLL O0-O3. [Evidencia y limites](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record de cinco enteros paso por copia `byval` en una llamada indirecta directa y precompilada, estatica y DLL O0-O3. Otras formas indirectas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: una firma indirecta combina enteros, un record mixto dividido en piezas y un record grande `byval` en posicion posterior; paso directamente y desde plantilla precompilada, estatica y DLL O0-O3. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un puntero SysV recibe y devuelve por valor el mismo record de tres enteros, directamente y desde plantilla precompilada, en estatica y DLL O0-O3. Otras formas de retorno siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un puntero SysV recibe por `byval` y devuelve por `sret` el mismo record de cinco enteros, directamente y desde plantilla precompilada, en estatica y DLL O0-O3. Otras formas de retorno en memoria siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un retorno `sret` de cinco enteros se combina con enteros, un record mixto dividido y un argumento `byval` del mismo tipo retornado; paso directamente y precompilado, estatica y DLL O0-O3. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un puntero SysV devuelve un record de tres enteros sin recibir un record como argumento, directamente y precompilado, estatica y DLL O0-O3. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un puntero SysV sin parametros devuelve un record de tres enteros directamente y desde plantilla precompilada, estatica y DLL O0-O3. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance parcial: un puntero variadico SysV con prefijo `int` y record mixto opcional pasa frente a C y desde plantilla precompilada sin encabezado, en estatica y DLL O0-O3. Otras firmas siguen pendientes; la prueba previa de transporte exige el rechazo de esta llamada y permanece intacta. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: la misma firma con dos records mixtos opcionales pasa frente a C y desde plantilla precompilada sin encabezado, estatica y DLL O0-O3; otras combinaciones siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record de cinco enteros como argumento opcional indirecto se pasa por copia `byval` y conserva el original; paso frente a C y precompilado, estatica y DLL O0-O3. Otras firmas por copia siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record mixto en registros seguido de otro record opcional `byval` pasa frente a C y desde plantilla precompilada, estatica y DLL O0-O3. Otras combinaciones siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record mixto opcional seguido de un `int` opcional pasa frente a C y desde plantilla precompilada, estatica y DLL O0-O3. Otras mezclas escalares siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record mixto opcional seguido de un `double` opcional pasa frente a C y desde plantilla precompilada, estatica y DLL O0-O3. Otras mezclas y promociones siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Verificacion adicional: un `float` opcional tras un record mixto se promociona a `double` y pasa frente a C y precompilado, estatica y DLL O0-O3. Otras promociones y combinaciones siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record mixto opcional seguido de un `int*` opcional pasa frente a C y precompilado, estatica y DLL O0-O3; se verifica escritura por el puntero. Otras formas de puntero siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record mixto, ocho `double` y un `int*` opcionales pasan con presion de registros y un `double` en pila, directamente y precompilados en estatica y DLL O0-O3. Otras clases y combinaciones siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record mixto opcional despues de ocho `double` se copia por `byval` en pila segun Clang y conserva el original; pasa directamente y precompilado en estatica y DLL O0-O3. Otras formas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Verificacion adicional: tras agotar registros enteros, un record mixto y un puntero posterior se pasan en pila y funcionan directamente y precompilados en estatica y DLL O0-O3. Otras condiciones siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un puntero variadico SysV con prefijo fijo `double`, record mixto e `int` opcionales pasa frente a C y precompilado, estatica y DLL O0-O3. Otros prefijos siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: la variante con prefijo fijo `int*` verifica escritura y pasa frente a C y precompilada, estatica y DLL O0-O3. Otros prefijos siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: una firma con parametros fijos `int` y `double`, record mixto e `int` opcionales pasa frente a C y precompilada, estatica y DLL O0-O3. Otros prefijos y tipos siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: una llamada variadica indirecta devuelve `double` y recibe record mixto e `int` opcionales; pasa frente a C y precompilada, estatica y DLL O0-O3. Otros retornos siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: una llamada variadica indirecta con retorno `void`, prefijo `int*` y record mixto opcional verifica escritura y pasa frente a C y precompilada, estatica y DLL O0-O3. Otros retornos y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: la variante con retorno `int*` preserva identidad y escritura frente a C y precompilada, estatica y DLL O0-O3. Otros retornos y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un `long long` C opcional de 64 bits junto a un record mixto conserva un valor fuera de 32 bits frente a C y precompilado, estatica y DLL O0-O3. Otros escalares y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: el prefijo fijo `long long` C conserva un valor fuera de 32 bits junto al record opcional, frente a C y precompilado en estatica y DLL O0-O3. Otros prefijos y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un `unsigned int` opcional conserva un valor superior al maximo entero con signo de 32 bits junto al record, frente a C y precompilado en estatica y DLL O0-O3. Otros escalares y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un prefijo fijo `unsigned int` conserva el mismo valor junto al record opcional, frente a C y precompilado en estatica y DLL O0-O3. Otros prefijos y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record mixto fijo y otro opcional de la misma identidad pasan por piezas frente a C y precompilados, estatica y DLL O0-O3. Otros records fijos y presiones siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: el record mixto fijo y otro opcional de identidad distinta pasan frente a C y precompilados, estatica y DLL O0-O3; se rechaza una variante no admitida del fijo. Otros layouts y presiones siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record fijo de cinco enteros se pasa por copia `byval` junto a un record mixto opcional y conserva el original, frente a C y precompilado en estatica y DLL O0-O3. Otros records y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Verificacion adicional: dos records de cinco enteros de la misma identidad, uno fijo y otro opcional, se pasan por copias `byval` separadas frente a C y precompilados en estatica y DLL O0-O3. Otras firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record mixto fijo y otro opcional de tres enteros pasan por piezas frente a C y precompilados en estatica y DLL O0-O3. Otros layouts y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: el record de tres enteros como parametro fijo y el mixto como opcional pasan por piezas frente a C y precompilados en estatica y DLL O0-O3. Otros layouts y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: `unsigned long long` fijo y opcional conserva los 64 bits junto a un record frente a C y precompilado en estatica y DLL O0-O3. Otros tipos y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: retornos `unsigned int` y `unsigned long long` conservan el bit alto junto a un record opcional, frente a C y precompilados en estatica y DLL O0-O3. Otros retornos y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: retornos `long long` y `float` pasan junto a un record opcional frente a C y precompilados en estatica y DLL O0-O3. Otros retornos y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: retornos `signed char` y `unsigned short` conservan `signext`/`zeroext` junto a un record opcional, frente a C y precompilados en estatica y DLL O0-O3. Otros retornos y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: retornos `short` y `unsigned char` conservan `signext`/`zeroext` junto a un record opcional, frente a C y precompilados en estatica y DLL O0-O3. Otros retornos y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record opcional de dos enteros y ocho bytes pasa como pieza `i64` en puntero variadico SysV, tambien precompilado; otras clases y firmas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record opcional de un `double` pasa por puntero variadico SysV, tambien desde plantilla precompilada; la emision de piezas se basa en el plan y los limites del layout. Otras formas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: un record fijo de dos enteros pasa junto a un record mixto opcional por puntero variadico SysV, tambien desde plantilla precompilada. Otras formas siguen pendientes. [Evidencia](TANDA-09-SYSV-ABI.md#puntero-de-funcion-sysv-devuelto-por-una-biblioteca).
  - Avance adicional: llamadas C variadicas SysV con prefijo escalar o de punteros, argumentos escalares promocionados y punteros, y retorno de puntero pasan las muestras con registros y pila. Se asocian planes de Clang a llamadas reales y se transportan a MIR. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Avance adicional: records SysV con piezas `i64` y `i8`, `i16` o `i32`, 16 bytes de tamaño y alineacion de 8, pasan como argumento y retorno junto a argumentos directos e indirectos. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Avance parcial: llamadas directas `sysv_abi` con `sbyte`, `byte`, `short`, `ushort`, `int`, `uint`, `long`, `ulong`, `bool`, `float`, `double`, `int*`, `double*`, `void*`, `int**` y retorno `void` o de esos tipos pasaron en estatica y DLL O0–O3. Otras firmas y callbacks SysV siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Avance adicional: funciones SysV C que reciben y devuelven el mismo record de uno a ocho bytes con el plan directo de Clang pasaron las pruebas indicadas; otras firmas y layouts siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron tambien records de nueve a dieciseis bytes con dos piezas enteras y retorno compuesto, incluidas plantillas precompiladas. No cubre otras clases de registro ni todos los layouts. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron firmas mixtas `double`/`int`, `int`/`double` e `int`/`float`, con piezas en distinto orden y plantillas precompiladas. Otras combinaciones mixtas siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron firmas con vectores fijos de dos `float`, como pieza unica o junto con otras piezas, incluido `double` en ambos ordenes y una muestra alineada a 16; otras formas vectoriales siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron records con un `float` o `double` como pieza escalar y plantilla precompilada; otras formas flotantes siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron records de dos `double` y combinaciones `double`/`float` en ambos ordenes, incluida precompilacion; otras combinaciones flotantes siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron records indirectos con retorno `sret` y argumento `byval`, incluidas muestras alineadas hasta 256 bytes; siguen pendientes otras formas indirectas. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron firmas con dos y ocho argumentos de record pequeno del mismo tipo y retorno del mismo tipo; otras firmas con varios argumentos siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron records pequenos de distintos tamanos y mezclas con `int`, incluidos ambos ordenes y ocho argumentos con paso por pila; otras mezclas siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Firmas de dos argumentos con record pequeno y `float` o `double` en ambos ordenes pasaron las pruebas dirigidas y de precompilacion; otras mezclas siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probo tambien `float`, record pequeno, `float`, con precompilacion; firmas flotantes de mayor complejidad siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Una firma con un record pequeno y nueve escalares `float`/`double` intercalados paso las pruebas, incluido el paso de flotantes por pila y precompilacion; quedan abiertas otras clasificaciones de agregados. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probo un record pequeno junto con `int*`, incluida la lectura y escritura de la memoria apuntada y precompilacion; quedan pendientes otros punteros y firmas mixtas. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron records de doce y dieciseis bytes clasificados en dos piezas enteras, seguidos de `int` y `float`; incluyen precompilacion y orden de argumentos nombrados. Otras clasificaciones y posiciones siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron records de cuatro y doce bytes con `uint`, `long` y `ulong`, incluidos valores de 64 bits fuera del rango de 32 bits y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron dos y tres records iguales de doce bytes, cada uno en dos piezas enteras, junto con un `int`; la muestra de tres agota los registros enteros e incluye argumento en pila. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probo una firma con records de doce y dieciseis bytes y `int`, cada record con la anchura fisica indicada por Clang, incluida precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - La misma mezcla paso tambien con el `int` antes de los dos records y argumentos nombrados reordenados, incluida precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Una mezcla de records de una y dos piezas enteras paso en ambos ordenes, con argumentos nombrados y precompilacion; otras clases de pieza siguen pendientes. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron retorno `sret` y dos records `byval` del mismo tipo intercalados con `float` e `int`, con alineaciones de 1 y 8 bytes y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probo tambien retorno de 20 bytes con argumentos indirectos de 24 y 32 bytes y alineaciones diferentes, incluida una muestra con argumento alineado a 16 y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron retorno indirecto y argumentos que combinan un record directo de tres o cuatro bytes con otro indirecto, incluidos argumentos nombrados y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - La mezcla con retorno indirecto paso tambien con un record directo de doce o dieciseis bytes dividido en dos piezas enteras y otro indirecto, incluida precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probo un record directo con piezas `double` e `int` en una llamada con retorno y otro argumento indirectos; se conservan las clases observadas por Clang para precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron ademas combinaciones de piezas enteras, flotantes y vectores de dos `float` con retorno indirecto, incluidos ambos ordenes y una muestra alineada a 16. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron retornos indirectos sin argumentos `byval`, tanto sin parametros como con escalares y un record directo de piezas mixtas; incluye precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron tres records directos de una pieza `float`, `double` o vector de dos `float`, junto a retorno indirecto y escalares; incluye precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probo un record directo con piezas `double` e `i64`, junto a retorno indirecto y escalares; se verificaron ambos campos enteros y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probo una llamada con retorno `int` y argumentos de record directo mixto y record indirecto `byval`, incluidos argumentos nombrados y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - La misma familia se probo con retornos `long`, `uint`, `ulong`, `float`, `double` y `void`, con enlace estatico, DLL y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probo retorno `int` con dos records directos de una pieza, uno entero y otro vectorial, sin argumento indirecto ni descompuesto. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - `sbyte`, `byte`, `short` y `ushort` como retorno y argumento, mezclados con records directos SysV, pasaron con extensiones de signo o cero, incluidas firmas combinadas y retorno indirecto. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron booleano C e `int*`, `double*`, `void*` e `int**` como retorno y argumento junto con records directos, con precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron punteros a record C y a `short` como argumento y retorno junto a records por valor, con identidad, escritura y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron enums C de 32 bits con signo y sin signo como argumento y retorno junto a records por valor, incluido consumo precompilado. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron retornos directos de dos piezas `double`/`i32`, `i64`/`double` y vector/vector, junto a records directos e indirectos, escalares y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron retornos directos de record de una pieza `i24`, `i32`, `i64`, `float` y vector de dos `float`, junto a argumentos directos, `byval` y precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron siete clasificaciones de records con relleno: enteros de 8, 16 o 32 bits junto a `double` y vectores junto a enteros de 8 o 16 bits, como argumento y retorno. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probo un record empaquetado de nueve bytes que Clang pasa indirectamente, con retornos indirectos, escalares y directos, y consumo precompilado. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
  - Se probaron records alineados con piezas `float`/`i64`, `i64`/`float`, `float`/vector y vector/`float` como argumento y retorno, con precompilacion. [Evidencia y limites](TANDA-09-SYSV-ABI.md).
- [ ] T09-004. Emitir una llamada C sin parámetros y con resultado escalar. [Prueba y contraste](pruebas-tanda-09-llamadas/).
- [ ] T09-005. Emitir una llamada C con parámetros escalares. [Prueba y contraste](pruebas-tanda-09-llamadas/).
- [ ] T09-006. Emitir el paso de punteros a una función C. [Prueba y contraste](pruebas-tanda-09-llamadas/).
- [ ] T09-007. Emitir el paso de agregados por valor conforme al ABI objetivo. Records de 1, 2, 3, 4, 8 y 24 bytes y union de 4 en Windows x64. [Prueba](pruebas-tanda-09-agregados/).
- [ ] T09-008. Emitir la recepción de agregados devueltos conforme al ABI objetivo. Mismas muestras y limite que T09-007. [Prueba](pruebas-tanda-09-agregados/).
- [ ] T09-009. Leer una variable global C importada. Global mutable escalar en estatica y DLL. [Prueba](pruebas-tanda-09-llamadas/).
- [ ] T09-010. Escribir una variable global C importada cuando su declaración lo permita. Global mutable y rechazo de escritura const. [Pruebas](TANDA-09-AUDITORIA-INICIAL.md).
- [ ] T09-011. Diseñar las promociones y el paso de argumentos de funciones C variádicas. [Reglas y limites auditados](TANDA-09-AUDITORIA-INICIAL.md).
- [ ] T09-012. Emitir llamadas variádicas con las promociones correctas. Llamadas directas e indirectas y tipos registrados en la auditoria, Windows x64. [Pruebas](pruebas-tanda-09-llamadas/).
- [x] T09-013. Enlazar una llamada contra una biblioteca estática de prueba. Evidencia: [proyecto mixto por consola](SCOPE-LOCK-EJECUCION.md#proyecto-mixto-por-consola); se conserva la limitación de caché.
- [x] T09-014. Enlazar y ejecutar una llamada contra una DLL de prueba. Evidencia: [proyecto C con DLL](SCOPE-LOCK-EJECUCION.md#presupuesto-función-libre-importada-de-dll); llamada escalar sin caché de artefactos.
- [ ] T09-015. Comprobar que los errores de enlace indiquen la biblioteca o símbolo faltante. La biblioteca ausente se diagnostica al cargar el proyecto y el simbolo ausente durante el enlace. [Pruebas](TANDA-09-AUDITORIA-INICIAL.md).
- [ ] T09-016. Contrastar los resultados de llamadas y datos con un consumidor C compilado por Clang. [Llamadas](pruebas-tanda-09-llamadas/), [agregados](pruebas-tanda-09-agregados/) y [vectorcall](pruebas-tanda-09-vectorcall/).

## 10. Nombres y operaciones de C++

- [ ] T10-001. Diseñar una identidad de declaración C++ que distinga contextos y sobrecargas.
- [ ] T10-002. Importar namespaces y sus miembros visibles.
- [ ] T10-003. Importar funciones libres C++ con sus tipos de parámetros y resultado.
- [ ] T10-004. Importar variables globales C++ accesibles.
  - Avance parcial: una variable escalar mutable declarada `dllimport` se leyó y escribió con una DLL real tanto desde un arnés como desde un proyecto NativeSharp con `CppLibraryReference`; faltan las demás formas de variables. Evidencia en [la bitácora](SCOPE-LOCK-EJECUCION.md).
- [ ] T10-005. Importar métodos estáticos de una clase.
  - Avance parcial: el driver importa métodos estáticos escalares con o sin `noexcept`; un método lanzador se ejecutó desde biblioteca estática y DLL atravesando un frame NS. Faltan otras firmas y contratos.
- [ ] T10-006. Importar campos estáticos de una clase.
  - Avance parcial: el driver importa campos estáticos escalares y un proyecto real los utilizó con biblioteca estática y DLL; TLS y otros contratos siguen pendientes.
- [ ] T10-007. Conservar el acceso público, protegido o privado de cada declaración.
- [ ] T10-008. Resolver una función sobrecargada a partir de los argumentos NativeSharp.
- [ ] T10-009. Definir cómo intervienen las conversiones implícitas C++ en esa resolución.
- [ ] T10-010. Resolver argumentos predeterminados mediante información de Clang.
  - Avance parcial: los snapshots de funciones y métodos identifican parámetros con valor predeterminado. Falta conservar y evaluar la expresión y usarla en llamadas; [evidencia](SCOPE-LOCK-EJECUCION.md).
- [ ] T10-011. Obtener el símbolo C++ y su mangling desde la representación de Clang.
- [ ] T10-012. Emitir llamadas a funciones libres C++.
  - Avance parcial: una función escalar `noexcept` declarada `dllimport` se llamó desde un proyecto NativeSharp enlazado con una DLL real. La cobertura de otras firmas y excepciones sigue pendiente; [evidencia](SCOPE-LOCK-EJECUCION.md).
- [ ] T10-013. Emitir llamadas a métodos estáticos C++.
  - Avance parcial: llamadas a sobrecargas `int` y `double` en arnés y llamadas escalares desde proyectos reales con bibliotecas estática y DLL; faltan otras firmas y contratos.
- [ ] T10-014. Leer campos estáticos C++ importados.
  - Avance parcial: lectura de campos escalares desde proyectos reales con bibliotecas estática y DLL; `const` y `volatile` se verificaron en el arnés. Faltan otros tipos y contratos.
- [ ] T10-015. Escribir campos estáticos C++ cuando su declaración lo permita.
  - Avance parcial: escritura de un campo `dllimport` desde un proyecto real; en el arnés se verificaron campos ordinario y `volatile` y el rechazo de escritura a `const`.
- [ ] T10-016. Definir cómo se proyectan operadores C++ sobre expresiones NativeSharp.
- [ ] T10-017. Comprobar sobrecargas, namespaces y miembros estáticos con una biblioteca compilada por Clang oficial.

## 11. Clases C++ y vida de objetos

- [ ] T11-001. Auditar los modelos de valor, referencia, propiedad y destrucción existentes en NSC.
- [ ] T11-002. Definir la representación NativeSharp de una instancia de clase C++.
- [ ] T11-003. Obtener de Clang tamaño y alineación de cada clase completa.
- [ ] T11-004. Importar los campos de instancia accesibles de una clase.
  - Avance parcial: proyectos reales importaron, leyeron y escribieron un campo escalar de un objeto C++ creado por la biblioteca, incluso con la misma definición incluida desde dos headers; faltan otros tipos, accesos y layouts.
- [ ] T11-005. Importar los métodos de instancia accesibles de una clase.
  - Avance parcial: el driver importó métodos públicos no virtuales escalares con o sin `noexcept`; se ejecutó un método lanzador desde biblioteca estática y DLL atravesando un frame NS. Faltan los demás contratos y diagnósticos.
- [ ] T11-006. Importar los constructores disponibles y sus parámetros.
- [ ] T11-007. Importar el destructor y sus restricciones de acceso.
- [ ] T11-008. Reservar almacenamiento con la alineación requerida para un objeto por valor.
  - Avance experimental: el local por valor llega al análisis de layout con tamaño y alineación de Clang; sigue protegido antes de HIR y todavía no reserva almacenamiento. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-incremental-objetos-c-locales-por-valor-2026-11-05).
- [ ] T11-009. Emitir la construcción de un objeto C++ en almacenamiento NativeSharp.
  - Pendiente para locales por valor: conectar el layout de Clang al almacenamiento final y construir in situ; el descenso actual crea un temporal y carga el valor. [Análisis](SCOPE-LOCK-EJECUCION.md#investigación-incremental-objetos-c-locales-por-valor-2026-11-05).
- [ ] T11-010. Emitir el argumento implícito de instancia de un método C++.
  - Avance parcial: llamadas `p->metodo()` con receptor C++ real pasaron en Windows x64, incluyendo proyectos con biblioteca estática y DLL. Evidencia en [la bitácora](SCOPE-LOCK-EJECUCION.md).
- [ ] T11-011. Leer un campo de instancia mediante la disposición calculada por Clang.
  - Avance parcial: lectura de un `int` público en offset 4 de un objeto C++ real, incluido un proyecto por el driver; faltan otros layouts.
- [ ] T11-012. Escribir un campo de instancia cuando su tipo y acceso lo permitan.
  - Avance parcial: escritura de un campo escalar en un proyecto real y observación desde C++ en el arnés; faltan otros tipos y contratos.
- [ ] T11-013. Emitir la destrucción de un objeto al finalizar su vida útil.
  - Pendiente para locales por valor: conectar el destructor externo a las regiones de limpieza; el descenso actual puede omitir el drop si no encuentra layout MIR. [Análisis](SCOPE-LOCK-EJECUCION.md#investigación-incremental-objetos-c-locales-por-valor-2026-11-05).
  - Avance experimental: el análisis semántico registra una región de cleanup para un local C++ con destructor; permanece bloqueado antes de layout y aún no emite la llamada. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-incremental-objetos-c-locales-por-valor-2026-11-05).
- [ ] T11-014. Destruir los objetos ya construidos si falla una construcción posterior.
- [ ] T11-015. Definir la copia de un objeto C++ con constructor de copia disponible.
- [ ] T11-016. Definir el movimiento de un objeto C++ con constructor de movimiento disponible.
- [ ] T11-017. Impedir una copia cuando el tipo C++ la haya eliminado.
- [ ] T11-018. Definir la reserva y liberación de objetos creados dinámicamente.
- [ ] T11-019. Definir el tratamiento de referencias y punteros que no poseen el objeto.
  - Avance parcial: un proyecto NativeSharp usa `ref` para modificar y llamar un objeto C++ de propiedad externa; en O0/O2 salir del ámbito no ejecutó su destructor y la biblioteca lo destruyó explícitamente una vez. Faltan vida útil del puntero, concurrencia, DLL y otras salidas. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-incremental-referencias-locales-a-objetos-c-2026-11-05).
- [ ] T11-020. Comprobar orden de constructores y destructores con contadores observables.
- [ ] T11-021. Comprobar copia y movimiento con tipos de comportamiento no trivial.
- [ ] T11-022. Comprobar la propiedad de objetos creados en una DLL y usados desde NativeSharp.

## 12. Herencia, despacho y tipos dinámicos

- [ ] T12-001. Importar las clases base y sus niveles de acceso.
- [ ] T12-002. Obtener de Clang los ajustes de dirección entre subobjetos base y derivados.
- [ ] T12-003. Permitir la conversión válida de derivado a base.
- [ ] T12-004. Emitir llamadas a métodos heredados con el ajuste de instancia requerido.
- [ ] T12-005. Identificar métodos virtuales y sus sustituciones.
- [ ] T12-006. Emitir despacho virtual respetando el ABI de Clang para el objetivo elegido.
- [ ] T12-007. Definir y comprobar destrucción mediante una base con destructor virtual.
- [ ] T12-008. Representar herencia múltiple y sus distintos subobjetos base.
- [ ] T12-009. Representar las reglas y ajustes de herencia virtual.
- [ ] T12-010. Definir la interoperación con RTTI cuando la biblioteca lo utilice.
- [ ] T12-011. Definir las conversiones dinámicas compatibles con el modelo importado.
- [ ] T12-012. Comparar el despacho y los ajustes con un consumidor C++ equivalente.

## 13. Templates y características evaluadas al compilar

- [ ] T13-001. Auditar cómo NSC representa genéricos, argumentos de tipo e instanciaciones.
- [ ] T13-002. Identificar las interfaces de Clang utilizables para instanciar templates.
- [ ] T13-003. Definir cómo se localiza la definición de un template en sus headers.
- [ ] T13-004. Importar una plantilla de clase y sus parámetros.
- [ ] T13-005. Importar una plantilla de función y sus parámetros.
- [ ] T13-006. Resolver un argumento de tipo C++ suministrado desde NativeSharp.
- [ ] T13-007. Diseñar la representación de un tipo NativeSharp como argumento de template C++.
- [ ] T13-008. Separar el tratamiento de structs NativeSharp triviales del de structs con propiedad o destrucción.
- [ ] T13-009. Definir la representación de clases NativeSharp de referencia usadas como argumentos.
- [ ] T13-010. Solicitar a Clang la instanciación de una plantilla con argumentos resueltos.
- [ ] T13-011. Importar los miembros de la especialización resultante.
- [ ] T13-012. Manejar parámetros de template que no son tipos.
- [ ] T13-013. Manejar parámetros de template que son plantillas.
- [ ] T13-014. Manejar paquetes de parámetros y sus expansiones.
- [ ] T13-015. Respetar especializaciones explícitas y parciales seleccionadas por Clang.
- [ ] T13-016. Conservar diagnósticos de sustitución y restricciones de templates.
- [ ] T13-017. Resolver restricciones y concepts mediante el análisis semántico correspondiente.
- [ ] T13-018. Evitar generar varias definiciones incompatibles para una misma especialización.
- [ ] T13-019. Auditar la selección COMDAT de NSLink y del enlazador utilizado en compilaciones mixtas.
- [ ] T13-020. Comprobar la misma especialización solicitada desde dos módulos NativeSharp.
- [ ] T13-021. Definir cómo se exponen resultados `constexpr` evaluados por Clang.
- [ ] T13-022. Definir cómo se consumen constantes y expresiones sujetas a evaluación en compilación.
- [ ] T13-023. Probar una plantilla de biblioteca con un tipo C++ como argumento.
- [ ] T13-024. Probar una plantilla con un struct NativeSharp trivial como argumento.
- [ ] T13-025. Probar una plantilla con un struct NativeSharp de vida no trivial.

## 14. Preprocesador, declaraciones especiales y módulos

- [ ] T14-001. Clasificar macros de constante, macros de función y macros sin proyección directa.
- [ ] T14-002. Definir cómo se importan valores de macros de constante verificables.
- [ ] T14-003. Definir el tratamiento de macros de función que necesitan expansión C/C++.
- [ ] T14-004. Respetar las condiciones de preprocesador de la configuración seleccionada.
- [ ] T14-005. Reflejar cambios de macros en la identidad de la sesión y su caché.
- [ ] T14-006. Investigar cómo importar bitfields sin alterar su disposición ni sus reglas de acceso.
- [ ] T14-007. Definir la representación de punteros a miembros de C++.
- [ ] T14-008. Definir cómo tratar atributos C/C++ que alteren ABI, alineación o llamadas.
- [ ] T14-009. Investigar el consumo de headers precompilados sin mezclar configuraciones.
- [ ] T14-010. Diseñar el consumo de módulos C++ compatibles con la versión fijada de Clang.
- [ ] T14-011. Comprobar que las dependencias transitivas de headers y módulos invalidan resultados obsoletos.

## 15. Excepciones, callbacks y recursos

- [ ] T15-001. Auditar el mecanismo de errores, limpieza y excepciones que implementa NSC.
- [ ] T15-002. Identificar el modelo de excepciones empleado por Clang en el objetivo Windows seleccionado.
  - Avance parcial: una prueba Clang Windows x64 emite `_CxxThrowException` y `__CxxFrameHandler3` para `throw`/`catch(int)`; falta validar variantes y cruce con NSC. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-excepciones-c-en-windows-x64).
- [ ] T15-003. Diseñar la propagación o adaptación de excepciones C++ al entrar en NativeSharp.
- [ ] T15-004. Diseñar la limpieza de objetos locales durante el desenrollado de pila.
- [ ] T15-005. Definir el comportamiento de funciones declaradas `noexcept`.
- [ ] T15-006. Comprobar el desenrollado a través de frames generados por NSC.
  - Avance parcial: una excepción C++ entera atravesó un frame NS; `finally` y destructor `raii` se ejecutaron una vez en O0 y O2, antes de ser capturada por el arnés C++. [Evidencia y límites](SCOPE-LOCK-EJECUCION.md#investigación-excepciones-c-en-windows-x64).
- [ ] T15-007. Comprobar la destrucción de recursos al propagarse una excepción desde una DLL.
  - Avance parcial: una DLL lanzó un entero y tipos C++ trivial y con destructor; `raii` NS se destruyó una vez en O0 y O2 y el destructor C++ se observó tras el `catch`. Un control negativo detectó la falta de limpieza. Faltan otros recursos y contratos. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-excepciones-c-en-windows-x64).
- [ ] T15-008. Diseñar cómo pasar una función NativeSharp como callback esperado por una biblioteca.
- [ ] T15-009. Emitir el adaptador necesario para un callback sin capturas.
- [ ] T15-010. Definir la representación y vida de capturas para callbacks con estado.
- [ ] T15-011. Comprobar un callback ejecutado durante una llamada C++.
- [ ] T15-012. Comprobar un callback conservado por la biblioteca después de la llamada inicial.
- [ ] T15-013. Definir el tratamiento de excepciones que crucen el límite de un callback.
- [ ] T15-014. Importar y utilizar smart pointers sin perder sus reglas de propiedad.
- [ ] T15-015. Definir la interoperación con allocators y sus instancias de estado.
- [ ] T15-016. Comprobar la transferencia de propiedad con tipos movibles y no copiables.
- [ ] T15-017. Diseñar la proyección de corutinas y de la vida útil de sus frames.
- [ ] T15-018. Comprobar suspensión, reanudación y liberación de una corutina importada.

## 16. Enlace, arranque y ejecución de bibliotecas

- [ ] T16-001. Determinar el punto de entrada real de un ejecutable NativeSharp enlazado con C++.
- [ ] T16-002. Identificar qué objetos de arranque y bibliotecas participan en el enlace.
- [ ] T16-003. Diseñar la inicialización de constructores globales C++ antes de su uso desde NativeSharp.
- [ ] T16-004. Diseñar la ejecución de destructores y callbacks de salida al terminar el proceso.
- [ ] T16-005. Comprobar constructores y destructores globales con observación independiente del CRT.
- [ ] T16-006. Identificar la inicialización de TLS requerida por las bibliotecas importadas.
- [ ] T16-007. Comprobar datos TLS desde más de un hilo.
- [ ] T16-008. Definir el orden de inicialización del runtime NativeSharp y el runtime C++.
- [ ] T16-009. Auditar el estado del runtime NativeSharp cuando intervienen varias DLL.
- [ ] T16-010. Definir la política de propiedad de memoria entre ejecutable y DLL.
- [ ] T16-011. Definir la política para recursos del CRT que atraviesan límites de DLL.
- [ ] T16-012. Comprobar carga y descarga de una DLL con objetos globales C++.
- [ ] T16-013. Revisar operaciones de inicialización que ocurran dentro de `DllMain` y sus restricciones.
- [ ] T16-014. Comprobar enlazado separado de varias unidades C++ y módulos NativeSharp.
- [ ] T16-015. Comprobar símbolos duplicados y selección de definiciones conforme a la política elegida.
- [ ] T16-016. Investigar la interoperación con LTO sin imponerla como requisito de la primera tanda.
- [ ] T16-017. Definir el tratamiento de operaciones atómicas y garantías de concurrencia compartidas.

## 17. Diagnósticos, depuración y experiencia de uso

- [ ] T17-001. Señalar la declaración C/C++ original cuando una importación falle.
- [ ] T17-002. Señalar el uso NativeSharp que solicita una operación inválida.
- [ ] T17-003. Mostrar las alternativas relevantes cuando falle una resolución de sobrecarga.
- [ ] T17-004. Preservar las notas de instanciación cuando falle un template.
- [ ] T17-005. Distinguir un header ausente de una biblioteca binaria ausente.
- [ ] T17-006. Distinguir un símbolo no encontrado de una incompatibilidad de ABI detectada.
- [ ] T17-007. Investigar cómo relacionar ubicaciones NativeSharp y C++ durante la depuración.
- [ ] T17-008. Comprobar la información de depuración en una llamada que cruza el límite de lenguaje.
- [ ] T17-009. Evitar reutilizar una sesión o caché con opciones de compilación diferentes.
- [ ] T17-010. Medir el costo de análisis repetido de headers con una biblioteca representativa.
- [ ] T17-011. Diseñar la reutilización segura de información solo cuando la medición y la corrección lo permitan.
- [ ] T17-012. Documentar cómo declarar y usar una biblioteca desde un proyecto NativeSharp.

## 18. Cobertura, pruebas integrales y cierre

- [ ] T18-001. Establecer una versión explícita del lenguaje C que cubre la línea base.
- [ ] T18-002. Establecer una versión explícita del lenguaje C++ que cubre la línea base.
- [ ] T18-003. Elaborar un inventario de características y combinaciones relevantes a partir de las fuentes del lenguaje y de Clang.
- [ ] T18-004. Relacionar cada entrada del inventario con sus reglas de tipos, ABI y vida útil aplicables.
- [ ] T18-005. Asociar cada entrada del inventario con una prueba o con una investigación pendiente.
- [ ] T18-006. Revisar las combinaciones entre características; no contar solo tipos de nodos del AST.
- [ ] T18-007. Registrar cada rechazo o limitación como pendiente, no como interoperabilidad terminada.
- [ ] T18-008. Seleccionar una biblioteca C real según los contratos que ejercite.
- [ ] T18-009. Compilar esa biblioteca con Clang oficial y consumirla desde NativeSharp.
- [ ] T18-010. Seleccionar una biblioteca C++ real con compilación separada y vida de objetos no trivial.
- [ ] T18-011. Compilar esa biblioteca con Clang oficial y consumirla desde NativeSharp.
- [ ] T18-012. Seleccionar una biblioteca basada en headers que ejercite templates.
- [ ] T18-013. Consumir desde NativeSharp especializaciones de esa biblioteca.
- [ ] T18-014. Ejecutar las pruebas de regresión de NSC ajenas a interoperabilidad.
- [ ] T18-015. Ejecutar las pruebas de interop con enlace estático.
- [ ] T18-016. Ejecutar las pruebas de interop con DLL.
- [ ] T18-017. Inspeccionar las dependencias de distribución del compilador resultante.
- [ ] T18-018. Comprobar el funcionamiento del paquete previsto en un entorno limpio compatible.
- [ ] T18-019. Documentar qué headers, bibliotecas y herramientas necesita el paquete para compilar programas.
- [ ] T18-020. Registrar las versiones y comandos de reproducción de todas las pruebas integrales.
- [ ] T18-021. Revisar los pendientes descubiertos y agregar tareas específicas antes de considerarlos cubiertos.
- [ ] T18-022. Publicar un informe de capacidades demostradas y limitaciones restantes.
- [ ] T18-023. Declarar terminado el objetivo solo cuando el inventario acordado y sus combinaciones estén cubiertos con evidencia.
