# Tracking: interoperabilidad directa de C y C++ con NativeSharp

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

- [ ] T04-001. Crear la implementación privada propuesta `ClangSession.cpp`.
- [ ] T04-002. Crear el constructor de invocación propuesto `ClangInvocationBuilder.cpp`.
- [ ] T04-003. Traducir el modo C o C++ a los argumentos correspondientes de Clang.
- [ ] T04-004. Traducir la versión de lenguaje a la invocación de Clang.
- [ ] T04-005. Traducir el triple objetivo a la invocación de Clang.
- [ ] T04-006. Incorporar cada directorio de inclusión a la invocación.
- [ ] T04-007. Incorporar cada definición de macro a la invocación.
- [ ] T04-008. Incorporar el directorio de recursos explícito a la invocación.
- [ ] T04-009. Incorporar las rutas configuradas de headers del sistema a la invocación.
- [ ] T04-010. Rechazar una configuración que no identifique los recursos necesarios.
- [ ] T04-011. Crear el consumidor de diagnósticos privado `ClangDiagnosticConsumer.cpp`.
- [ ] T04-012. Copiar la severidad de cada diagnóstico Clang al formato de sesión.
- [ ] T04-013. Copiar el mensaje y sus ubicaciones al formato de sesión.
- [ ] T04-014. Conservar las notas asociadas con su diagnóstico principal.
- [ ] T04-015. Asegurar que el consumidor viva más que el motor de diagnósticos que lo utiliza.
- [ ] T04-016. Crear el `CompilerInstance` con la configuración construida.
- [ ] T04-017. Inicializar los diagnósticos de `CompilerInstance` con propiedad explícita.
- [ ] T04-018. Crear la acción de frontend propuesta `ClangSessionAction.cpp`.
- [ ] T04-019. Llamar a `BeginSourceFile` una sola vez por apertura de unidad.
- [ ] T04-020. Omitir `EndSourceFile` si `BeginSourceFile` falla.
- [ ] T04-021. Ejecutar la acción de frontend sin repetir la inicialización de la unidad.
- [ ] T04-022. Consultar los diagnósticos de error además del resultado de ejecución de Clang.
- [ ] T04-023. Mantener vivos el AST y el estado semántico durante la vida útil necesaria de la sesión.
- [ ] T04-024. Cerrar la unidad exactamente una vez después de una apertura exitosa.
- [ ] T04-025. Liberar los recursos de sesión en el orden compatible con sus dependencias.
- [ ] T04-026. Evitar una finalización global de LLVM al cerrar una sesión individual.
- [ ] T04-027. Comprobar la cancelación antes de iniciar el análisis de una unidad.
- [ ] T04-028. Propagar la cancelación durante los puntos de trabajo que Clang permita interrumpir.
- [ ] T04-029. Evitar que dos ejecutores accedan simultáneamente al mismo contexto sin coordinación.
- [ ] T04-030. Mantener independientes las opciones y los recursos de sesiones distintas.

## 05. Incorporación al flujo de NSC

- [ ] T05-001. Localizar la creación actual de `ProjectCompilation` en cada ruta de compilación.
- [x] T05-002. Añadir el punto de inyección opcional de sesión en `ProjectCompilation.h`. Evidencia: [prueba de inyección](LINEA-BASE-TANDAS.md).
- [x] T05-003. Implementar en `ProjectCompilation.cpp` el almacenamiento de la sesión inyectada. Evidencia: [prueba de inyección](LINEA-BASE-TANDAS.md).
- [x] T05-004. Añadir un acceso controlado a la sesión desde las fases que la requieran. Evidencia: [prueba de inyección](LINEA-BASE-TANDAS.md).
- [ ] T05-005. Definir el orden de destrucción entre `ProjectCompilation` y la sesión.
- [ ] T05-006. Integrar la configuración opcional en la ruta correspondiente de `DriverExecution.cpp`.
- [ ] T05-007. Revisar la creación de compilaciones desde el servicio de lenguaje.
- [ ] T05-008. Preservar la vida útil de la sesión cuando existan snapshots de `ProjectCompilation`.
- [ ] T05-009. Crear `CppDiagnosticAdapter.cpp` en la capa frontend, sin dependencia inversa del target interop.
- [ ] T05-010. Convertir las severidades de sesión a severidades de NSC.
- [ ] T05-011. Convertir ubicaciones y notas de sesión a diagnósticos de NSC.
- [ ] T05-012. Conservar el comportamiento actual si no se solicita una sesión C/C++.
- [ ] T05-013. Informar un error claro si se solicita interop en una construcción que lo tiene desactivado.
- [ ] T05-014. Evitar crear una sesión por un fallo anterior que impida su uso.

## 06. Verificación de la primera tanda

