# Tanda 07: referencias y dependencias C/C++

Estado: análisis e implementación agrupados en curso; no se da la tanda por cerrada.

- `ProjectLoader.cpp::parseCppLibraryReferenceItem` admite `Header`, `Language`
  (`C` o `C++`), `Standard` y opcionalmente `Library` y `Runtime`. Los hijos
  `IncludeDirectory` (`User`/`System`), `Define` y `Undefine` conservan orden.
  El cargador canoniza y verifica header, directorios y artefactos; `Runtime`
  requiere una biblioteca y un archivo DLL existente.
- `Project.h` conserva una lista de `CppLibraryReference` por proyecto. Cada
  referencia contiene header, biblioteca, DLL, lenguaje, estándar, directorios y
  macros. `CppProjectSessions.cpp` abre una sesión por referencia, en orden y
  agrupada por proyecto del grafo, con triple y recursos del entorno compartido.
- `CompilationInputManifest` registra como entradas el header principal, la
  biblioteca y la DLL; la identidad de bundle rechaza proyectos con referencias
  C/C++ porque aún no cubre las dependencias transitivas y búsquedas negativas.
  No debe retirarse esa barrera basándose solo en hashes de archivos observados.
- Las entradas de inventario de funciones, globals y records conservan proyecto,
  ordinal, referencia y triple junto con la declaración; el inventario de
  namespaces conserva proyecto, ordinal, referencia y módulo. Las selecciones de
  funciones usan `visibleProjects`, y `ProjectCompilationSnapshots.cpp` verifica
  que los orígenes seleccionados correspondan al proyecto y referencia activos.
  Esto describe las categorías implementadas, no toda declaración posible de C++.
- La validación previa de las referencias de un proyecto comprueba idioma,
  estándar reconocido por la tabla de Clang y compatible con el idioma, header
  no vacío y forma de includes/macros antes de abrir sus sesiones. La existencia
  real de los archivos y otras opciones siguen a cargo de la apertura de Clang;
  no se afirma un diagnóstico anticipado de todas las opciones posibles.
  Pendiente de prueba agrupada.
- La sesión puede enumerar archivos observados y calcular el SHA-256 del buffer
  cargado por Clang. Esto no incluye necesariamente consultas fallidas, buffers
  virtuales ni modificaciones concurrentes, y por sí solo no valida la caché.
  Se agregó una prueba para reabrir una sesión tras cambiar un header transitivo
  sin cambiar su tamaño; está pendiente de ejecución.
- La declaración de DLL/importación y el empaquetado de bibliotecas estáticas
  tienen evidencia parcial previa. Siguen abiertas la importación COFF larga y
  otras topologías; el diseño de entrada no equivale a soporte integral de enlace.
- Los errores de apertura del header se convierten a diagnósticos de NSC desde la
  tanda 05. Su presentación por el servicio de lenguaje sigue pendiente.

Siguiente paso: comprobar visibilidad y procedencia en el grafo, completar las
pruebas de referencias independientes y cambio de header y ejecutar la validación
agrupada de la tanda. No habilitar la caché sin identidad de dependencias completa.
