# Tanda 08: seguimiento detallado de interoperabilidad C

Esta vista desglosa los 20 ítems de la [tanda 08](TRACKING.md#08-declaraciones-nombres-y-tipos-c). Los identificadores originales permanecen vigentes. Cada casilla marcada describe **únicamente el caso y la validación indicados**, no la cobertura completa de su categoría. Las casillas vacías son trabajo o validación pendientes; no equivalen necesariamente a código inexistente.

La evidencia histórica y sus revisiones se conserva en [TRACKING.md](TRACKING.md). Un resultado antiguo puede haber sido ampliado después; ante discrepancias debe contrastarse con el código, el registro enlazado y el gate actual. La validación integral de la tanda sigue pendiente. Esta lista no debe interpretarse como un inventario cerrado de todas las variantes de C: las combinaciones aún no inventariadas siguen incluidas en los ítems originales.

Los identificadores con sufijo (`a`, `b`...) desglosan la casilla anterior del mismo número; su estado anterior se conserva como encabezado, sin contar una segunda tarea. Una casilla pendiente de comprobación no afirma que la capacidad esté ausente: indica que falta cotejar la evidencia o completar su implementación si la comprobación falla.

**Decisión vigente del usuario:** NativeSharp todavía no está liberado. Se trabaja con un único compilador y el formato NSMETA vigente; la compatibilidad con versiones históricas de NSMETA no es un objetivo de esta tanda. No introducir versiones nuevas ni conservar compatibilidad histórica por iniciativa propia. Las notas históricas se mantienen como evidencia y no son requisitos activos.

## T08-001 — Representación y nombres

- [x] T08-001-01: Auditar la representación y búsqueda de nombres existentes. [Auditoría](TANDA-08-TIPOS-C.md).

## T08-002 — Identidad de declaraciones

- [x] T08-002-01: Comprobar estabilidad de USR de declaraciones C entre sesiones. [Auditoría](T08-002-IDENTIDAD-AUDITORIA.md).
- [x] T08-002-02: Comprobar restauración de USR de records y enums mediante el driver precompilado. [Prueba](registro-commits/t08-002-ruta-driver-c.md).
- [x] T08-002-03: Ejercitar encabezados homónimos de directorios distintos. [Prueba](registro-commits/t08-002-homonimos-c.md).
- [x] T08-002-04: Comprobar casos de typedef C equivalentes y rechazos por incompatibilidad. [Evidencia](registro-commits/t08-h1-typedef-c-equivalentes.md).
- [x] T08-002-05: Comprobar conciliación de records compatibles entre encabezados y metadata. [Evidencia](registro-commits/t08-h5-records-c-precompilado.md).
- [x] T08-002-06: Comprobar muestras de dependencias C en plantillas precompiladas. [Evidencia](registro-commits/t08-h3-identidad-expectativas.md).
- T08-002-07 y T08-002-08: **Retirados del alcance por decisión del usuario.**
- **T08-002-09 — Identidad y conciliación:**
- [x] T08-002-09a: Cotejar una función C externa con firma idéntica declarada en dos encabezados: Clang aporta el mismo USR y NSC agrupa ambas declaraciones. [Prueba](registro-commits/t08-002-pruebas-identidad-c.md), [resultado](registro-commits/t08-002-resultados-identidad-c.md).
- [x] T08-002-09b: Cotejar funciones `static inline` distintas en encabezados homónimos: el USR coincide, pero NSC conserva dos grupos y diagnostica ambigüedad en la muestra comprobada. [Evidencia](registro-commits/t08-002-homonimos-c.md).
- [x] T08-002-09c: Cotejar los USR de globales externas y `static` entre encabezados; la sonda homónima observó el mismo USR para las `static`. [Evidencia](registro-commits/t08-002-homonimos-c.md).
- [x] T08-002-09c1: Verificar selección de globales `static` de encabezados homónimos: una sola se acepta y dos se diagnostican como ambiguas, con contenido igual o distinto. [Evidencia](registro-commits/t08-identidad-global-static-homonima.md).
- [x] T08-002-09d: Verificar en análisis directo muestras de typedefs que apuntan a records o enums incompatibles. [Evidencia](registro-commits/t08-identidad-typedef-record-enum-conflicto.md).
- [x] T08-002-09d1: Verificar muestras de conflictos de identidad mediante bibliotecas precompiladas en O0, con y sin fuente original. [Evidencia](registro-commits/t08-typedef-precompilado-fuente-ausente.md).
  - Resuelto para el record completo con fuentes disponibles: encabezados distintos equivalentes se concilian y un record incompatible se rechaza. [Evidencia](registro-commits/t08-typedef-precompilado-conciliacion.md).
  - Resuelto para el enum completo con fuentes disponibles: encabezados equivalentes se concilian y valores incompatibles se rechazan. [Evidencia](registro-commits/t08-typedef-enum-precompilado-conciliacion.md).
- [x] T08-002-09d2: Comprobar paso y retorno de un valor de enum con typedefs conciliados entre biblioteca precompilada y encabezado diferente, en O0 y con y sin fuente original. [Evidencia](registro-commits/t08-typedef-enum-valor-precompilado.md).
- [x] T08-002-09e: Verificar la identidad de los campos y enumeradores de la muestra de records anónimos anidados al restaurarse en NSC. [Evidencia](registro-commits/t08-identidad-anonimos-restaurados.md).
  - [x] T08-002-09e1: Verificar los USR de Clang de una muestra, su presencia en la biblioteca y ejecutar accesos directos y precompilados en O0. [Evidencia](registro-commits/t08-identidad-anonimos-publicacion.md).
  - [x] T08-002-09e2: Comparar los USR de los campos y enumeradores con los que NSC restaura para esa muestra mediante el driver.
- [x] T08-002-09f: Verificar una muestra de firmas de función externa C equivalentes e incompatibles en dos encabezados. [Evidencia](registro-commits/t08-firma-externa-c-conflicto.md).
- [x] T08-002-09g: Verificar una muestra de funciones `static inline` idénticas en encabezados homónimos de directorios distintos. [Evidencia](registro-commits/t08-inline-homonima-identica.md).
- [x] T08-002-09h: Verificar que dos bibliotecas con funciones `static inline` homónimas conservan implementaciones distintas al consumir sus plantillas, en O0. [Evidencia](registro-commits/t08-inline-dos-bibliotecas.md).
- [x] T08-002-09i: Preservar la identidad de globales C `static` homónimas usadas por plantillas de dos bibliotecas precompiladas, en O0. [Reproducción](registro-commits/t08-static-dos-bibliotecas.md), [corrección](registro-commits/t08-static-dos-bibliotecas-correccion.md).
- [x] T08-002-09j: Comprobar lecturas y modificaciones independientes de dos globales C internas homónimas mediante plantillas precompiladas, en O0–O3. [Evidencia](registro-commits/t08-static-dos-bibliotecas-mutables.md).
- [x] T08-002-09k: Comprobar las mismas operaciones con dos productores referenciados como proyectos fuente, en O0. [Evidencia](registro-commits/t08-static-proyectos-fuente.md).
- [x] T08-002-09l: Conciliar una global C externa compartida por plantillas de dos bibliotecas precompiladas y rechazar la muestra incompatible `int`/`long`, en O0. [Reproducción](registro-commits/t08-externa-dos-bibliotecas.md), [corrección](registro-commits/t08-externa-dos-bibliotecas-correccion.md).
- [x] T08-002-09m1: Verificar una global externa `volatile` compartida y rechazar una redeclaración sin `volatile`, en O0. [Evidencia](registro-commits/t08-global-externa-volatile.md).
- [x] T08-002-09m2: Verificar una global externa `const` compartida y rechazar una redeclaración sin `const`, en O0. [Evidencia](registro-commits/t08-global-externa-const.md).
- [x] T08-002-09m3: Verificar un puntero externo compartido y rechazar discrepancias `const` o `volatile` en el dato apuntado, en O0. [Evidencia](registro-commits/t08-global-externa-puntero-calificadores.md).
- [x] T08-002-09m4: Verificar lectura y actualización atómicas de una global C externa compartida y rechazar discrepancia de `_Atomic`, en O0. [Evidencia](registro-commits/t08-global-externa-atomica.md).
- [x] T08-002-09m5: Publicar y consumir una global C externa TLS usada por plantillas de dos bibliotecas sin encabezados originales, en O0–O3. [Evidencia](registro-commits/t08-tls-externa-plantillas.md), [O0–O3](registro-commits/t08-tls-plantillas-optimizaciones.md).
  - [x] T08-002-09m5a: Comprobar el aislamiento de la global externa compartida entre el hilo principal y otro hilo real, en O0–O3. [Evidencia](registro-commits/t08-tls-externa-hilos.md).
- [x] T08-002-09m6: Publicar y consumir dos globales C TLS internas desde plantillas precompiladas sin encabezados, en O0–O3. [Evidencia](registro-commits/t08-tls-interna-plantillas.md), [O0–O3](registro-commits/t08-tls-plantillas-optimizaciones.md).
  - [x] T08-002-09m6a: Comprobar aislamiento entre hilo principal y un hilo real para las dos globales TLS internas, en O0–O3. [Evidencia](registro-commits/t08-tls-interna-hilos.md).
- [x] T08-002-09m7a: Verificar TLS de DLL usada por plantilla precompilada desde dos hilos sin el encabezado original, en O0–O3. [Evidencia](registro-commits/t08-tls-dll-plantillas.md), [O0–O3](registro-commits/t08-tls-dll-dos-productores.md).
- [x] T08-002-09m7b: Verificar la misma TLS de DLL compartida por dos productores y aislada entre dos hilos, en O0–O3. [Evidencia](registro-commits/t08-tls-dll-dos-productores.md).
- [x] T08-002-09m7c: Conservar la identidad de TLS homónima procedente de DLL de nombres diferentes y comprobar aislamiento por hilo en O0–O3. [Evidencia](registro-commits/t08-tls-dll-resolucion.md).
  - [ ] T08-002-09m7c1: Admitir dos DLL diferentes con el mismo nombre de archivo procedentes de directorios diferentes; requieren una identidad y carga por ruta que preserven ambas bibliotecas.
- [ ] T08-002-09m7d: Comprobar los demás calificadores y variantes de enlace de la global externa compartida.
- [x] T08-002-09n: Enlazar una sola vez una biblioteca C idéntica propagada por dos productores y conservar dos bibliotecas distintas con el mismo nombre, en O0. [Evidencia](registro-commits/t08-dependencia-c-compartida.md).
- **T08-002-10 — Sombras en plantillas:**
- [x] T08-002-10a: Verificar una muestra de sombra de función C por parámetro y local en una plantilla precompilada en O0. [Evidencia](registro-commits/t08-sombra-funcion-parametro-local.md).
- [x] T08-002-10b: Verificar una muestra de sombra de global C por variables de `foreach` y `catch`, en O0. [Evidencia](registro-commits/t08-sombra-foreach-catch-o0.md).
- [x] T08-002-10c: Verificar muestras de inicializadores de local, `for` y desestructuración antes de introducir el nombre sombreado, en O0. [Evidencia](registro-commits/t08-inicializadores-ambito-o0.md).
- **T08-002-11 — Publicación y conflictos:**
- [x] T08-002-11a: Validar una función C requerida por plantilla precompilada con encabezado, en O0. [Evidencia](registro-commits/t08-plantilla-funcion-c.md).
- [x] T08-002-11b: Validar la misma función tras retirar el encabezado del productor, en O0. [Evidencia](registro-commits/t08-plantilla-funcion-c.md).
- [x] T08-002-11c: Validar muestras de global C y typedef requeridos por plantilla sin encabezado, en O0. [Evidencia](registro-commits/t08-plantilla-global-alias-c.md).
- [x] T08-002-11d: Diagnosticar una firma de función C incompatible entre biblioteca precompilada y encabezado visible cuando se demandan ambas. [Evidencia](registro-commits/t08-plantilla-funcion-conflicto.md).
- [x] T08-002-11e: Verificar una declaración incompatible visible no demandada directamente: la muestra compila y ejecuta solo la llamada de la plantilla, en O0. [Evidencia](registro-commits/t08-funcion-incompatible-no-demandada.md).
- **T08-002-12 — Ámbitos adicionales:**
- [x] T08-002-12a: Verificar muestras de sombra por parámetro de lambda y de inicializador local dentro de lambda, en O0. [Evidencia](registro-commits/t08-lambda-sombra-inicializador.md).
- [x] T08-002-12b: Verificar muestras de sombras dentro y fuera de bloques anidados y bucles en O0. [Evidencia](registro-commits/t08-inicializadores-ambito-o0.md).
- [x] T08-002-12c: Verificar muestras de alcance de nombres introducidos por patrones y desestructuración en O0.
  - [x] T08-002-12c1: Comprobar muestras de desestructuración sencilla y anidada en O0. [Evidencia](registro-commits/t08-ambito-desestructuracion-o0.md).
  - [x] T08-002-12c2: Comprobar una ligadura `if let` que sombrea una global C y deja visible esa global fuera del bloque. [Evidencia](registro-commits/t08-sombra-patron-o0.md).
- [x] T08-002-13: Comprobar dos muestras de enums anónimos sin procedencia física compartida, incluso con contenido igual, en O0. [Evidencia](registro-commits/t08-enum-anonimo-conflicto-o0.md).

## T08-003 — Funciones C

- [x] T08-003-01: Probar muestras de llamadas escalares y con punteros. [Detalle](TANDA-08-TIPOS-C.md).
- [x] T08-003-02: Probar funciones `static inline` y el camino ThinLTO indicado en el tracking principal.
- [x] T08-003-03: Probar las muestras de declaraciones sin prototipo y promociones C11 registradas en el tracking principal.
- **T08-003-04 — Declaraciones y redeclaraciones:**
- [ ] T08-003-04a: Verificar selección de prototipo compatible entre declaración adelantada y definición.
- [ ] T08-003-04b: Verificar diagnóstico de redeclaraciones con parámetros o retorno incompatibles.
- [ ] T08-003-04c: Verificar funciones variádicas y llamadas sin prototipo con promociones por defecto.
- [ ] T08-003-04d: Verificar captura y símbolo de funciones `static inline` de encabezados distintos.
- **T08-003-05 — Visibilidad y enlace:**
- [ ] T08-003-05a: Verificar llamada externa desde namespace y resolución con un local homónimo.
- [ ] T08-003-05b: Verificar enlace de función C en biblioteca estática y DLL.
- [ ] T08-003-05c: Verificar función usada por plantilla precompilada con encabezado presente y oculto.

## T08-004 — Variables globales C

- [x] T08-004-01: Comprobar las muestras de globales externas y `static` registradas en el tracking principal.
- [x] T08-004-02: Comprobar la matriz dirigida de escalares y punteros atómicos no TLS indicada en el tracking principal.
- [x] T08-004-03: Comprobar las muestras dirigidas de TLS estático, atómico y DLL registradas en el tracking principal.
- **T08-004-04 — Globales por tipo y calificador:**
- [ ] T08-004-04a: Verificar globales escalares enteras y flotantes con `const` y `volatile`.
- [ ] T08-004-04b: Verificar globales de enum, puntero a enum y record C.
- [ ] T08-004-04c: Verificar arreglos globales y matrices con punteros calificados.
- [ ] T08-004-04d: Verificar `_Atomic` en global externa y `static`, con lectura y escritura cruzadas.
- [ ] T08-004-04e: Verificar `_Thread_local` con y sin `_Atomic` en ejecutable y DLL.
- [ ] T08-004-05: Volver a ejecutar los controles TLS y de DLL dentro del gate integral de la tanda.
- **T08-004-06 — Consumo y resolución:**
- [ ] T08-004-06a: Verificar una global externa con encabezado en el consumidor.
- [ ] T08-004-06b: Verificar una global requerida por plantilla con encabezado oculto y metadata.
- [ ] T08-004-06c: Verificar ambigüedad de globales C homónimas y sombra por local NativeSharp.
- [ ] T08-004-06d: Verificar identidad y separación de globales `static` con encabezados homónimos.

## T08-005 — Typedef

- [x] T08-005-01: Probar las muestras de alias escalares encadenados y punteros registradas en el tracking principal.
- **T08-005-02 — Formas de typedef:**
- [ ] T08-005-02a: Verificar cadenas de typedefs escalares con signos y anchos distintos.
- [ ] T08-005-02b: Verificar typedef de puntero con `const` y `volatile` en el apuntado.
- [ ] T08-005-02c: Verificar typedef de arreglo fijo y de puntero a función C.
- [ ] T08-005-02d: Verificar typedef de struct y union, incluidos records anónimos.
- [ ] T08-005-02e: Verificar typedef de enum nombrado y enum anónimo.
- **T08-005-03 — Reconciliación:**
- [ ] T08-005-03a: Verificar typedef equivalente entre dos encabezados distintos.
- [ ] T08-005-03b: Verificar conflicto por tipo subyacente incompatible.
- [ ] T08-005-03c: Verificar consumo del typedef a través de metadata sin encabezado.

## T08-006 — Enteros y `_Bool`

- [x] T08-006-01: Probar la muestra mixta de campo, arreglo y llamada `_Bool` O0–O3.
- [x] T08-006-02: Comparar campos C volátiles con Clang en los caminos directo y precompilado. [Resultado](registro-commits/t08-volatile-campos-revalidacion-alias-pendiente.md).
- **T08-006-03 — Anchuras y signo:**
- [ ] T08-006-03a: Verificar `char`, `signed char` y `unsigned char` contra Clang en el target fijado.
- [ ] T08-006-03b: Verificar `short` y `unsigned short` en parámetros, retorno y campo.
- [ ] T08-006-03c: Verificar `int` y `unsigned int` en parámetros, retorno y campo.
- [ ] T08-006-03d: Verificar `long`, `unsigned long`, `long long` y `unsigned long long` con los anchos reales de Clang.
- [ ] T08-006-03e: Verificar representación `_Bool` en memoria y en firmas C.
- **T08-006-04 — Conversiones y ABI:**
- [ ] T08-006-04a: Verificar extensión de signo o cero de parámetros y retornos estrechos.
- [ ] T08-006-04b: Verificar promociones de enteros estrechos y `_Bool` en operaciones aritméticas y bitwise.
- [ ] T08-006-04c: Verificar almacenamiento y lectura cruzada de escalares en globales, campos y arreglos.
- [ ] T08-006-04d: Verificar límites representables y conversiones entre firmado y sin signo.

## T08-007 — Punto flotante

- [x] T08-007-01: Probar `long double` IEEE binary64 de la muestra directa y precompilada O0–O3.
- **T08-007-02 — Tipos y formatos:**
- [ ] T08-007-02a: Verificar tamaño y representación de `float` contra Clang.
- [ ] T08-007-02b: Verificar tamaño y representación de `double` contra Clang.
- [ ] T08-007-02c: Verificar `long double` y diagnóstico cuando su formato no sea representable.
- **T08-007-03 — Contextos:**
- [ ] T08-007-03a: Verificar parámetros y retornos de cada tipo flotante.
- [ ] T08-007-03b: Verificar lectura y escritura de campos y globales flotantes.
- [ ] T08-007-03c: Verificar persistencia del formato y uso desde metadata precompilada.

## T08-008 — `void`

- [x] T08-008-01: Probar el retorno `void`, `void*` y prototipo `f(void)` de la muestra C11.
- **T08-008-02 — Usos de `void`:**
- [ ] T08-008-02a: Verificar `void` como retorno y `f(void)` sin parámetros.
- [ ] T08-008-02b: Verificar `void*` en parámetro, retorno, campo y global.
- [ ] T08-008-02c: Verificar conservación de `const void*` y `volatile void*` en conversiones.
- [ ] T08-008-02d: Verificar restauración de firmas con `void*` desde metadata.

## T08-009 — Enumeraciones

- [x] T08-009-01: Probar las muestras de enum C y conversiones enum/entero registradas en el tracking principal.
- [x] T08-009-02: Probar el caso corregido de comparación `unsigned int` / `int`. [Evidencia](registro-commits/t08-enum-c-uint-int.md).
- **T08-009-03 — Valores y operaciones:**
- [ ] T08-009-03a: Verificar valores negativos y límites del tipo entero subyacente.
- [ ] T08-009-03b: Verificar conversiones entre enum, `int`, `unsigned int` y enteros de mayor ancho.
- [ ] T08-009-03c: Verificar comparaciones mixtas con operandos firmados y sin signo.
- [ ] T08-009-03d: Verificar aritmética, desplazamientos y operaciones bitwise.
- **T08-009-04 — Identidad y persistencia:**
- [ ] T08-009-04a: Verificar reconciliación de enum nombrado en dos encabezados.
- [ ] T08-009-04b: Verificar colisión de enums anónimos con el mismo primer enumerador.
- [ ] T08-009-04c: Verificar enum anónimo como campo de record publicado.
- [ ] T08-009-04d: Verificar identidad y valores de enum anidado restaurado desde metadata.
- **T08-009-05 — Consumo:**
- [ ] T08-009-05a: Verificar enum en firma pública con encabezado disponible.
- [ ] T08-009-05b: Verificar la misma firma y constantes con encabezado oculto.
- [ ] T08-009-05c: Verificar conflictos entre definición del encabezado y catálogo precompilado.

## T08-010 — Struct C

- [x] T08-010-01: Comparar las muestras de layout natural, empaquetado y anidado contra Clang. [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
- [x] T08-010-02: Probar muestras de miembros anónimos y anidados en los caminos indicados en el tracking principal.
- [x] T08-010-03: Probar muestras de miembro flexible, lectura/escritura y conversión a puntero O0–O3 indicadas en el tracking principal.
- **T08-010-04 — Miembros de struct:**
- [ ] T08-010-04a: Verificar campos escalares enteros, flotantes, enum y `_Bool`.
- [ ] T08-010-04b: Verificar campos puntero y puntero a función, incluidos calificadores.
- [ ] T08-010-04c: Verificar arreglos fijos y miembro flexible al final del struct.
- [ ] T08-010-04d: Verificar campos de bits con y sin signo, anónimos y de ancho cero.
- [ ] T08-010-04e: Verificar promoción de miembros de struct y union anónimos en varios niveles.
- [ ] T08-010-04f: Verificar struct anidado en `pack(1)`, `pack(2)` y `pack(4)`.
- [ ] T08-010-05: Resolver el orden de acceso mediante alias a miembro flexible `volatile` en modo Microsoft e ISO.
- **T08-010-06 — Caminos de consumo:**
- [ ] T08-010-06a: Verificar lectura y escritura cruzadas para miembros anónimos por encabezado directo.
- [ ] T08-010-06b: Verificar los mismos miembros con biblioteca precompilada y encabezado.
- [ ] T08-010-06c: Verificar los mismos miembros con biblioteca precompilada y solo metadata.
- [ ] T08-010-06d: Verificar campos flexibles calificados en esos tres caminos.
- [ ] T08-010-07: Validar los modos Microsoft e ISO con dos referencias al mismo encabezado en la misma compilación.
- [ ] T08-010-08: Verificar el alias volátil reasignado en los cuatro niveles de optimización después de la corrección.
- [ ] T08-010-09: Verificar lectura y escritura `*(data + index)` frente a Clang después de la corrección.

## T08-011 — Union C

- [x] T08-011-01: Probar muestras de miembros superpuestos, campos de bits y unions empaquetadas indicadas en el tracking principal.
- **T08-011-02 — Formas de union:**
- [ ] T08-011-02a: Verificar union natural con miembros enteros, flotantes y puntero.
- [ ] T08-011-02b: Verificar union empaquetada con miembros desalineados.
- [ ] T08-011-02c: Verificar union dentro de struct y struct dentro de union.
- [ ] T08-011-02d: Verificar union anónima con miembros promovidos y campos de bits superpuestos.
- **T08-011-03 — Validación:**
- [ ] T08-011-03a: Comparar tamaño, alineación y offsets con Clang.
- [ ] T08-011-03b: Comparar bytes tras escribir por cada miembro y leer por otro.
- [ ] T08-011-03c: Repetir el control con productor precompilado y sin encabezado en el consumidor.

## T08-012 — Layout obtenido de Clang

- [x] T08-012-01: Comparar los tamaños, alineaciones de record y offsets de la muestra de layout. [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
- [ ] T08-012-02: Resolver y verificar el contrato de alineación efectiva de cada campo frente a Clang.
- **T08-012-03 — Cobertura de layout:**
- [ ] T08-012-03a: Comparar offset y alineación de campos escalares naturales y bajo `pack(1/2/4)`.
- [ ] T08-012-03b: Comparar layout de arreglos de records y records anidados.
- [ ] T08-012-03c: Comparar ventanas y alineación de campos de bits con y sin nombre.
- [ ] T08-012-03d: Comparar `_Alignas` y typedefs con alineación explícita.
- [ ] T08-012-03e: Repetir las comparaciones con información restaurada de metadata.

## T08-013 — Tipos incompletos

- [x] T08-013-01: Probar las muestras de punteros a structs y unions opacos en uso directo y precompilado.
- **T08-013-02 — Declaración y definición:**
- [ ] T08-013-02a: Verificar struct adelantado y definición posterior en el mismo encabezado.
- [ ] T08-013-02b: Verificar union adelantada y definición posterior en otro encabezado visible.
- [ ] T08-013-02c: Verificar conciliación de tipo opaco entre encabezado y metadata.
- [ ] T08-013-02d: Verificar rechazo de definiciones incompatibles con la misma identidad.
- **T08-013-03 — Operaciones:**
- [ ] T08-013-03a: Verificar paso, retorno y almacenamiento de `Opaque*` y `Opaque**`.
- [ ] T08-013-03b: Verificar rechazo de `sizeof`, variable por valor y acceso a miembro de tipo incompleto.
- [ ] T08-013-03c: Verificar rechazo de aritmética e indexación de `Opaque*` y admisión aplicable a `Opaque**`.

## T08-014 — Punteros

- [x] T08-014-01: Probar la muestra `_Bool*` devuelta, almacenada y seleccionada que documenta el tracking principal.
- **T08-014-02 — Tipos y conversiones:**
- [ ] T08-014-02a: Verificar punteros a enteros, flotantes y `_Bool`.
- [ ] T08-014-02b: Verificar punteros a struct, union, enum y `void`.
- [ ] T08-014-02c: Verificar `T**` y conservación de calificadores en cada nivel representado.
- [ ] T08-014-02d: Verificar conversiones compatibles y diagnósticos por pérdida de calificadores.
- **T08-014-03 — Contextos:**
- [ ] T08-014-03a: Verificar punteros como parámetros y retornos de funciones C.
- [ ] T08-014-03b: Verificar punteros en campos y globales C.
- [ ] T08-014-03c: Verificar uso y restauración de punteros desde biblioteca precompilada.
- **T08-014-04 — Contratos:**
- [ ] T08-014-04a: Verificar alineación de puntero a campo natural y empaquetado.
- [ ] T08-014-04b: Verificar nulabilidad y diagnósticos de desreferencia aplicables.
- [ ] T08-014-04c: Verificar propagación de `const` y `volatile` en inicialización y reasignación.

## T08-015 — Calificadores

- [x] T08-015-01: Probar las muestras registradas de `const` y `volatile` en globales, campos y arreglos.
- [x] T08-015-02: Comparar los campos volátiles C contra Clang en los casos del fixture. [Resultado](registro-commits/t08-volatile-campos-revalidacion-alias-pendiente.md).
- **T08-015-03 — Modo de la unidad consumidora:**
- [ ] T08-015-03a: Definir el modo predeterminado del consumidor cuando no se especifica uno explícito.
- [ ] T08-015-03b: Verificar selección explícita de Microsoft e ISO por proyecto.
- [ ] T08-015-03c: Conservar la selección por archivo en un grafo con múltiples proyectos.
- [ ] T08-015-03d: Resolver el modo del consumidor cuando dos referencias usan modos distintos.
- **T08-015-04 — Alias y alineación:** [Diagnóstico](registro-commits/t08-volatile-alias-aritmetica-diagnostico.md).
- [ ] T08-015-04a: Conservar el contrato C volátil al inferir un alias local.
- [ ] T08-015-04b: Conservar el orden y la alineación al leer y escribir mediante el alias.
- [ ] T08-015-04c: Verificar el contrato después de reasignar el alias.
- [ ] T08-015-04d: Verificar el contrato después de aritmética e indexación de punteros.
- [ ] T08-015-04e: Verificar que un campo empaquetado o subalineado no reciba alineación atómica no demostrada.
- **T08-015-05 — Transporte del contrato:**
- [ ] T08-015-05a: Verificar alias `volatile` pasado como parámetro y retornado por función.
- [ ] T08-015-05b: Verificar alias reasignado en ramas de control con modos compatibles.
- [ ] T08-015-05c: Verificar propagación entre proyectos y biblioteca precompilada.
- **T08-015-06 — Caché y aislamiento:**
- [ ] T08-015-06a: Verificar invalidación o identidad de caché al cambiar Microsoft/ISO.
- [ ] T08-015-06b: Verificar que `volatile` nativo conserve su contrato sin importar un header C.
- [ ] T08-015-06c: Verificar el mismo comportamiento nativo al importar un header C.
- **T08-015-07 — Calificadores de puntero:**
- [ ] T08-015-07a: Verificar rechazo de escritura mediante `const T*` importado.
- [ ] T08-015-07b: Verificar acceso `volatile T*` en lectura, escritura e índice.
- [ ] T08-015-07c: Verificar `const volatile T*` en campos, globales y parámetros.
- [ ] T08-015-07d: Verificar calificadores del apuntado y del puntero en `T* const` y `T**`.
- [ ] T08-015-08: Habilitar la generación con modo explícito únicamente después de completar su contrato y retirar la protección temporal con pruebas.
- [ ] T08-015-09: Repetir la comparación de IR de alias directo, reasignado y con aritmética en Microsoft e ISO O0–O3.
- [ ] T08-015-10: Verificar el comportamiento con referencias mixtas y con biblioteca precompilada.

## T08-016 — Arreglos fijos

- [x] T08-016-01: Probar las muestras de arreglos y matrices empaquetadas registradas en el tracking principal.
- [x] T08-016-02: Probar las muestras de arreglos de punteros calificados y records indicadas en el tracking principal.
- **T08-016-03 — Formas de arreglo:**
- [ ] T08-016-03a: Verificar arreglo fijo de escalares enteros y flotantes.
- [ ] T08-016-03b: Verificar matriz multidimensional de escalares.
- [ ] T08-016-03c: Verificar arreglo de structs, unions y enums.
- [ ] T08-016-03d: Verificar arreglo de punteros `const` y punteros a datos `const` o `volatile`.
- [ ] T08-016-03e: Verificar arreglos dentro de records empaquetados y anidados.
- **T08-016-04 — Acceso y consumo:**
- [ ] T08-016-04a: Comparar tamaño, offsets y alineación de elementos con Clang.
- [ ] T08-016-04b: Verificar lecturas y escrituras cruzadas de arreglos y matrices.
- [ ] T08-016-04c: Verificar toma de dirección y diagnóstico cuando el elemento está subalineado.
- [ ] T08-016-04d: Repetir el acceso con encabezado oculto y metadata precompilada.

## T08-017 — Punteros a función C

- [x] T08-017-01: Probar las muestras registradas de callbacks escalares y callbacks con record o enum.
- [x] T08-017-02: Probar las muestras precompiladas de callbacks con y sin encabezado indicadas en el tracking principal.
- **T08-017-03 — Firmas y convenciones:**
- [ ] T08-017-03a: Verificar callback con parámetros y retorno escalares.
- [ ] T08-017-03b: Verificar callback con parámetros o retorno de puntero a record y enum.
- [ ] T08-017-03c: Verificar callback variádico C y su identidad.
- [ ] T08-017-03d: Verificar callback `vectorcall` y su convención de llamada.
- **T08-017-04 — Almacenamiento y calificadores:**
- [ ] T08-017-04a: Verificar callback almacenado en campo de struct C.
- [ ] T08-017-04b: Verificar callback almacenado en global C.
- [ ] T08-017-04c: Verificar callback retornado por función C.
- [ ] T08-017-04d: Verificar lectura/escritura de callback `volatile` alineado y empaquetado.
- [ ] T08-017-04e: Verificar rechazo de escritura sobre un callback almacenado `const`.
- **T08-017-05 — Metadata:**
- [ ] T08-017-05a: Verificar publicación de firma y convención del callback en metadata.
- [ ] T08-017-05b: Verificar invocación del callback restaurado con encabezado visible.
- [ ] T08-017-05c: Verificar invocación del callback restaurado con encabezado oculto.
- [ ] T08-017-06: Conciliar las notas históricas contradictorias sobre callbacks volátiles almacenados con el comportamiento y las pruebas actuales.
- [ ] T08-017-07: Conciliar el rechazo histórico del método que retorna callback precompilado con la validación posterior del mismo camino.

## T08-018 — Colisiones de nombres

- [x] T08-018-01: Probar la muestra de sombra local de global y enumerador C en O0.
- **T08-018-02 — Colisiones y ámbitos:**
- [ ] T08-018-02a: Verificar colisión entre función C y función NativeSharp en namespace.
- [ ] T08-018-02b: Verificar global y enumerador C ocultados por local o parámetro NativeSharp.
- [ ] T08-018-02c: Verificar colisión entre typedef C y tipo NativeSharp.
- [ ] T08-018-02d: Verificar sombras en bloques, bucles, `catch` y lambdas.
- **T08-018-03 — Resolución:**
- [ ] T08-018-03a: Verificar resolución antes, dentro y después del ámbito de sombra.
- [ ] T08-018-03b: Verificar diagnóstico de ambigüedad entre dos declaraciones C visibles.
- [ ] T08-018-03c: Repetir los casos de función y global desde plantilla precompilada.

## T08-019 — Diagnósticos de tipos incompletos

- [x] T08-019-01: Probar diagnósticos para aritmética, incremento e índice de records C incompletos. [Evidencia](registro-commits/aritmetica-puntero-incompleto-c.md).
- [x] T08-019-02: Probar el diagnóstico de local incompleto por valor y la muestra de `sizeof`. [Evidencia](registro-commits/diagnostico-local-incompleto-c.md).
- **T08-019-03 — Operaciones inválidas:**
- [ ] T08-019-03a: Verificar diagnóstico de `sizeof` y `alignof` sobre struct y union incompletos.
- [ ] T08-019-03b: Verificar diagnóstico de campo o variable local incompleta por valor.
- [ ] T08-019-03c: Verificar diagnóstico de acceso a miembro no disponible.
- [ ] T08-019-03d: Verificar diagnóstico de aritmética, incremento e índice sobre puntero a incompleto.
- [ ] T08-019-03e: Verificar diagnóstico de parámetro o retorno incompleto por valor.
- [ ] T08-019-04: Verificar que los diagnósticos identifican la declaración y la operación inválida también con metadata.

## T08-020 — Comparación de layout con Clang

- [x] T08-020-01: Comparar la muestra de 55 valores de layout y lecturas/escrituras cruzadas. [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
- **T08-020-02 — Comparación de layout:**
- [ ] T08-020-02a: Comparar tamaño, alineación y offsets de structs naturales y `pack(1/2/4)`.
- [ ] T08-020-02b: Comparar unions naturales y empaquetadas con miembros superpuestos.
- [ ] T08-020-02c: Comparar structs y unions anónimos o anidados.
- [ ] T08-020-02d: Comparar arreglos de records y miembros flexibles.
- **T08-020-03 — Comparación de IR:**
- [ ] T08-020-03a: Comparar alineación de `load` y `store` de campos escalares.
- [ ] T08-020-03b: Comparar ventanas de lectura y escritura de bitfields.
- [ ] T08-020-03c: Comparar accesos `volatile` alineados y empaquetados en modos Microsoft e ISO.
- **T08-020-04 — Tipos desde metadata:**
- [ ] T08-020-04a: Verificar `sizeof(T)` y `alignof(T)` para un struct C publicado sin encabezado.
- [ ] T08-020-04b: Verificar declaración y cast de puntero a tipo C publicado sin encabezado.
- [ ] T08-020-04c: Verificar `offsetof` y acceso a miembro restaurado desde metadata.
- [ ] T08-020-04d: Repetir los controles para union, enum y typedef publicados.

## Cierre de la tanda

- [ ] T08-GATE-01: Completar la implementación y las comprobaciones pendientes de T08-002 a T08-020.
- [ ] T08-GATE-02: Resolver o clasificar con evidencia el fallo de la prueba de topología de ondas paralelas.
- [ ] T08-GATE-03: Ejecutar la validación integral agrupada de la tanda 8 y corregir sus fallos hasta que pase.
- [ ] T08-GATE-04: Actualizar estados y evidencias contra la revisión validada, sin cerrar por muestras parciales.
- [ ] T08-GATE-05: Revisar la agrupación de commits relacionados y preservar un respaldo del historial, conforme a lo acordado.