- [ ] T06-001. Registrar el módulo de pruebas de sesión en la infraestructura de pruebas que corresponda.
- [ ] T06-002. Asegurar que los temporales de esas pruebas se crean dentro de A:.
- [x] T06-003. Comprobar que una sesión C abre una unidad con `bridge_probe` y usa los recursos de Clang. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [x] T06-004. Comprobar que una sesión C++ abre una unidad con una clase de prueba. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [ ] T06-005. Comprobar el diagnóstico de una versión de Clang incompatible.
- [ ] T06-006. Comprobar que dos configuraciones diferentes producen identidades distintas.
- [x] T06-007. Comprobar el fallo por recursos de Clang ausentes. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [x] T06-008. Comprobar que una unidad inválida entrega ubicación y mensaje de error. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [ ] T06-009. Comprobar que un fallo de `BeginSourceFile` no intenta finalizar una unidad no abierta.
- [x] T06-010. Comprobar que cerrar dos veces no libera recursos dos veces. Evidencia: [sesión real](LINEA-BASE-TANDAS.md).
- [ ] T06-011. Comprobar la cancelación y la liberación posterior de recursos.
- [ ] T06-012. Comprobar que dos sesiones con opciones distintas no se contaminan.
- [x] T06-013. Comprobar la inyección de una sesión en `ProjectCompilation`. Evidencia: [pruebas con sesión simulada y sesión Clang real](LINEA-BASE-TANDAS.md).
- [ ] T06-014. Comprobar una compilación NativeSharp normal con la integración desactivada.
- [ ] T06-015. Comprobar una compilación NativeSharp normal con la integración activada y sin sesión solicitada.
- [ ] T06-016. Inspeccionar importaciones normales y diferidas de los ejecutables producidos.
- [ ] T06-017. Registrar resultados, comandos y limitaciones de la primera tanda.

## 07. Entrada de bibliotecas y modelo de importación

- [ ] T07-001. Auditar cómo `ProjectLoader.cpp` lee actualmente referencias de bibliotecas.
- [ ] T07-002. Auditar cómo `Project.h` conserva las opciones del proyecto.
- [ ] T07-003. Auditar cómo el grafo del proyecto propaga dependencias entre módulos.
- [ ] T07-004. Auditar qué datos de entrada conserva el manifiesto de compilación actual.
- [ ] T07-005. Diseñar cómo declarar que una dependencia expone headers C o C++.
- [ ] T07-006. Diseñar cómo indicar el header principal de una dependencia.
- [ ] T07-007. Diseñar cómo especificar sus directorios de inclusión.
- [ ] T07-008. Diseñar cómo especificar sus macros y opciones de lenguaje.
- [ ] T07-009. Diseñar cómo especificar bibliotecas estáticas de enlace.
- [ ] T07-010. Diseñar cómo especificar bibliotecas de importación y DLL de ejecución.
  - Avance parcial: `CppLibraryReference` permite declarar ambas y una biblioteca NativeSharp estática puede registrar y empaquetar las dependencias declaradas. Se valida la correspondencia para entradas COFF cortas y se probó un archivo que importa dos DLL encadenadas, con consumidor ON y OFF. Pendientes los formatos de importación larga, otras topologías multidll y el cierre completo de dependencias transitivas. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-incremental-archivo-de-importación-con-varias-dll-2026-11-05).
  - Importación larga: se lee el nombre del descriptor en el caso AMD64 probado, pero se rechaza el enlace porque el PE de NSC omitió la DLL. La barrera también cubre el fixture mixto de entradas largas y cortas; falta resolver el enlace. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-incremental-importación-coff-larga-de-estilo-ms-2026-11-05).
  - Caché multidll: se observó un acierto y se rechazaron la DLL alterada o ausente y el `.lib` alterado. Tras actualizar la DLL, la biblioteca de importación y NSMETA, se observó una clave nueva y el comportamiento cambió de 42 a 47; quedan otras topologías por verificar. [Evidencia](SCOPE-LOCK-EJECUCION.md#investigación-incremental-actualizar-dll-y-biblioteca-de-importación-multidll-2026-11-05).
- [ ] T07-011. Validar las opciones de importación antes de crear la sesión Clang.
- [ ] T07-012. Conservar la procedencia de cada declaración importada.
- [ ] T07-013. Hacer disponibles los diagnósticos del header en la compilación NativeSharp.
- [ ] T07-014. Registrar los archivos incluidos para detectar cambios en dependencias.
- [ ] T07-015. Comprobar la carga de dos bibliotecas con configuraciones independientes.
- [ ] T07-016. Comprobar que cambiar un header invalida el resultado que dependa de él.

## 08. Declaraciones, nombres y tipos C

- [ ] T08-001. Auditar la representación y búsqueda de nombres actual de NSC.
- [ ] T08-002. Definir una identidad estable para cada declaración C importada.
- [ ] T08-003. Importar declaraciones de funciones C desde el AST de Clang.
- [ ] T08-004. Importar declaraciones de variables globales C.
- [ ] T08-005. Importar alias de tipos declarados con `typedef`.
- [ ] T08-006. Mapear los tipos enteros C con su tamaño y signo efectivos.
- [ ] T08-007. Mapear los tipos de punto flotante según el objetivo.
- [ ] T08-008. Mapear el tipo `void` en parámetros y resultados donde corresponda.
- [ ] T08-009. Mapear enumeraciones y sus valores representables.
- [ ] T08-010. Importar la declaración y campos de un `struct` C.
- [ ] T08-011. Importar la declaración y campos de una `union` C.
- [ ] T08-012. Obtener de Clang tamaño, alineación y desplazamiento de cada campo.
- [ ] T08-013. Representar tipos declarados pero incompletos sin inventar su tamaño.
- [ ] T08-014. Mapear punteros y su tipo apuntado.
- [ ] T08-015. Conservar los calificadores de tipo que afecten al acceso.
- [ ] T08-016. Mapear arreglos de tamaño conocido.
- [ ] T08-017. Definir la representación de punteros a función C.
- [ ] T08-018. Resolver colisiones entre nombres importados y nombres NativeSharp.
- [ ] T08-019. Presentar diagnósticos útiles al usar una declaración incompleta de forma inválida.
- [ ] T08-020. Comparar tamaño, alineación y offsets importados con los calculados por Clang.

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
