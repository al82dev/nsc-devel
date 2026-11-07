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

El validador integral incluye los scripts de la tanda inventariados. La última
ejecución integral registrada terminó con cuatro fallos en controles TLS de DLL
C; desde entonces se añadieron controles dirigidos sin repetir el gate. La tanda
sigue abierta y la siguiente ejecución integral queda pendiente tras agrupar
las correcciones. [Resultado](registro-commits/t08-validacion-link-lib-auto.md),
[cobertura](registro-commits/cobertura-validador-tanda-08.md).

- [x] T08-001. Auditar la representación y búsqueda de nombres actual de NSC. Evidencia: [auditoría de tipos C](TANDA-08-TIPOS-C.md).
- [ ] T08-002. Definir una identidad estable para cada declaración C importada.
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
- [ ] T08-003. Importar declaraciones de funciones C desde el AST de Clang.
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
- [ ] T08-004. Importar declaraciones de variables globales C.
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
- [ ] T08-005. Importar alias de tipos declarados con `typedef`.
  - El puntero `const CMatrix*` y su variante mutable llegaron a funciones C y
    ejecutaron O0–O3; se rechazaron `const` y dimensiones incompatibles.
    La suite de sesión pasó. Otras clases de matrices siguen pendientes.
    [Evidencia](registro-commits/proyeccion-puntero-matriz-c.md).
  - Un proyecto real pasó de O0 a O3 con alias escalares encadenados, matriz
    local, enum, struct y callback. Una función `const CMatrix*` sigue sin
    importarse y tiene prueba separada. [Evidencia](registro-commits/proyecto-typedef-c.md).
- [ ] T08-006. Mapear los tipos enteros C con su tamaño y signo efectivos.
  - La prueba mixta `_Bool` de campo, arreglo y llamada pasó O0–O3. Es una
    muestra, no el cierre del ítem. [Evidencia](TANDA-08-TIPOS-C.md#validación-dirigida-agrupada-identidades-y-_bool).
  - `8be3c138f` prepara un control mixto de campo y arreglo `_Bool`, con
    tamaño, alineación, offsets, lectura y escritura. No ejecutado todavía.
  - `fe2fddfca` añade argumentos y resultados `_Bool` en llamada C; pendiente
    de ejecución, no demuestra aún su ABI.
- [ ] T08-007. Mapear los tipos de punto flotante según el objetivo.
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
- [ ] T08-008. Mapear el tipo `void` en parámetros y resultados donde corresponda.
  - La validación agrupada del fixture de records pasó los casos preparados de
    globales y arreglos C `volatile void*`, con los controles negativos incluidos.
    [Detalle](registro-commits/validacion-agrupada-volatile-08.md).
  - La muestra C11 pasó retorno `void`, parámetro y retorno `void*` y prototipo
    de cero argumentos en O0–O3, con rechazo de argumento extra. No cubre
    todas las posiciones o combinaciones. [Evidencia](registro-commits/prueba-prototipos-c.md).
- [ ] T08-009. Mapear enumeraciones y sus valores representables.
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
- [ ] T08-010. Importar la declaración y campos de un `struct` C.
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
- [ ] T08-011. Importar la declaración y campos de una `union` C.
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
- [ ] T08-012. Obtener de Clang tamaño, alineación y desplazamiento de cada campo.
- [ ] T08-013. Representar tipos declarados pero incompletos sin inventar su tamaño.
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
- [ ] T08-014. Mapear punteros y su tipo apuntado.
- [ ] T08-015. Conservar los calificadores de tipo que afecten al acceso.
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
- [ ] T08-016. Mapear arreglos de tamaño conocido.
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
- [ ] T08-017. Definir la representación de punteros a función C.
  - Productor y consumidores precompilados para callback almacenado con enum pasaron O0–O3 con y sin referencia explícita al header; se ocultó solo la copia de salida. [Evidencia](registro-commits/enum-c-reconciliado-campo.md).
  - El callback almacenado que recibe y devuelve enum C pasó el fixture de campo/global O0–O3 y la suite de sesión. [Evidencia](registro-commits/callback-enum-precompilado.md).
  - Un callback global con record usado solo en su firma se importó y permitió resolver un campo mediante `var`; llegó a IR, con ejecución O0–O3 pendiente. [Detalle](registro-commits/callback-global-record-inferido.md).
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
- [ ] T08-018. Resolver colisiones entre nombres importados y nombres NativeSharp.
  - Un caso real de sombra local de global y enumerador C pasó O0, con comprobación de resolución después del bloque y del valor de C. [Detalle](registro-commits/sombra-local-valores-c.md).
- [ ] T08-019. Presentar diagnósticos útiles al usar una declaración incompleta de forma inválida.
  - Los diagnósticos para `+`, `-`, `+=`, incremento e índice sobre records C incompletos pasaron sus casos negativos; el control `Opaque**` pasó O0–O3. [Detalle](registro-commits/aritmetica-puntero-incompleto-c.md).
  - El local C incompleto por valor produce un diagnóstico específico; la
    prueba anterior de `sizeof` también pasó. Faltan otras operaciones y
    combinaciones. [Evidencia](registro-commits/diagnostico-local-incompleto-c.md).
- [ ] T08-020. Comparar tamaño, alineación y offsets importados con los calculados por Clang.
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

## 09. Llamadas, enlace y datos C

- [ ] T09-001. Auditar las representaciones HIR y MIR que usa NSC para llamadas externas.
- [ ] T09-002. Auditar la emisión actual de llamadas C en el generador de código.
- [ ] T09-003. Conservar el nombre de símbolo y la convención de llamada elegidos por Clang.
- [ ] T09-004. Emitir una llamada C sin parámetros y con resultado escalar.
- [ ] T09-005. Emitir una llamada C con parámetros escalares.
- [ ] T09-006. Emitir el paso de punteros a una función C.
- [ ] T09-007. Emitir el paso de agregados por valor conforme al ABI objetivo.
- [ ] T09-008. Emitir la recepción de agregados devueltos conforme al ABI objetivo.
- [ ] T09-009. Leer una variable global C importada.
- [ ] T09-010. Escribir una variable global C importada cuando su declaración lo permita.
- [ ] T09-011. Diseñar las promociones y el paso de argumentos de funciones C variádicas.
- [ ] T09-012. Emitir llamadas variádicas con las promociones correctas.
- [x] T09-013. Enlazar una llamada contra una biblioteca estática de prueba. Evidencia: [proyecto mixto por consola](SCOPE-LOCK-EJECUCION.md#proyecto-mixto-por-consola); se conserva la limitación de caché.
- [x] T09-014. Enlazar y ejecutar una llamada contra una DLL de prueba. Evidencia: [proyecto C con DLL](SCOPE-LOCK-EJECUCION.md#presupuesto-función-libre-importada-de-dll); llamada escalar sin caché de artefactos.
- [ ] T09-015. Comprobar que los errores de enlace indiquen la biblioteca o símbolo faltante.
- [ ] T09-016. Contrastar los resultados de llamadas y datos con un consumidor C compilado por Clang.

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
