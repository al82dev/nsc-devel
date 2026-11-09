# Tanda 08: seguimiento detallado de interoperabilidad C

Esta vista desglosa los 20 ítems de la [tanda 08](TRACKING.md#08-declaraciones-nombres-y-tipos-c). Los identificadores originales permanecen vigentes. Cada casilla marcada describe **únicamente el caso y la validación indicados**, no la cobertura completa de su categoría. Las casillas vacías son trabajo o validación pendientes; no equivalen necesariamente a código inexistente.

La evidencia histórica y sus revisiones se conserva en [TRACKING.md](TRACKING.md). Un resultado antiguo puede haber sido ampliado después; ante discrepancias debe contrastarse con el código, el registro enlazado y el gate actual. La validación integral de la tanda sigue pendiente. Esta lista no debe interpretarse como un inventario cerrado de todas las variantes de C: las combinaciones aún no inventariadas siguen incluidas en los ítems originales.

Los identificadores con sufijo (`a`, `b`...) desglosan la casilla anterior del mismo número; su estado anterior se conserva como encabezado, sin contar una segunda tarea. Una casilla pendiente de comprobación no afirma que la capacidad esté ausente: indica que falta cotejar la evidencia o completar su implementación si la comprobación falla.

**Decisión vigente del usuario:** NativeSharp tiene un único contrato NSMETA: el que entiende el compilador actual. El lenguaje evoluciona de forma acumulativa; los programas anteriores que usan capacidades todavía admitidas deben seguir compilándose. El contenido y las capacidades que requiere determinan si puede procesarse, no las etiquetas históricas. No introducir versiones paralelas ni planear migraciones entre etiquetas. No consultar documentación histórica de versionamiento; el material archivado queda fuera de la consulta activa. Se conservan intactas las pruebas existentes. Esta decisión no certifica automáticamente todos los artefactos anteriores.

## T08-001 — Representación y nombres

- [x] T08-001-01: Auditar la representación y búsqueda de nombres existentes. [Auditoría](TANDA-08-TIPOS-C.md).

## T08-002 — Identidad de declaraciones

- Validación del bloque (2026-11-08): aprobada; las casillas abiertas quedan bloqueadas por decisiones pendientes. [Evidencia](registro-commits/t08-002-validacion-bloque.md).

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
  - [x] T08-002-09m7c0: Empaquetar las DLL TLS cargadas dinámicamente y ejecutar el programa desde una salida aislada y otro directorio de trabajo. [Evidencia](registro-commits/t08-tls-dll-empaquetado-dinamico.md).
  - [x] T08-002-09m7c0a: Comprobar el empaquetado de una dependencia transitiva de una DLL TLS cargada dinámicamente. [Evidencia](registro-commits/t08-tls-dll-transitiva.md).
  - [x] T08-002-09m7c0b: Comprobar restauración en caché del consumidor y de ambas DLL TLS. [Evidencia](registro-commits/t08-tls-dll-cache.md).
  - [x] T08-002-09m7c0c: Comprobar primeros accesos a las DLL TLS desde dos hilos liberados por una señal común, en O0–O3. [Evidencia y límite](registro-commits/t08-tls-dll-concurrencia.md).
  - [x] T08-002-09m7c0d: Instrumentar la entrada a `LoadLibraryExW` y comprobar que dos hilos coinciden antes de permitir la carga inicial. [Evidencia](registro-commits/t08-tls-dll-concurrencia-instrumentada.md).
  - [x] T08-002-09m7c0e: Resolver la DLL TLS por la ruta del módulo cuando otra DLL con el mismo nombre ya está cargada, en O0–O3. [Evidencia](registro-commits/t08-tls-dll-preexistente.md).
  - [x] T08-002-09m7c0f: Comprobar una DLL y una salida con U+6F22, incluyendo restauración desde caché. [Evidencia](registro-commits/t08-tls-dll-unicode.md).
  - [x] T08-002-09m7c0g: Ampliar la cobertura Unicode a otras formas de ruta, nombres de fuentes y equivalencia de mayúsculas/minúsculas. [Evidencia](registro-commits/t08-d01-dll-unicode.md).
    - [x] T08-002-09m7c0g1: Proyecto, fuente y salida con U+6F22, ejecución en O0, restauración desde caché y diagnóstico sin sobrescribir una DLL diferente. [Evidencia](registro-commits/t08-proyecto-fuente-unicode.md).
    - [x] T08-002-09m7c0g2: Otras formas de ruta y equivalencia de mayúsculas/minúsculas Unicode; no quedan cubiertas por el caso anterior. [Evidencia](registro-commits/t08-d01-dll-unicode.md).
      - [x] T08-002-09m7c0g2a: Referencias `.lib` al mismo archivo con `É`/`é`, control de archivo distinto con `Ê` y archivo de proyecto con U+6F22 y `Name` explícito. [Evidencia](registro-commits/t08-rutas-unicode-mayusculas.md).
      - [x] T08-002-09m7c0g2b: Otras equivalencias Unicode y otras entradas de ruta; no están demostradas por el caso `É`/`é`.
        - [x] Corregir rutas UTF-8 de referencias de proyecto y encabezados C; comprobar bibliotecas, fuentes, proyectos y directorios de inclusión con Δ/δ, Ж/ж y Ā/ā, incluida ejecución C en O0. [Evidencia](registro-commits/t08-referencias-rutas-unicode.md).
        - [x] Seguir a Windows (`os.path.samefile`) con NFC/NFD, İ/i, ı/I, ß/ẞ, ǅ/ǆ, Σ/ς, K (Kelvin)/k y el control É/é en fuentes, referencias de proyecto, encabezados C, salida y caché; corregir los nombres de producto, el directorio temporal y la caché que la página de códigos activa no puede escribir (identidad del paquete, raíz de bibliotecas, emisión LLVM, ThinLTO, candados de caché y enlace con NSLink), con ejecutables O0–O3 y PDB. [Evidencia](registro-commits/t08-rutas-unicode-nombres-salida.md).
      - [x] T08-002-09m7c0g2d: DLL NativeSharp con nombre de archivo no ASCII: se enlazan con NSLink y lld-link y se cargan por ruta UTF-16 con la biblioteca cargadora Unicode (decisión del usuario T08-D01). [Evidencia](registro-commits/t08-d01-dll-unicode.md).
      - [x] T08-002-09m7c0g2c: Compilar un productor que importa C desde proyecto, fuente y encabezado ubicados en ruta U+6F22, y consumir su biblioteca sin el encabezado en O0. [Evidencia](registro-commits/t08-productor-c-unicode.md).
  - [ ] T08-002-09m7c1: Admitir dos DLL diferentes con el mismo nombre de archivo procedentes de directorios diferentes; requieren una identidad y carga por ruta que preserven ambas bibliotecas. **Bloqueada:** quedan T08-D04b (TLS sin importaciones, pendiente de aprobación) y T08-D05b (alias importado directamente con dependencias homónimas).
    - [x] T08-002-09m7c1a: Verificar que Windows carga las dos DLL bajo nombres independientes y conserva sus valores TLS. [Evidencia](registro-commits/t08-dll-homonimas-alias-control.md).
    - [x] T08-002-09m7c1b: Derivar nombres de salida estables desde la identidad de las DLL y usar la misma correspondencia en generación y empaquetado. [Evidencia](registro-commits/t08-dll-homonimas-identidad.md).
    - [x] T08-002-09m7c1c: Resolver las dos TLS en dos hilos, publicar ambas DLL sin sobrescribir una DLL preexistente y ejecutar desde otra carpeta en O0–O3. [Evidencia](registro-commits/t08-dll-homonimas-identidad.md).
    - [x] T08-002-09m7c1d1: Restaurar el consumidor y ambas DLL desde caché en O0–O3, incluso cuando ya estaban presentes al guardar el paquete. [Evidencia](registro-commits/t08-dll-homonimas-identidad.md).
    - [ ] T08-002-09m7c1d2: Resolver las importaciones y dependencias transitivas que apuntan a DLL homónimas; las variantes no comprobadas siguen abiertas. **Bloqueada:** quedan T08-D04b y T08-D05b.
      - [x] T08-002-09m7c1d2a: Comparar la identidad de módulo de una importación directa frente a un alias copiado y un hardlink. [Evidencia](registro-commits/t08-dll-homonimas-import-directo-control.md).
      - [x] T08-002-09m7c1d2a1: Comprobar con un control C que una importación enlazada al nombre empaquetado comparte el módulo y el estado TLS en dos hilos después de trasladar el ejecutable. [Evidencia](registro-commits/t08-dll-homonimas-import-alias-control.md).
      - [ ] T08-002-09m7c1d2b: Mantener una sola instancia y estado TLS entre la llamada importada y el acceso NativeSharp, incluso después de copiar el paquete a otro equipo. **Bloqueada:** las variantes comprobables están cerradas; los calificadores en metadata y la superficie `volatile` se resolvieron en T08-D02/D03; queda TLS sin importaciones (T08-D04b). El traslado a otro equipo se verifica como traslado de carpeta; un equipo distinto no está disponible en este entorno.
        - Preparación: la selección de alias está separada de la inspección del PE para poder utilizarla antes del enlace. [Evidencia](registro-commits/t08-dll-homonimas-seleccion-antes-enlace.md).
        - [x] En un ejecutable, enlazar funciones C de origen Clang no ambiguo al alias TLS; comprobar lectura y escritura en dos hilos y traslado de carpeta en O0–O3. [Evidencia](registro-commits/t08-dll-homonimas-import-directo-alias.md).
        - [x] Restaurar desde caché un ejecutable que utiliza una función C desde una plantilla precompilada y dos DLL TLS homónimas, de O0 a O3. [Evidencia](registro-commits/t08-dll-homonimas-funcion-precompilada-cache.md).
        - [x] Publicar y restaurar el mismo tipo de paquete con una ruta de caché larga, incluidas ambas DLL TLS, en O0–O3. [Evidencia](registro-commits/t08-cache-ruta-larga.md).
        - [x] Comprobar que dos DLL homónimas que exportan la misma función producen un diagnóstico y no publican un ejecutable con una elección arbitraria. [Evidencia](registro-commits/t08-dll-homonimas-origen-ambiguo.md).
        - [x] Distinguir la misma función C de dos DLL TLS homónimas cuando cada llamada proviene de una plantilla precompilada con su origen identificado; comprobar O0–O3 y restauración desde caché. [Evidencia](registro-commits/t08-dll-homonimas-dos-funciones.md).
        - [x] Comprobar ambas funciones y sus variables TLS desde dos hilos, en O0–O3. [Evidencia](registro-commits/t08-dll-homonimas-dos-funciones-hilos.md).
        - [ ] Resolver las demás formas de distinguir orígenes, incluidas llamadas con argumentos variables; comprobar caché para referencias directas a encabezados, traslado a otro equipo y las demás variantes de importación. **Bloqueada:** subcasillas comprobables cerradas; quedan solo decisiones pendientes.
          - Avance: la sesión de Clang puede registrar opcionalmente una huella de tokens y decisiones del preprocesador, incluido `__has_include`; aún no constituye una clave de caché completa. [Detalle](registro-commits/t08-preprocesador-huella.md).
          - Avance: la huella también registra decisiones de condicionales por macros que afectan directivas; el soporte de caché directa sigue pendiente. [Detalle](registro-commits/t08-huella-condicionales-macro.md).
          - Avance: las macros dependientes de fecha y hora impiden producir una huella reutilizable; la identidad completa de caché sigue pendiente. [Detalle](registro-commits/t08-huella-macros-inestables.md).
          - [x] En una muestra de encabezado C directo, reutilizar la compilación e invalidarla al cambiar un encabezado transitivo o el resultado de `__has_include`, en O0 y O3; desactivar la reutilización con macros temporales. [Evidencia](registro-commits/t08-cache-encabezados-c-directos.md).
          - [x] Resolver en el sitio de llamada una función C variádica de cada una de dos DLL TLS homónimas y comprobar seis argumentos enteros de O0 a O3. [Evidencia](registro-commits/t08-dll-homonimas-variadicas.md).
          - [ ] Completar las variantes de dirección de función y delegados, incluidos typedefs homónimos y otros modos de consumo. **Bloqueada:** quedan solo decisiones pendientes.
            - [x] Tomar la dirección de una función C importada sin TLS dentro de una función que no es el punto de entrada (módulo partido) y usarla como callback desde NativeSharp y desde C, directo y precompilado sin encabezado, O0–O3. [Evidencia](registro-commits/t08-callback-direccion-coordinador.md).
            - [x] Guardar y llamar mediante punteros C variádicos de dos DLL TLS homónimas desde plantillas precompiladas, con typedefs de nombres distintos, en O0–O3 y con caché. [Evidencia](registro-commits/t08-dll-homonimas-variadicas-puntero.md).
            - [x] Conciliar la muestra precompilada de typedefs homónimos a puntero de función variádica con tipos escalares; rechazar una firma incompatible. [Evidencia](registro-commits/t08-dll-homonimas-typedef-funcion.md).
            - [ ] Comprobar otras firmas de typedef de función, incluyendo records, enums y calificadores; no extrapolar la muestra escalar. **Bloqueada:** quedan solo decisiones pendientes.
              - [x] Reproducir en O0 el conflicto de dos typedefs de función que reciben un record C idéntico desde productores precompilados y contrastarlo con un record incompatible. [Evidencia](registro-commits/t08-typedef-funcion-record-reproduccion.md).
              - [ ] Conciliar firmas con records precompilados usando evidencia de identidad y definición suficiente; comprobar también records distintos con igual tamaño y nombres coincidentes. **Bloqueada:** quedan solo decisiones pendientes.
                - [x] Conciliar typedefs de función con un parámetro puntero a record equivalente de dos productores precompilados, sin encabezados, O0–O3; rechazar records incompatibles, incluso del mismo tamaño. [Evidencia](registro-commits/t08-typedef-funcion-record-conciliacion.md).
                - [x] Invocar mediante typedef variádico dos funciones de DLL homónimas con un puntero a record proporcionado por cada DLL, sin encabezados, O0–O3 y con caché. [Evidencia](registro-commits/t08-typedef-record-invocacion.md).
                - [x] Invocar la variante con un record anidado en dos DLL, sin encabezados, O0–O3 y caché; rechazar un campo anidado incompatible del mismo tamaño. [Evidencia](registro-commits/t08-typedef-record-anidado.md).
                - [x] Invocar punteros a función variádica con retorno de record pequeño y grande, de dos DLL homónimas, sin encabezados, O0–O3 y caché. [Evidencia](registro-commits/t08-typedef-record-retorno.md).
                - [x] Invocar mediante typedef variádico con parámetro enum C de dos DLL homónimas, sin encabezados, O0–O3 y caché; rechazar enums de valores distintos y diagnosticar enumerador ambiguo sin productor. [Evidencia](registro-commits/t08-typedef-enum-funcion.md).
                - [x] Invocar un typedef variádico que recibe y devuelve un enum C en dos DLL, sin encabezados, O0–O3 y caché; rechazar definiciones incompatibles. [Evidencia](registro-commits/t08-typedef-enum-retorno.md).
                - [x] Conciliar firmas con `const CRecord*` iguales y rechazar la firma sin `const`, sin encabezados, en O0. [Evidencia](registro-commits/t08-typedef-record-const.md).
                - [x] Conciliar firmas con `volatile` y `const volatile` en el record apuntado, rechazar calificadores incompatibles y contrastar calificadores superiores del puntero con Clang, en O0 y O3. [Evidencia y límite](registro-commits/t08-callback-puntero-volatile.md).
                - [x] Conservar el calificador al invocar indirectamente callbacks C que reciben punteros volátiles: identidad `fn:` con calificación por parámetro y retorno, conversión de métodos, invocación, retorno y `var`; directo y precompilado sin encabezado, O0–O3, con rechazos de pérdida. [Reproducción](registro-commits/t08-callback-volatile-invocacion.md), [corrección](registro-commits/t08-callback-volatile-identidad.md).
                - [ ] Comprobar otras variantes de enums, calificadores y tipos de retorno. **Bloqueada:** las variantes comprobables están cerradas; quedan las dos decisiones pendientes listadas.
                  - [x] Conservar `const` del dato apuntado en la identidad de callbacks C: conversión, invocación, resultado, `var` y superficie pública materializada desde metadata; directo y precompilado sin encabezado, O0–O3, con seis rechazos y control Clang. [Evidencia](registro-commits/t08-callback-const-identidad.md).
                  - [x] Importar funciones, campos y globales C cuyo tipo es un puntero a función con parámetros o resultado `volatile T*` o `const T*`, sin typedef; directo y precompilado sin encabezado, O0–O3, con cinco rechazos y control Clang. [Evidencia](registro-commits/t08-puntero-funcion-calificado.md).
                  - [x] Verificar el parámetro `volatile T*` o `const T*` de una lambda o método NativeSharp convertido a callback C: el cuerpo de la lambda recibe la calificación (escritura rechazada, lectura volatile en IR); directo y precompilado, O0–O3. [Evidencia y límites](registro-commits/t08-lambda-callback-calificado.md).
                  - [x] Decisión resuelta por T08-D02 (NSMeta): la firma de un método NativeSharp público con parámetro `T const*`, o de un delegate con nombre declarado con alias C calificado, conserva `const`/`volatile` al materializarse desde metadata; el consumidor convierte ese método a un callback C con `const` y aplica a esos delegates las reglas calificadas. [Origen](registro-commits/t08-lambda-callback-calificado.md), [Evidencia](registro-commits/t08-d02-d03-calificadores-nsmeta.md).
                  - [x] Pasar una lambda directamente como argumento de una función C cuyo parámetro es un typedef de función: clasificado como regla general del lenguaje, fuera de interop. Ninguna función acepta una lambda en línea como argumento: ni C ni NativeSharp, ni con `fn*` ni con delegate con nombre, con tipos o sin ellos. La conversión ocurre al asignarla a una variable del tipo destino. [Evidencia](registro-commits/t08-lambda-callback-calificado.md).
                  - [x] Llevar la calificación `volatile` (y `const`) a la firma de un `delegate` con nombre declarado con alias C: conversión, invocación, resultado y lambdas; directo O0–O3 con siete rechazos y control Clang; desde T08-D02 también en el camino precompilado sin encabezado. [Evidencia](registro-commits/t08-delegate-alias-calificado.md), [Evidencia](registro-commits/t08-d02-d03-calificadores-nsmeta.md).
                  - [x] Decisión resuelta por T08-D03: la superficie pública materializada desde metadata representa una firma de callback con `volatile` mediante la escritura interna `T volatile*`, aceptada solo en fuentes materializadas; el código de usuario no cambia. [Opciones](registro-commits/t08-superficie-callback-volatile-decision.md), [Evidencia](registro-commits/t08-d02-d03-calificadores-nsmeta.md).
          - [x] Probar la muestra de ambas DLL con funciones variádicas homónimas desde plantillas precompiladas y con caché, en O0–O3 y con valores distinguibles. [Evidencia](registro-commits/t08-dll-homonimas-variadicas.md).
          - [x] Comprobar promociones, argumentos flotantes y agregados en funciones variádicas, además de otras formas de consumo.
            - [x] Comparar promociones de `byte`, `short` y `float`, junto con `double`, contra controles C para dos DLL homónimas, O0–O3 y caché. [Evidencia](registro-commits/t08-dll-homonimas-variadicas-promociones.md).
            - [x] Comprobar agregados y otras promociones, incluidas variantes con signo y punteros, sin extrapolar la muestra anterior: enteros estrechos con signo y sin signo, `bool`, `long long`, `unsigned`, `double`, punteros y records C de 8, 24 y 3 bytes por valor (los records eran rechazados; corregido), directo y por puntero a función variádico, en dos DLL TLS homónimas, O0–O3 y caché. [Evidencia](registro-commits/t08-variadicas-agregados.md).
        - [ ] Investigar el valor inicial TLS de una DLL de control sin importaciones: también falla en el control C; causa aún no identificada. [Reproducción](pruebas-tanda-08-identidad-c/tls-dll-distintas/validar-origen-ambiguo.py). **Bloqueada:** causa determinada; queda la decisión pendiente listada.
          - [x] Contrastar importación directa y carga dinámica, con y sin `DllMain`; inspeccionar el índice y el bloque TLS observado. [Evidencia](registro-commits/t08-dll-sin-import-inspeccion-tls.md).
          - [x] Confirmar que `DllMain` se ejecuta y contrastar una DLL que contiene una importación no invocada por la función TLS. [Evidencia](registro-commits/t08-dll-tls-importacion-no-invocada.md).
          - [x] Comparar la DLL sin importaciones con una copia que contiene un descriptor de importación vacío, sin añadir dependencias. [Evidencia](registro-commits/t08-dll-tls-directorio-importacion.md).
          - [x] Reproducir la diferencia en un proceso anfitrión convencional, tanto en el hilo principal como en otro hilo. [Evidencia](registro-commits/t08-tls-sin-import-host-estandar.md).
          - [x] Decisión tomada (T08-D04): NSC rechaza con la causa un `Runtime` con TLS estático y sin directorio de importación, directo o transportado; el diagnóstico es provisional y la capacidad completa sigue en T08-D04. [Opciones](registro-commits/t08-tls-sin-import-decision.md), [Evidencia](registro-commits/t08-d04-tls-sin-importaciones.md).
      - [ ] T08-002-09m7c1d2c: Resolver las dependencias transitivas de las DLL homónimas y comprobar el consumidor resultante. Resuelto con ensamblados privados y contextos de activación (T08-D05); queda el alias importado directamente (T08-D05b). [Evidencia](registro-commits/t08-d05-dll-homonimas-transitivas.md).
        - [x] Invalidar el bundle al cambiar una DLL transitiva del runtime C y comprobar reconstrucción y restauración en O0 y O3; la resolución de homónimas permanece pendiente. [Evidencia](registro-commits/t08-cache-runtime-transitivo.md).
        - [x] Decisión resuelta (T08-D05, manifiestos side-by-side sin modificar DLL): dos `api.dll` homónimas con dependencias `dep.dll` homónimas y distintas se rechazaban al empaquetar («runtime package contains different DLLs named 'dep.dll'»), sin elección arbitraria. Conservar ambas exige reescribir las importaciones de los alias, usar manifiestos side-by-side o mantener el rechazo como contrato. [Evidencia y opciones](registro-commits/t08-dll-homonimas-transitivas-decision.md), [Evidencia](registro-commits/t08-d05-dll-homonimas-transitivas.md).
    - [x] T08-002-09m7c1d3: Comprobar dos DLL TLS homónimas cuando el consumidor es una DLL NativeSharp, con hilos y caché en O0–O3. [Evidencia y límites](registro-commits/t08-dll-producto-tls-homonimas.md). Las dependencias transitivas homónimas de un producto DLL se aíslan desde T08-D05 [Evidencia](registro-commits/t08-d05-dll-homonimas-transitivas.md).
      - [x] Enlazar funciones C de nombre distinto al alias de cada DLL desde el producto DLL NativeSharp y comprobar que comparten la TLS, con hilos y caché en O0–O3. [Evidencia](registro-commits/t08-dll-producto-funciones-alias.md).
- [x] T08-002-09m7d: Comprobar los demás calificadores y variantes de enlace de la global externa compartida: `const volatile`, `int **`, arreglo fijo, `restrict`, `const int *const`, `_Alignas` y `dllimport` desde DLL, en O0–O3; rechazo de discrepancias de tamaño de arreglo, `volatile` y `restrict`. [Evidencia y límites](registro-commits/t08-global-externa-variantes.md).
  - [x] T08-002-09m7d1: Importar una global C `extern T arr[];` de tamaño desconocido como `T[*]` (indexación, decaimiento calificado, DLL, plantillas precompiladas, tipo compuesto `T a[]`/`T a[N]` entre plantillas), O0–O3, con rechazos. [Evidencia](registro-commits/t08-global-arreglo-incompleto.md).
  - [x] T08-002-09m7d2: Rechazar el uso transportado de una global cuya declaración en el productor no se pudo importar (`const int **`): el productor registra la declaración vista y el consumidor la conserva como estado no importable (sin cambio de NSMeta); unión con otro productor rechazada por ambigüedad, uso aislado rechazado con el motivo, sombras y otras globales O0–O3. [Evidencia](registro-commits/t08-global-no-importable.md).
- [x] T08-002-09n: Enlazar una sola vez una biblioteca C idéntica propagada por dos productores y conservar dos bibliotecas distintas con el mismo nombre, en O0. [Evidencia](registro-commits/t08-dependencia-c-compartida.md).
- **T08-002-10 — Sombras en plantillas:**
- [x] T08-002-10a: Verificar una muestra de sombra de función C por parámetro y local en una plantilla precompilada en O0. [Evidencia](registro-commits/t08-sombra-funcion-parametro-local.md).
- [x] T08-002-10b: Verificar una muestra de sombra de global C por variables de `foreach` y `catch`, en O0. [Evidencia](registro-commits/t08-sombra-foreach-catch-o0.md).
- [x] T08-002-10c: Verificar muestras de inicializadores de local, `for` y desestructuración antes de introducir el nombre sombreado, en O0. [Evidencia](registro-commits/t08-inicializadores-ambito-o0.md).
- **T08-002-11 — Publicación y conflictos:**
  - La comparación de una declaración enum observada desde el mismo buffer virtual conserva evidencia transitoria de su fuente. [Evidencia](registro-commits/t08-enum-fuente-virtual.md).
  - El lector de catálogos sin contexto valida la entrada y el módulo nuevo usa el contrato vigente; la prueba de sesión pasó. [Evidencia](registro-commits/t08-lectura-catalogo-contexto.md).
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

- Validación del bloque (2026-11-08): aprobada. [Evidencia](registro-commits/t08-003-validacion-bloque.md).

- [x] T08-003-01: Probar muestras de llamadas escalares y con punteros. [Detalle](TANDA-08-TIPOS-C.md).
- [x] T08-003-02: Probar funciones `static inline` y el camino ThinLTO indicado en el tracking principal.
  - La prueba de sesión distingue la incorporación de objetos C++ sin internalización y su rechazo con internalización. [Evidencia](registro-commits/t08-control-thinlto-internalizacion.md).
- [x] T08-003-03: Probar las muestras de declaraciones sin prototipo y promociones C11 registradas en el tracking principal.
- **T08-003-04 — Declaraciones y redeclaraciones:**
- [x] T08-003-04a: Verificar selección de prototipo compatible entre declaración adelantada y definición (repetida, typedef, `const` superior, arreglo y función ajustados, sin prototipo completada, `static inline`), directo y precompilado, O0–O3, con control Clang. [Evidencia](registro-commits/t08-funciones-declaraciones.md).
- [x] T08-003-04b: Verificar diagnóstico de redeclaraciones con parámetros o retorno incompatibles: mismo encabezado (Clang), dos encabezados, dos productores precompilados y aridad del prototipo compuesto. [Evidencia](registro-commits/t08-funciones-declaraciones.md).
- [x] T08-003-04c: Verificar funciones variádicas y llamadas sin prototipo con promociones por defecto (enteros estrechos, `bool`, `float`, `long`, puntero y record), directo y precompilado, O0–O3; rechazo de `struct` NativeSharp. [Evidencia](registro-commits/t08-funciones-declaraciones.md).
- [x] T08-003-04d: Verificar captura y símbolo de funciones `static inline` de encabezados distintos: ayudantes y `static` locales independientes, directo y con plantillas de dos productores sin encabezados, O0–O3. [Evidencia](registro-commits/t08-funciones-declaraciones.md).
  - [x] T08-003-04d1: Importar una función C `static` sin `inline` definida en un encabezado: directo, precompilado y dos productores, O0–O3, con rechazos de homónimas y de declaración sin cuerpo. [Evidencia](registro-commits/t08-funcion-static-encabezado.md).
- **T08-003-05 — Visibilidad y enlace:**
- [x] T08-003-05a: Verificar llamada externa desde namespace y resolución con un local homónimo (sombra por bloque y rechazo de llamar al local), O0–O3. [Evidencia](registro-commits/t08-funciones-visibilidad-enlace.md).
- [x] T08-003-05b: Verificar enlace de función C en biblioteca estática y DLL publicada junto al ejecutable, O0–O3, con control Clang y rechazo de símbolo no definido. [Evidencia](registro-commits/t08-funciones-visibilidad-enlace.md).
- [x] T08-003-05c: Verificar función usada por plantilla precompilada con encabezado presente y oculto, biblioteca estática y DLL, O0–O3. [Evidencia](registro-commits/t08-funciones-visibilidad-enlace.md).

## T08-004 — Variables globales C

- Validación del bloque (2026-11-08): aprobada; quedan dos decisiones pendientes y T08-004-05 para la validación integral. [Evidencia](registro-commits/t08-004-validacion-bloque.md).

- [x] T08-004-01: Comprobar las muestras de globales externas y `static` registradas en el tracking principal.
- [x] T08-004-02: Comprobar la matriz dirigida de escalares y punteros atómicos no TLS indicada en el tracking principal.
- [x] T08-004-03: Comprobar las muestras dirigidas de TLS estático, atómico y DLL registradas en el tracking principal.
- **T08-004-04 — Globales por tipo y calificador:**
- [x] T08-004-04a: Verificar globales escalares enteras y flotantes con `const` y `volatile` (todos los anchos, `_Bool`, `float`, `double`, `const volatile`), directo, DLL y precompilado, O0–O3, con control Clang, IR volatile y rechazos. [Evidencia](registro-commits/t08-globales-escalares-calificados.md).
  - [x] T08-004-04a1: Resuelta por T08-D06: `&` de una global C `const` (y de sus miembros) produce `T const*`, como en C; las pruebas que fijaban el rechazo se actualizaron. [Evidencia](registro-commits/t08-d06-direccion-global-const.md). Antes: permitir `&` de una global C `const` como `T const*`, como en C. Hoy se rechaza («cannot take the address of a qualified C/C++ global») y ese rechazo lo fijan `pruebas-proyecto/ejecutar-cpp.cmd`, `CppClangSessionTests` y `pruebas-tanda-08-record-c`. [Evidencia](registro-commits/t08-globales-escalares-calificados.md#decisión-pendiente-t08-004-04a1).
- [x] T08-004-04b: Verificar globales de enum, puntero a enum y record C (struct, union, `const`, `volatile`, punteros), directo, DLL y precompilado sin encabezado, O0–O3, con control Clang y rechazos. Corregida la siembra en el catálogo C de los records/enums que solo aparecen como tipo de una global usada por una plantilla. [Evidencia](registro-commits/t08-globales-enum-record.md).
- [x] T08-004-04c: Verificar arreglos globales y matrices con punteros calificados (`const char *[]`, `volatile int *[]`, `int *const []`, records y tres dimensiones), directo, DLL y precompilado, O0–O3, con control Clang y seis rechazos. [Evidencia](registro-commits/t08-globales-arreglos.md).
- [x] T08-004-04d: Verificar `_Atomic` en global externa y `static`, con lectura y escritura cruzadas (once tipos, accesores C `static inline`, IR seq_cst), directo y precompilado, O0–O3, con control Clang. [Evidencia](registro-commits/t08-globales-atomicas.md).
  - [x] T08-004-04d1: Resuelta por T08-D07: una global `_Atomic` de tipo record se importa con la política de Clang (sin bloqueo para 1, 2, 4 y 8 bytes; `__atomic_load`/`__atomic_store` para los demás). [Evidencia](registro-commits/t08-d07-atomicas-record.md). Antes: decisión pendiente entre solo records sin bloqueo o también la biblioteca atómica. [Límite](registro-commits/t08-globales-atomicas.md#límite-registrado-t08-004-04d1).
- [x] T08-004-04e: Verificar `_Thread_local` con y sin `_Atomic` en ejecutable y DLL (externas y `static` de encabezado, lectura C en el mismo hilo y aislamiento con un hilo creado por C), directo y precompilado, O0–O3, con control Clang. [Evidencia](registro-commits/t08-globales-tls.md).
- [ ] T08-004-05: Volver a ejecutar los controles TLS y de DLL dentro del gate integral de la tanda. Se ejecuta en la validación integral de la tanda 8.
  - Ejecutados en el gate integral del 2026-11-09: pasan salvo `directo\validar-relleno-cero.cmd` (carrera del control C, sin NSC) y `atomic\validar.cmd` (expectativa de caché retirada; pasa en ejecución derivada). [Evidencia](registro-commits/t08-validacion-integral-20261109.md).
- **T08-004-06 — Consumo y resolución:**
- [x] T08-004-06a: Verificar una global externa con encabezado en el consumidor: plantilla y código propio comparten el objeto, O0–O3. [Evidencia](registro-commits/t08-globales-consumo.md).
- [x] T08-004-06b: Verificar una global requerida por plantilla con encabezado oculto y metadata, O0–O3. [Evidencia](registro-commits/t08-globales-consumo.md).
- [x] T08-004-06c: Verificar ambigüedad de globales C homónimas y sombra por local, parámetro y variable de `for`, O0–O3, con rechazo de declaraciones incompatibles. [Evidencia](registro-commits/t08-globales-consumo.md).
- [x] T08-004-06d: Verificar identidad y separación de globales `static` con encabezados homónimos en dos productores, O0–O3, y rechazo de ambas vistas en un proyecto. [Evidencia](registro-commits/t08-globales-consumo.md).

## T08-005 — Typedef

- [x] T08-005-01: Probar las muestras de alias escalares encadenados y punteros registradas en el tracking principal.
- **T08-005-02 — Formas de typedef:**
- [x] T08-005-02a: Verificar cadenas de typedefs escalares con signos y anchos distintos. [Evidencia](registro-commits/t08-typedef-c.md).
- [x] T08-005-02b: Verificar typedef de puntero con `const` y `volatile` en el apuntado. [Evidencia](registro-commits/t08-typedef-c.md).
- [x] T08-005-02c: Verificar typedef de arreglo fijo y de puntero a función C. [Evidencia](registro-commits/t08-typedef-c.md).
- [x] T08-005-02d: Verificar typedef de struct y union, incluidos records anónimos. [Evidencia](registro-commits/t08-typedef-c.md).
- [x] T08-005-02e: Verificar typedef de enum nombrado y enum anónimo. [Evidencia](registro-commits/t08-typedef-c.md).
- **T08-005-03 — Reconciliación:**
- [x] T08-005-03a: Verificar typedef equivalente entre dos encabezados distintos. [Evidencia](registro-commits/t08-typedef-c.md).
  - [x] T08-005-03a1: Resuelta por T08-D09: un struct o union C definido de forma equivalente en dos encabezados (texto idéntico o encabezado común con guardas) se usa por valor (locales, copias, globales, campos, argumentos y retornos), directo y precompilado. [Evidencia](registro-commits/t08-d09-record-dos-encabezados.md). Antes: un struct C definido en dos encabezados (texto idéntico o encabezado común con guardas) solo se usa por puntero; `Pair p;` se rechaza («cannot be created by value until object lifetime is implemented»). La prueba unitaria «repeated C record observations do not claim value layout» fija ese contrato. Relacionado con T08-010-07.
- [x] T08-005-03b: Verificar conflicto por tipo subyacente incompatible. [Evidencia](registro-commits/t08-typedef-c.md).
  - [x] T08-005-03b1: Resuelta por T08-D10: cada plantilla transportada resuelve sus typedef C en el catálogo de su productor; un consumidor usa dos productores con typedef homónimos incompatibles, y su propio código conserva la ambigüedad. [Evidencia](registro-commits/t08-d10-typedef-productores.md). Antes: dos productores con typedef homónimos incompatibles no pueden usarse en un mismo consumidor ni a través de sus plantillas («conflicting C/C++ type alias»); lo fija `typedef-funcion-record/validar.py --incompatible`. Alternativa: resolver cada plantilla en el catálogo de su productor, como las constantes enum y las globales `static`.
- [x] T08-005-03c: Verificar consumo del typedef a través de metadata sin encabezado. Corregidos el cast a typedef C en plantilla pública y el typedef escalar idéntico entre consumidor y catálogo. [Evidencia](registro-commits/t08-typedef-c.md).
  - Evidencia previa: un cast a un typedef C en una plantilla pública (`(Color)2`, `(MyInt)2`) impide publicar la biblioteca («generated precompiled public template surface is not valid NativeSharp source»). [Hallazgo](registro-commits/t08-globales-enum-record.md#hallazgos-registrados).

## T08-006 — Enteros y `_Bool`

- [x] T08-006-01: Probar la muestra mixta de campo, arreglo y llamada `_Bool` O0–O3.
- [x] T08-006-02: Comparar campos C volátiles con Clang en los caminos directo y precompilado. [Resultado](registro-commits/t08-volatile-campos-revalidacion-alias-pendiente.md).
- **T08-006-03 — Anchuras y signo:**
- [x] T08-006-03a: Verificar `char`, `signed char` y `unsigned char` contra Clang en el target fijado. [Evidencia](registro-commits/t08-enteros-c.md).
- [x] T08-006-03b: Verificar `short` y `unsigned short` en parámetros, retorno y campo. [Evidencia](registro-commits/t08-enteros-c.md).
- [x] T08-006-03c: Verificar `int` y `unsigned int` en parámetros, retorno y campo. [Evidencia](registro-commits/t08-enteros-c.md).
- [x] T08-006-03d: Verificar `long`, `unsigned long`, `long long` y `unsigned long long` con los anchos reales de Clang. [Evidencia](registro-commits/t08-enteros-c.md).
- [x] T08-006-03e: Verificar representación `_Bool` en memoria y en firmas C. [Evidencia](registro-commits/t08-enteros-c.md).
- **T08-006-04 — Conversiones y ABI:**
- [x] T08-006-04a: Verificar extensión de signo o cero de parámetros y retornos estrechos. [Evidencia](registro-commits/t08-enteros-c.md).
- [x] T08-006-04b: Verificar promociones de enteros estrechos y `_Bool` en operaciones aritméticas y bitwise. [Evidencia](registro-commits/t08-enteros-c.md).
  - **Decisión pendiente T08-006-04b1:** `-` y `~` unarios sobre enteros estrechos conservan el tipo del operando en NativeSharp (`~ub == 0`, `-sb == -128`); C y C# promueven a `int` (-256, 128). Las constantes enum C sí se promueven.
- [x] T08-006-04c: Verificar almacenamiento y lectura cruzada de escalares en globales, campos y arreglos. [Evidencia](registro-commits/t08-enteros-c.md).
- [x] T08-006-04d: Verificar límites representables y conversiones entre firmado y sin signo. [Evidencia](registro-commits/t08-enteros-c.md).

## T08-007 — Punto flotante

- [x] T08-007-01: Probar `long double` IEEE binary64 de la muestra directa y precompilada O0–O3.
- **T08-007-02 — Tipos y formatos:**
- [x] T08-007-02a: Verificar tamaño y representación de `float` contra Clang. [Evidencia](registro-commits/t08-flotantes-c.md).
- [x] T08-007-02b: Verificar tamaño y representación de `double` contra Clang. [Evidencia](registro-commits/t08-flotantes-c.md).
- [x] T08-007-02c: Verificar `long double` y diagnóstico cuando su formato no sea representable. Corregido: una función C con un tipo flotante sin portador ya informa el motivo. [Evidencia](registro-commits/t08-flotantes-c.md).
- **T08-007-03 — Contextos:**
- [x] T08-007-03a: Verificar parámetros y retornos de cada tipo flotante. [Evidencia](registro-commits/t08-flotantes-c.md).
- [x] T08-007-03b: Verificar lectura y escritura de campos y globales flotantes. [Evidencia](registro-commits/t08-flotantes-c.md).
- [x] T08-007-03c: Verificar persistencia del formato y uso desde metadata precompilada. [Evidencia](registro-commits/t08-flotantes-c.md).

## T08-008 — `void`

- [x] T08-008-01: Probar el retorno `void`, `void*` y prototipo `f(void)` de la muestra C11.
- **T08-008-02 — Usos de `void`:**
- [x] T08-008-02a: Verificar `void` como retorno y `f(void)` sin parámetros. [Evidencia](registro-commits/t08-void-c.md).
- [x] T08-008-02b: Verificar `void*` en parámetro, retorno, campo y global. [Evidencia](registro-commits/t08-void-c.md).
- [x] T08-008-02c: Verificar conservación de `const void*` y `volatile void*` en conversiones. [Evidencia](registro-commits/t08-void-c.md).
- [x] T08-008-02d: Verificar restauración de firmas con `void*` desde metadata. [Evidencia](registro-commits/t08-void-c.md).

## T08-009 — Enumeraciones

- [x] T08-009-01: Probar las muestras de enum C y conversiones enum/entero registradas en el tracking principal.
- [x] T08-009-02: Probar el caso corregido de comparación `unsigned int` / `int`. [Evidencia](registro-commits/t08-enum-c-uint-int.md).
- **T08-009-03 — Valores y operaciones:**
- [x] T08-009-03a: Verificar valores negativos y límites del tipo entero subyacente. [Evidencia](registro-commits/t08-enum-c.md).
- [x] T08-009-03b: Verificar conversiones entre enum, `int`, `unsigned int` y enteros de mayor ancho. [Evidencia](registro-commits/t08-enum-c.md).
- [x] T08-009-03c: Verificar comparaciones mixtas con operandos firmados y sin signo. Corregido: un operando enum de base `int` frente a `unsigned int` sigue a C. [Evidencia](registro-commits/t08-enum-c.md).
  - **Decisión pendiente T08-009-03c1:** los enumeradores son constantes `int`; NativeSharp compara `int` con `unsigned int` ampliando a `long` (`-1 < 1u` verdadero) y C convierte a `unsigned int` (falso). Solo un operando de tipo enum C sigue la regla de C.
- [x] T08-009-03d: Verificar aritmética, desplazamientos y operaciones bitwise. Corregido: `op=` sobre un objeto enum C. [Evidencia](registro-commits/t08-enum-c.md).
- **T08-009-04 — Identidad y persistencia:**
- [x] T08-009-04a: Verificar reconciliación de enum nombrado en dos encabezados. [Evidencia](registro-commits/t08-enum-c.md).
- [x] T08-009-04b: Verificar colisión de enums anónimos con el mismo primer enumerador. [Evidencia](registro-commits/t08-enum-c.md).
- [x] T08-009-04c: Verificar enum anónimo como campo de record publicado. [Evidencia](registro-commits/t08-enum-c.md).
- [x] T08-009-04d: Verificar identidad y valores de enum anidado restaurado desde metadata. [Evidencia](registro-commits/t08-enum-c.md).
- **T08-009-05 — Consumo:**
- [x] T08-009-05a: Verificar enum en firma pública con encabezado disponible. [Evidencia](registro-commits/t08-enum-c.md).
- [x] T08-009-05b: Verificar la misma firma y constantes con encabezado oculto. [Evidencia](registro-commits/t08-enum-c.md).
- [x] T08-009-05c: Verificar conflictos entre definición del encabezado y catálogo precompilado. [Evidencia](registro-commits/t08-enum-c.md).

## T08-010 — Struct C

- [x] T08-010-01: Comparar las muestras de layout natural, empaquetado y anidado contra Clang. [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
- [x] T08-010-02: Probar muestras de miembros anónimos y anidados en los caminos indicados en el tracking principal.
- [x] T08-010-03: Probar muestras de miembro flexible, lectura/escritura y conversión a puntero O0–O3 indicadas en el tracking principal.
- **T08-010-04 — Miembros de struct:**
- [x] T08-010-04a: Verificar campos escalares enteros, flotantes, enum y `_Bool`. [Evidencia](registro-commits/t08-record-miembros.md).
- [x] T08-010-04b: Verificar campos puntero y puntero a función, incluidos calificadores. [Evidencia](registro-commits/t08-record-miembros.md).
  - Corregido: los campos C `volatile T *` (incluido `volatile void *`) se importan con la calificación del apuntado, y el acceso por el puntero leído sigue el modo volatile del record (acquire/release en Microsoft, simple en ISO). [Hallazgo previo](registro-commits/t08-void-c.md#hallazgo-para-t08-010-04b).
- [x] T08-010-04c: Verificar arreglos fijos y miembro flexible al final del struct. [Evidencia](registro-commits/t08-record-miembros.md).
- [x] T08-010-04d: Verificar campos de bits con y sin signo, anónimos y de ancho cero. [Evidencia](registro-commits/t08-record-miembros.md).
  - Corregidos: la máscara de preservación se creaba con bits fuera de la unidad de asignación y detenía el backend en -O1; los campos cuya ventana mínima cruzaba el final del record no se importaban.
- [x] T08-010-04e: Verificar promoción de miembros de struct y union anónimos en varios niveles. [Evidencia](registro-commits/t08-record-miembros.md).
- [x] T08-010-04f: Verificar struct anidado en `pack(1)`, `pack(2)` y `pack(4)`. [Evidencia](registro-commits/t08-record-miembros.md).
- [x] T08-010-05: Resolver el orden de acceso mediante alias a miembro flexible `volatile` en modo Microsoft e ISO. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - Resuelta T08-010-05a por T08-D13 (opción 1): el alias sigue el modo del record o encabezado de origen, como el acceso directo. Con Microsoft (y Default) emite acquire/release como Clang `-fms-volatile`; con ISO, accesos simples como `-fno-ms-volatile`. Antes: el alias perdía el orden en modo Microsoft.
- **T08-010-06 — Caminos de consumo:**
- [x] T08-010-06a: Verificar lectura y escritura cruzadas para miembros anónimos por encabezado directo. [Evidencia](registro-commits/t08-record-miembros.md).
- [x] T08-010-06b: Verificar los mismos miembros con biblioteca precompilada y encabezado. [Evidencia](registro-commits/t08-record-miembros.md).
- [x] T08-010-06c: Verificar los mismos miembros con biblioteca precompilada y solo metadata. [Evidencia](registro-commits/t08-record-miembros.md).
- [x] T08-010-06d: Verificar campos flexibles calificados en esos tres caminos. [Evidencia](registro-commits/t08-record-miembros.md).
- [x] T08-010-07: Validar los modos Microsoft e ISO con dos referencias al mismo encabezado en la misma compilación. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - Sin modo único, el alias es `volatile` simple, igual que el acceso directo al mismo almacenamiento.
- [x] T08-010-08: Verificar el alias volátil reasignado en los cuatro niveles de optimización después de la corrección. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- [x] T08-010-09: Verificar lectura y escritura `*(data + index)` frente a Clang después de la corrección. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- [x] T08-010-10: Publicar en metadata un struct C declarado solo con etiqueta (`struct Holder { … };`, sin typedef): el consumidor precompilado sin encabezado informa `unknown type name 'Holder'`, mientras el camino directo funciona. [Origen](registro-commits/t08-puntero-funcion-calificado.md). [Evidencia](registro-commits/t08-record-etiqueta.md).
  - Corregido: las plantillas publicadas siembran en el catálogo C los records, unions y enums nombrados solo por su etiqueta.

## T08-011 — Union C

- [x] T08-011-01: Probar muestras de miembros superpuestos, campos de bits y unions empaquetadas indicadas en el tracking principal.
- **T08-011-02 — Formas de union:**
- [x] T08-011-02a: Verificar union natural con miembros enteros, flotantes y puntero. [Evidencia](registro-commits/t08-union-c.md).
- [x] T08-011-02b: Verificar union empaquetada con miembros desalineados. [Evidencia](registro-commits/t08-union-c.md).
- [x] T08-011-02c: Verificar union dentro de struct y struct dentro de union. [Evidencia](registro-commits/t08-union-c.md).
  - [x] T08-011-02c1: Resuelta por T08-D14: el tipo sin etiqueta de un miembro con nombre (también como elemento de arreglos y punteros, y compartido por varios miembros) recibe la identidad sintetizada `Owner::__nsc_c_member_<miembro>`, directo y precompilado. [Evidencia](registro-commits/t08-d14-miembro-sin-etiqueta.md). Antes: un miembro con nombre cuyo tipo es un struct o union sin etiqueta no se importa (falta en el camino directo; en el precompilado hace fallar la generación de la biblioteca). Opciones: identidad sintetizada en metadata, omisión coherente o rechazo explícito. [Detalle](registro-commits/t08-union-c.md#decisión-pendiente-t08-011-02c1).
- [x] T08-011-02d: Verificar union anónima con miembros promovidos y campos de bits superpuestos. [Evidencia](registro-commits/t08-union-c.md).
- **T08-011-03 — Validación:**
- [x] T08-011-03a: Comparar tamaño, alineación y offsets con Clang. [Evidencia](registro-commits/t08-union-c.md).
- [x] T08-011-03b: Comparar bytes tras escribir por cada miembro y leer por otro. [Evidencia](registro-commits/t08-union-c.md).
- [x] T08-011-03c: Repetir el control con productor precompilado y sin encabezado en el consumidor. [Evidencia](registro-commits/t08-union-c.md).

## T08-012 — Layout obtenido de Clang

- [x] T08-012-01: Comparar los tamaños, alineaciones de record y offsets de la muestra de layout. [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
- [x] T08-012-02: Resolver y verificar el contrato de alineación efectiva de cada campo frente a Clang. [Evidencia](registro-commits/t08-d15-alineacion-efectiva.md).
  - Verificado el contrato vigente: en 56 accesos ordinarios NSC = min(alineación de Clang, natural) y nunca la excede; los campos de bits usan ventanas más estrechas con alineación no mayor. [Evidencia](registro-commits/t08-layout-c.md).
  - Resuelta T08-012-02a por T08-D15: se adopta la alineación de Clang, `gcd(alineación del record, offset)`, donde está garantizada (objetos y elementos del record); un puntero a miembro conserva la natural. Se calcula con los datos que NSMeta ya transporta. [Evidencia](registro-commits/t08-d15-alineacion-efectiva.md).
- **T08-012-03 — Cobertura de layout:**
- [x] T08-012-03a: Comparar offset y alineación de campos escalares naturales y bajo `pack(1/2/4)`. [Evidencia](registro-commits/t08-layout-c.md).
- [x] T08-012-03b: Comparar layout de arreglos de records y records anidados. [Evidencia](registro-commits/t08-layout-c.md).
- [x] T08-012-03c: Comparar ventanas y alineación de campos de bits con y sin nombre. [Evidencia](registro-commits/t08-layout-c.md).
- [x] T08-012-03d: Comparar `_Alignas` y typedefs con alineación explícita. [Evidencia](registro-commits/t08-layout-c.md).
- [x] T08-012-03e: Repetir las comparaciones con información restaurada de metadata. [Evidencia](registro-commits/t08-layout-c.md).

## T08-013 — Tipos incompletos

- [x] T08-013-01: Probar las muestras de punteros a structs y unions opacos en uso directo y precompilado.
- **T08-013-02 — Declaración y definición:**
- [x] T08-013-02a: Verificar struct adelantado y definición posterior en el mismo encabezado. [Evidencia](registro-commits/t08-incompletos-c.md).
- [x] T08-013-02b: Verificar union adelantada y definición posterior en otro encabezado visible. [Evidencia](registro-commits/t08-incompletos-c.md).
- [x] T08-013-02c: Verificar conciliación de tipo opaco entre encabezado y metadata. [Evidencia](registro-commits/t08-incompletos-c.md).
  - Resuelta T08-013-02c1 por T08-D16: la declaración adelantada propia del consumidor se concilia por etiqueta y USR con el record que publicó el productor, opaco o completo. [Evidencia](registro-commits/t08-d16-opaco-consumidor.md).
- [x] T08-013-02d: Verificar rechazo de definiciones incompatibles con la misma identidad. [Evidencia](registro-commits/t08-incompletos-c.md).
- **T08-013-03 — Operaciones:**
- [x] T08-013-03a: Verificar paso, retorno y almacenamiento de `Opaque*` y `Opaque**`. [Evidencia](registro-commits/t08-incompletos-c.md).
  - Corregido: los campos C `Record **` (punteros intermedios sin calificar) se importan.
- [x] T08-013-03b: Verificar rechazo de `sizeof`, variable por valor y acceso a miembro de tipo incompleto. [Evidencia](registro-commits/t08-incompletos-c.md).
- [x] T08-013-03c: Verificar rechazo de aritmética e indexación de `Opaque*` y admisión aplicable a `Opaque**`. [Evidencia](registro-commits/t08-incompletos-c.md).

## T08-014 — Punteros

- [x] T08-014-01: Probar la muestra `_Bool*` devuelta, almacenada y seleccionada que documenta el tracking principal.
- **T08-014-02 — Tipos y conversiones:**
- [x] T08-014-02a: Verificar punteros a enteros, flotantes y `_Bool`. [Evidencia](registro-commits/t08-punteros-c.md).
- [x] T08-014-02b: Verificar punteros a struct, union, enum y `void`. [Evidencia](registro-commits/t08-punteros-c.md).
- [x] T08-014-02c: Verificar `T**` y conservación de calificadores en cada nivel representado. [Evidencia](registro-commits/t08-punteros-c.md).
  - Límite de representación fijado: un parámetro con calificador en el segundo nivel o en un puntero intermedio se difiere con «parameter 0 is not yet mapped». Corregido: los campos `T **` de base escalar se importan.
- [x] T08-014-02d: Verificar conversiones compatibles y diagnósticos por pérdida de calificadores. [Evidencia](registro-commits/t08-punteros-c.md).
  - Resuelta T08-014-02d1 por T08-D17: `int const** q = &p` se rechaza; en un puntero a puntero, `T const**` califica el tipo base, como en C. [Evidencia](registro-commits/t08-d17-puntero-a-puntero-const.md).
- **T08-014-03 — Contextos:**
- [x] T08-014-03a: Verificar punteros como parámetros y retornos de funciones C. [Evidencia](registro-commits/t08-punteros-c.md).
- [x] T08-014-03b: Verificar punteros en campos y globales C. [Evidencia](registro-commits/t08-punteros-c.md).
- [x] T08-014-03c: Verificar uso y restauración de punteros desde biblioteca precompilada. [Evidencia](registro-commits/t08-punteros-c.md).
- **T08-014-04 — Contratos:**
- [x] T08-014-04a: Verificar alineación de puntero a campo natural y empaquetado. [Evidencia](registro-commits/t08-punteros-c.md).
- [x] T08-014-04b: Verificar nulabilidad y diagnósticos de desreferencia aplicables. [Evidencia](registro-commits/t08-punteros-c.md).
- [x] T08-014-04c: Verificar propagación de `const` y `volatile` en inicialización y reasignación. [Evidencia](registro-commits/t08-punteros-c.md).

## T08-015 — Calificadores

- [x] T08-015-01: Probar las muestras registradas de `const` y `volatile` en globales, campos y arreglos.
- [x] T08-015-02: Comparar los campos volátiles C contra Clang en los casos del fixture. [Resultado](registro-commits/t08-volatile-campos-revalidacion-alias-pendiente.md).
- **T08-015-03 — Modo de la unidad consumidora:**
- [x] T08-015-03a: Definir el modo predeterminado del consumidor cuando no se especifica uno explícito. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - Con T08-D13 el consumidor no tiene modo: rige el `VolatileMode` de cada `CppLibraryReference` (Default es el del destino, Microsoft en Windows). La propiedad `CppConsumerVolatileMode` se rechaza.
- [x] T08-015-03b: Verificar selección explícita de Microsoft e ISO por proyecto. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - La selección es por referencia (`VolatileMode`), verificada en Default, Microsoft e ISO, directo, precompilado y en grafo.
- [x] T08-015-03c: Conservar la selección por archivo en un grafo con múltiples proyectos. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - Corregido: en un grafo de proyectos vivos que importan el mismo encabezado, la global C declarada en los dos con modos distintos era ambigua y el typedef de record era «conflicting C/C++ type alias». Ahora el código de cada proyecto usa su propia referencia.
- [x] T08-015-03d: Resolver el modo del consumidor cuando dos referencias usan modos distintos. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - Modo desconocido: `volatile` simple, igual que el acceso directo.
- **T08-015-04 — Alias y alineación:** [Diagnóstico](registro-commits/t08-volatile-alias-aritmetica-diagnostico.md).
- [x] T08-015-04a: Conservar el contrato C volátil al inferir un alias local. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- [x] T08-015-04b: Conservar el orden y la alineación al leer y escribir mediante el alias. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- [x] T08-015-04c: Verificar el contrato después de reasignar el alias. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- [x] T08-015-04d: Verificar el contrato después de aritmética e indexación de punteros. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- [x] T08-015-04e: Verificar que un campo empaquetado o subalineado no reciba alineación atómica no demostrada. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - Un miembro flexible empaquetado no tiene alias (`var p = v->items` se rechaza); el alias de un miembro flexible alineado toma la alineación del campo, como C y el acceso directo.
- **T08-015-05 — Transporte del contrato:**
- [ ] T08-015-05a: Verificar alias `volatile` pasado como parámetro y retornado por función. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - Verificado: el alias pasado a un parámetro C conserva el orden (lo aplica C); un parámetro NativeSharp `int*` no lo recibe («no viable overload»), porque NativeSharp no escribe `T volatile*` en firmas propias.
  - **Pendiente funcional:** un puntero `volatile T*` devuelto por una función C no inicia el contrato. En modo Microsoft NSC emite `volatile` simple y Clang, acquire/release: los símbolos de función C no llevan el modo de su referencia. Alternativas en la evidencia.
- [x] T08-015-05b: Verificar alias reasignado en ramas de control con modos compatibles. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - Corregido: reasignar desde un miembro flexible (`p = b->items`) borraba el modo.
- [x] T08-015-05c: Verificar propagación entre proyectos y biblioteca precompilada. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- **T08-015-06 — Caché y aislamiento:**
- [x] T08-015-06a: Verificar invalidación o identidad de caché al cambiar Microsoft/ISO. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- [x] T08-015-06b: Verificar que `volatile` nativo conserve su contrato sin importar un header C. [Evidencia](registro-commits/t08-calificadores-c.md).
- [x] T08-015-06c: Verificar el mismo comportamiento nativo al importar un header C. [Evidencia](registro-commits/t08-calificadores-c.md).
- **T08-015-07 — Calificadores de puntero:**
- [x] T08-015-07a: Verificar rechazo de escritura mediante `const T*` importado. [Evidencia](registro-commits/t08-calificadores-c.md).
- [x] T08-015-07b: Verificar acceso `volatile T*` en lectura, escritura e índice. [Evidencia](registro-commits/t08-calificadores-c.md).
  - El orden acquire/release a través del alias `var` en modo Microsoft quedó resuelto por T08-D13.
- [x] T08-015-07c: Verificar `const volatile T*` en campos, globales y parámetros. [Evidencia](registro-commits/t08-calificadores-c.md).
- [x] T08-015-07d: Verificar calificadores del apuntado y del puntero en `T* const` y `T**`. [Evidencia](registro-commits/t08-calificadores-c.md).
- [x] T08-015-08: Habilitar la generación con modo explícito únicamente después de completar su contrato y retirar la protección temporal con pruebas. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - El modo explícito es el `VolatileMode` de la referencia y ya genera código. La protección temporal de `CppConsumerVolatileMode` («not yet supported by code generation») pasa a rechazo definitivo con la causa, probado.
- [x] T08-015-09: Repetir la comparación de IR de alias directo, reasignado y con aritmética en Microsoft e ISO O0–O3. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - `flexible-c/diagnosticar-var-volatile.cmd` da ahora `IGUAL` en todos los casos de Microsoft e ISO en O0–O3.
- [x] T08-015-10: Verificar el comportamiento con referencias mixtas y con biblioteca precompilada. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- [x] T08-015-11: Condicional entre dos miembros flexibles C (`var p = c ? a->items : b->items;`). [Evidencia](registro-commits/t08-015-11-condicional-flexible.md).
  - Corregido (hallado en T08-D13): cada rama miembro flexible o `extern T a[]` decae a `T*` frente a otro arreglo igual o a un `T*`, con el contrato volatile de origen; IR igual a Clang en los dos modos. Antes: «operand of * must be a non-void pointer».

## T08-016 — Arreglos fijos

- [x] T08-016-01: Probar las muestras de arreglos y matrices empaquetadas registradas en el tracking principal.
- [x] T08-016-02: Probar las muestras de arreglos de punteros calificados y records indicadas en el tracking principal.
- **T08-016-03 — Formas de arreglo:**
- [x] T08-016-03a: Verificar arreglo fijo de escalares enteros y flotantes. [Evidencia](registro-commits/t08-arreglos-c.md).
- [x] T08-016-03b: Verificar matriz multidimensional de escalares. [Evidencia](registro-commits/t08-arreglos-c.md).
- [x] T08-016-03c: Verificar arreglo de structs, unions y enums. [Evidencia](registro-commits/t08-arreglos-c.md).
- [x] T08-016-03d: Verificar arreglo de punteros `const` y punteros a datos `const` o `volatile`. [Evidencia](registro-commits/t08-arreglos-c.md), [campos](registro-commits/t08-d23-arreglos-punteros-campo.md).
  - Verificado en globales C (`int *const[]`, `const int *[]`, `volatile int *[]`).
  - Resuelta T08-016-03d1 por T08-D23: como campo de record C se importan, directo y precompilado, con las mismas reglas que las globales (incluido `pack(1)`, matrices y datos escalares, enum, record y `char`). [Evidencia](registro-commits/t08-d23-arreglos-punteros-campo.md). Antes: como campo de record no se importaban (faltaba el miembro en directo; el precompilado no reconstruía el record).
- [x] T08-016-03e: Verificar arreglos dentro de records empaquetados y anidados. [Evidencia](registro-commits/t08-arreglos-c.md).
- **T08-016-04 — Acceso y consumo:**
- [x] T08-016-04a: Comparar tamaño, offsets y alineación de elementos con Clang. [Evidencia](registro-commits/t08-arreglos-c.md).
- [x] T08-016-04b: Verificar lecturas y escrituras cruzadas de arreglos y matrices. [Evidencia](registro-commits/t08-arreglos-c.md).
- [x] T08-016-04c: Verificar toma de dirección y diagnóstico cuando el elemento está subalineado. [Evidencia](registro-commits/t08-arreglos-c.md).
- [x] T08-016-04d: Repetir el acceso con encabezado oculto y metadata precompilada. [Evidencia](registro-commits/t08-arreglos-c.md).

## T08-017 — Punteros a función C

- [x] T08-017-01: Probar las muestras registradas de callbacks escalares y callbacks con record o enum.
- [x] T08-017-02: Probar las muestras precompiladas de callbacks con y sin encabezado indicadas en el tracking principal.
  - [x] T08-017-02a: `pruebas-tanda-08-callback/validar-record-precompilado.cmd` falla con `conflicting C/C++ type alias 'RecordCallback'` (tipos idénticos, `reconciled=no`); observado el 2026-11-08 durante la regresión de [t08-callback-volatile-identidad.md](registro-commits/t08-callback-volatile-identidad.md). Causa no determinada. Corregido: reconciliación de typedef de puntero a función con records conciliados entre proyecto vivo y precompilado. [Evidencia](registro-commits/t08-callbacks-c.md).
- **T08-017-03 — Firmas y convenciones:**
- [x] T08-017-03a: Verificar callback con parámetros y retorno escalares. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-03b: Verificar callback con parámetros o retorno de puntero a record y enum. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-03c: Verificar callback variádico C y su identidad. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-03d: Verificar callback `vectorcall` y su convención de llamada. [Evidencia](registro-commits/t08-d22-vectorcall.md).
  - Resuelta T08-017-03d1 por T08-D22: funciones y callbacks C `__vectorcall` con la clasificación de Clang (HFA/HVA en registros o por referencia), directo, DLL y precompilado; una función C con la otra convención se rechaza. [Evidencia](registro-commits/t08-d22-vectorcall.md). Antes: `__vectorcall` no soportado; NSC lo rechazaba con diagnóstico explícito.
- **T08-017-04 — Almacenamiento y calificadores:**
- [x] T08-017-04a: Verificar callback almacenado en campo de struct C. [Evidencia](registro-commits/t08-callbacks-c.md).
  - Resuelta T08-017-04a1 por T08-D18: una función C convertida a puntero C es su propia dirección (`s->op == mul` en C); solo los métodos NativeSharp conservan `__c_boundary`. [Evidencia](registro-commits/t08-d18-identidad-funcion-c.md).
- [x] T08-017-04b: Verificar callback almacenado en global C. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-04c: Verificar callback retornado por función C. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-04d: Verificar lectura/escritura de callback `volatile` alineado y empaquetado. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-04e: Verificar rechazo de escritura sobre un callback almacenado `const`. [Evidencia](registro-commits/t08-callbacks-c.md).
- **T08-017-05 — Metadata:**
- [x] T08-017-05a: Verificar publicación de firma y convención del callback en metadata. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-05b: Verificar invocación del callback restaurado con encabezado visible. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-05c: Verificar invocación del callback restaurado con encabezado oculto. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-06: Conciliar las notas históricas contradictorias sobre callbacks volátiles almacenados con el comportamiento y las pruebas actuales. [Evidencia](registro-commits/t08-callbacks-c.md).
- [x] T08-017-07: Conciliar el rechazo histórico del método que retorna callback precompilado con la validación posterior del mismo camino. [Evidencia](registro-commits/t08-callbacks-c.md).

## T08-018 — Colisiones de nombres

- [x] T08-018-01: Probar la muestra de sombra local de global y enumerador C en O0.
- **T08-018-02 — Colisiones y ámbitos:**
- [x] T08-018-02a: Verificar colisión entre función C y función NativeSharp en namespace. [Evidencia](registro-commits/t08-colisiones-c.md).
- [x] T08-018-02b: Verificar global y enumerador C ocultados por local o parámetro NativeSharp. [Evidencia](registro-commits/t08-colisiones-c.md).
- [x] T08-018-02c: Verificar colisión entre typedef C y tipo NativeSharp. [Evidencia](registro-commits/t08-colisiones-c.md).
- [x] T08-018-02d: Verificar sombras en bloques, bucles, `catch` y lambdas. [Evidencia](registro-commits/t08-colisiones-c.md).
- **T08-018-03 — Resolución:**
- [x] T08-018-03a: Verificar resolución antes, dentro y después del ámbito de sombra. [Evidencia](registro-commits/t08-colisiones-c.md).
- [x] T08-018-03b: Verificar diagnóstico de ambigüedad entre dos declaraciones C visibles. [Evidencia](registro-commits/t08-colisiones-c.md).
- [x] T08-018-03c: Repetir los casos de función y global desde plantilla precompilada. [Evidencia](registro-commits/t08-colisiones-c.md).
  - Evidencia previa: un campo estático privado usado por una plantilla pública no viaja con ella; si el encabezado del productor declara una global C homónima importable, la plantilla se une en silencio a la global en vez del campo. [Sonda](registro-commits/t08-global-no-importable.md#observación-para-t08-018-03c).
  - Resuelta T08-018-03c1 por T08-D19: NSMeta publica, con su accesibilidad, los miembros no públicos que nombra una plantilla publicada; la plantilla los resuelve en el consumidor en vez de unirse a la declaración C. [Evidencia](registro-commits/t08-d19-dependencias-privadas.md).

## T08-019 — Diagnósticos de tipos incompletos

- [x] T08-019-01: Probar diagnósticos para aritmética, incremento e índice de records C incompletos. [Evidencia](registro-commits/aritmetica-puntero-incompleto-c.md).
- [x] T08-019-02: Probar el diagnóstico de local incompleto por valor y la muestra de `sizeof`. [Evidencia](registro-commits/diagnostico-local-incompleto-c.md).
- **T08-019-03 — Operaciones inválidas:**
- [x] T08-019-03a: Verificar diagnóstico de `sizeof` y `alignof` sobre struct y union incompletos. [Evidencia](registro-commits/t08-incompletos-diag-c.md).
- [x] T08-019-03b: Verificar diagnóstico de campo o variable local incompleta por valor. [Evidencia](registro-commits/t08-incompletos-diag-c.md).
- [x] T08-019-03c: Verificar diagnóstico de acceso a miembro no disponible. [Evidencia](registro-commits/t08-incompletos-diag-c.md).
- [x] T08-019-03d: Verificar diagnóstico de aritmética, incremento e índice sobre puntero a incompleto. [Evidencia](registro-commits/t08-incompletos-diag-c.md).
- [x] T08-019-03e: Verificar diagnóstico de parámetro o retorno incompleto por valor. [Evidencia](registro-commits/t08-incompletos-diag-c.md).
- [x] T08-019-04: Verificar que los diagnósticos identifican la declaración y la operación inválida también con metadata. [Evidencia](registro-commits/t08-incompletos-diag-c.md).

## T08-020 — Comparación de layout con Clang

- [x] T08-020-01: Comparar la muestra de 55 valores de layout y lecturas/escrituras cruzadas. [Evidencia](registro-commits/t08-layout-comparacion-clang.md).
- **T08-020-02 — Comparación de layout:**
- [x] T08-020-02a: Comparar tamaño, alineación y offsets de structs naturales y `pack(1/2/4)`. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
- [x] T08-020-02b: Comparar unions naturales y empaquetadas con miembros superpuestos. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
- [x] T08-020-02c: Comparar structs y unions anónimos o anidados. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
- [x] T08-020-02d: Comparar arreglos de records y miembros flexibles. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
- **T08-020-03 — Comparación de IR:**
- [x] T08-020-03a: Comparar alineación de `load` y `store` de campos escalares. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
  - Desde T08-D15, NSC emite la alineación de Clang en los accesos donde está garantizada. [Evidencia](registro-commits/t08-d15-alineacion-efectiva.md).
- [x] T08-020-03b: Comparar ventanas de lectura y escritura de bitfields. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
- [x] T08-020-03c: Comparar accesos `volatile` alineados y empaquetados en modos Microsoft e ISO.
  - Verificado el acceso directo a campos `volatile` alineados y empaquetados en modos Microsoft e ISO, directo y desde plantilla precompilada: idéntico a Clang. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
  - Por alias NativeSharp (T08-D13): los alineados son iguales a Clang en los dos modos; el miembro flexible empaquetado no tiene alias; el campo puntero de un struct `pack(1)` apunta a un dato alineado y es atómico en Microsoft, como Clang. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- **T08-020-04 — Tipos desde metadata:**
- [x] T08-020-04a: Verificar `sizeof(T)` y `alignof(T)` para un struct C publicado sin encabezado. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
- [x] T08-020-04b: Verificar declaración y cast de puntero a tipo C publicado sin encabezado. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
- [x] T08-020-04c: Verificar `offsetof` y acceso a miembro restaurado desde metadata. [Evidencia](registro-commits/t08-layout-comparacion-c.md).
- [x] T08-020-04d: Repetir los controles para union, enum y typedef publicados. [Evidencia](registro-commits/t08-layout-comparacion-c.md).

## T08-D — Resolución de las decisiones del usuario (2026-11-09)

Decisiones recibidas el 2026-11-09. Cada casilla se cierra solo con la capacidad implementada y probada; un diagnóstico es provisional mientras falte la capacidad.

- [x] T08-D01: DLL NativeSharp con nombre no ASCII, cargada por ruta Unicode (T08-002-09m7c0g2d). [Evidencia](registro-commits/t08-d01-dll-unicode.md).
- [x] T08-D02: NSMeta conserva `const`/`volatile` de alias C en firmas públicas de métodos y delegates. [Evidencia](registro-commits/t08-d02-d03-calificadores-nsmeta.md).
- [x] T08-D03: firma pública con callback `volatile`, publicada y consumida conservando el calificador. [Evidencia](registro-commits/t08-d02-d03-calificadores-nsmeta.md).
- [ ] T08-D04: TLS en DLL sin importaciones, con solución reproducible y sin importación artificial. [Evidencia](registro-commits/t08-d04-tls-sin-importaciones.md).
  - [x] T08-D04a: diagnóstico al consumir (directo y precompilado) sin modificar la DLL; `validar-variadicas-agregados.py` deja de usar DLL con TLS sin inicializar.
  - [ ] T08-D04b: copia distribuida con directorio de importación vacío, opt-in por referencia. **Pendiente de aprobación del usuario:** el cambio concreto y sus efectos (identidad, firma, CheckSum, alcance) están presentados en la evidencia. Revisión del prototipo, plan de implementación y pruebas: [preparación](registro-commits/t08-d04b-d05b-soluciones-preparadas.md).
- [ ] T08-D05: DLL homónimas con dependencias transitivas. [Evidencia](registro-commits/t08-d05-dll-homonimas-transitivas.md).
  - [x] T08-D05a: alias TLS cargados por el cargador de alias: cada uno recibe un ensamblado privado `NativeSharp.Isolation.*` y un manifiesto de contexto de activación; ejecutable y producto DLL, O0–O3, caché; sin modificar DLL.
  - [ ] T08-D05b: alias importado directamente (función exclusiva de una de las DLL homónimas) con dependencias homónimas: requiere enlazar en ejecución esas funciones cuando el alias necesita aislamiento; hoy se rechaza con la causa. Solución A (enlace en ejecución, sin alterar DLL; alcance estricto recomendado), solución B (manifiesto embebido, altera la DLL), pruebas y efectos: [preparación](registro-commits/t08-d04b-d05b-soluciones-preparadas.md). **Pendiente de la decisión del usuario.**
- [x] T08-D06: `&` de global C `const` produce un puntero a dato constante; la escritura se rechaza (T08-004-04a1). [Evidencia](registro-commits/t08-d06-direccion-global-const.md).
- [x] T08-D07: `_Atomic` sobre record, tamaños directos y con biblioteca atómica (T08-004-04d1); también TLS, `volatile`, `static` del encabezado y DLL. [Evidencia](registro-commits/t08-d07-atomicas-record.md).
- [x] T08-D08: validadores `.cmd` versionados con LF pasan a CRLF: `.gitattributes` fija `eol=crlf` para `.cmd`/`.bat` y los 176 archivos en LF del árbol de trabajo se convirtieron. [Evidencia](registro-commits/t08-d08-cmd-crlf.md).
- [x] T08-D09: struct C equivalente en dos encabezados, usado por valor (T08-005-03a1). [Evidencia](registro-commits/t08-d09-record-dos-encabezados.md).
- [x] T08-D10: typedef C homónimos e incompatibles de productores distintos (T08-005-03b1). [Evidencia](registro-commits/t08-d10-typedef-productores.md).
- [ ] T08-D11: `-` y `~` sobre enteros estrechos: reglas de NativeSharp frente a expresiones C (T08-006-04b1). [Investigación](registro-commits/t08-d11-d12-semantica-c-investigacion.md).
  - Sin cambio de reglas, por instrucción del usuario. La sonda `enteros-c/sonda-semantica-c.py` fija 38 casos C/NSC. La investigación muestra que el tipo no distingue un entero de C y compara cinco delimitaciones del «operando C» (origen directo con regla A o B, tipos C distintos, diagnóstico, contexto explícito) con sus efectos en expresiones mixtas. **Pendiente de la elección del usuario.**
- [ ] T08-D12: `int` frente a `unsigned int` en operaciones C (T08-009-03c1). [Investigación](registro-commits/t08-d11-d12-semantica-c-investigacion.md).
  - Igual que T08-D11: el mismo análisis y la misma sonda. **Pendiente de la elección del usuario.**
- [ ] T08-D13: modo `volatile` del alias NativeSharp a almacenamiento C (T08-010-05a). [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
  - [x] T08-D13a: el alias sigue el modo del record, la global o la referencia de origen, verificado frente al acceso directo y a Clang (directo O0–O3, precompilado, referencias mixtas, grafo, caché y ejecución). La propiedad `CppConsumerVolatileMode` se rechaza.
  - [ ] T08-D13b: puntero `volatile T*` devuelto por una función C (T08-015-05a). Pendiente funcional, con diagnóstico y alternativas.
- [x] T08-D14: miembro de tipo struct o union sin etiqueta (T08-011-02c1). [Evidencia](registro-commits/t08-d14-miembro-sin-etiqueta.md).
- [x] T08-D15: alineación efectiva de Clang, usada solo cuando está garantizada (T08-012-02a). [Evidencia](registro-commits/t08-d15-alineacion-efectiva.md).
- [x] T08-D16: declaración opaca propia del consumidor reconciliada con NSMeta (T08-013-02c1). [Evidencia](registro-commits/t08-d16-opaco-consumidor.md).
- [x] T08-D17: rechazo de `int const** q = &p` (T08-014-02d1). [Evidencia](registro-commits/t08-d17-puntero-a-puntero-const.md).
- [x] T08-D18: dirección de función C frente al envoltorio `__c_boundary` (T08-017-04a1). [Evidencia](registro-commits/t08-d18-identidad-funcion-c.md).
- [x] T08-D19: dependencias privadas de plantillas públicas en NSMeta (T08-018-03c1). [Evidencia](registro-commits/t08-d19-dependencias-privadas.md).
- [x] T08-D20: gate histórico en CRLF y caché vigente; carrera del control C de relleno TLS (T08-GATE-03a). Validadores actualizados; el «relleno» no era una carrera: Windows no pone en cero `SizeOfZeroFill`. [Evidencia](registro-commits/t08-d20-gate-historico.md).
- [ ] T08-D21: reagrupación de commits al cerrar la tanda (T08-GATE-05a).
- [x] T08-D22: callbacks `__vectorcall` (T08-017-03d1). [Evidencia](registro-commits/t08-d22-vectorcall.md).
- [x] T08-D23: arreglos de punteros calificados como campo de record (T08-016-03d1). [Evidencia](registro-commits/t08-d23-arreglos-punteros-campo.md).
- [x] T08-D24: el productor NSMeta escribía un minor mínimo según su catálogo (lógica del 2026-11-06 al 08). Ahora escribe siempre el contrato vigente (`kMetadataFormatMinor`); pasan `nsc_namespace_function_metadata_tests`, `nsc_stage50_9_metadata_host_compatibility_tests` e `item344`. [Evidencia](registro-commits/t08-d24-minor-vigente.md).
  - Fixture histórico 1.46 (`nsc.stage48_2.library-metadata`, 2 de 444 comprobaciones): no existe una muestra auténtica ni en el repositorio ni en `A:` (1.46 vivió menos de una hora, entre `d801af165` y `311431667`). La prueba no se modifica y no se reintroduce compatibilidad. Opciones para el usuario: dejarlo así (vigente) o construir el escritor de la época para archivar su muestra. [Investigación](registro-commits/t08-d24-fixture-historico-146.md).

## Cierre de la tanda

- [ ] T08-GATE-01: Completar la implementación y las comprobaciones pendientes de T08-002 a T08-020.
  - Todas las casillas de T08-002 a T08-020 están terminadas o bloqueadas con evidencia; las bloqueadas dependen de decisiones pendientes (sobre todo T08-010-05a) o son características nuevas registradas (T08-016-03d1, T08-017-03d1). [Evidencia](registro-commits/t08-validacion-integral-20261109.md).
  - 2026-11-09, tarde: T08-016-03d1 (D23) y T08-017-03d1 (D22) cerradas. Siguen bloqueadas las casillas de T08-010-05a (D13), además de D11, D12, D04b y D05b. [Evidencia](registro-commits/t08-validacion-integral-cierre-20261109.md).
  - 2026-11-09, noche: T08-D13a resuelve T08-010-05a y cierra T08-010-05/07/08/09, T08-015-03, 04, 05b, 05c, 06a, 08, 09, 10 y T08-020-03c. Queda pendiente funcional T08-015-05a (D13b); T08-015-11 se corrigió después; siguen esperando decisión D11, D12, D04b y D05b. [Evidencia](registro-commits/t08-d13-alias-volatile-modo-origen.md).
- [x] T08-GATE-02: Resolver o clasificar con evidencia el fallo de la prueba de topología de ondas paralelas. [Evidencia](registro-commits/t08-gate-ondas-paralelas.md).
- [x] T08-GATE-03: Ejecutar la validación integral agrupada de la tanda 8 y corregir sus fallos hasta que pase. [Evidencia](registro-commits/t08-validacion-integral-cierre-20261109.md).
  - Ejecutada el 2026-11-09: el gate complementario pasa; el histórico falla en cuatro validadores ajenos al compilador (LF, restricción de caché retirada por `03bf12e42` en dos pruebas y carrera del control C de relleno TLS). Las tres primeras pasan en ejecución derivada. T08-GATE-03a resuelta por T08-D20 (validadores actualizados). [Evidencia](registro-commits/t08-validacion-integral-20261109.md).
  - Reejecutada tras las decisiones T08-D: el gate complementario incluye las suites T08-D. La primera vez, el histórico falló en `typedef-funcion-record\validar-enum.py --mismatch`, una expectativa que T08-D10 dejó superada y que se actualizó. La segunda ejecución da código 0 en los dos gates. [Evidencia](registro-commits/t08-validacion-integral-cierre-20261109.md).
- [x] T08-GATE-04: Actualizar estados y evidencias contra la revisión validada, sin cerrar por muestras parciales. [Evidencia](registro-commits/t08-validacion-integral-20261109.md).
- [ ] T08-GATE-05: Revisar la agrupación de commits relacionados y preservar un respaldo del historial, conforme a lo acordado.
  - Respaldo creado sin reescribir el historial (bundle de la rama en `A:\nsc-interop-build\claude\respaldo\`). **Decisión pendiente T08-GATE-05a:** el criterio de reagrupación acordado no está en el repositorio; no se reescribió ningún commit. [Evidencia](registro-commits/t08-validacion-integral-20261109.md).
