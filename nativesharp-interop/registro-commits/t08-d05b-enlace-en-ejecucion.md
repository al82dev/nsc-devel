# T08-D05b: enlace en ejecución de un alias TLS cuyas dependencias homónimas deben aislarse

Fecha: 2026-11-09. Decisión del usuario (punto 3): implementar el enlace en ejecución únicamente cuando sea necesario
para aislar las dependencias de DLL homónimas, sin modificar las DLL. Es la solución A con alcance estricto de la
[preparación](t08-d04b-d05b-soluciones-preparadas.md).

## Problema

Dos `api.dll` homónimas y distintas se distribuyen con alias `__nsc_tls_<hash>.dll` (T08-D01/D02). Cuando cada una
depende de una `dep.dll` homónima y distinta, T08-D05a da a cada alias un ensamblado privado y un manifiesto de
contexto de activación, que el cargador de alias activa antes de `LoadLibrary`.

Una función que existe en una sola de las `api.dll` se importaba por la tabla de importación desde su alias. El
cargador de Windows resuelve esa tabla, y con ella la `dep.dll` del alias, antes de que el código pueda activar un
contexto. El empaquetado lo rechazaba: «… is imported directly, so its dependencies cannot be isolated».

## Cambio

Las funciones C de una DLL que necesita aislamiento se enlazan en ejecución, por el cargador de alias, igual que las
funciones que existen en las dos DLL homónimas. Las demás funciones no cambian.

**Qué DLL necesitan aislamiento** (`RuntimeClosurePackaging.cpp`, `collectCppRuntimesRequiringIsolation`, nuevo).
Antes de construir el HIR, el driver calcula el cierre de dependencias de cada `Runtime` C candidato. Usa la misma
búsqueda junto a la DLL que el empaquetado. Agrupa por nombre las dependencias de contenido distinto (sha256). Una
DLL necesita aislamiento si su cierre contiene una dependencia cuyo nombre aparece con otro contenido en el cierre de
otra DLL homónima. El nombre de la propia raíz no cuenta. El resultado son identidades `archivo:sha256`.

**Paso al HIR.**

- `ProjectCompilation::setCppRuntimeIsolationResolver` recibe esa función; el driver la instala en
  `configureProjectExecution` (`DriverExecution.cpp`).
- `compileProjectOopHir` la llama y pasa el conjunto a `ProjectOopHirModel::setCppIsolatedDllIdentities`.
- `HirBuildCoordinator.cpp` (`buildNamespaceFunction`): una función C usa el enlace en ejecución
  (`__nsc_tls_import_*`) si su DLL tiene alias TLS y, además, el nombre existe en dos DLL homónimas distintas
  (como antes) o la DLL está en ese conjunto (`cppDllRequiresIsolation`).

El cargador de alias (`__nsc_cpp_dll_tls_export`) ya activaba el contexto de `<alias>.nsc-isolation.manifest`; no
cambia. Como la función ya no se importa por la tabla, el rechazo del empaquetado deja de aplicarse. Se conserva
para cualquier alias que siga en la tabla de importación por otra vía.

**Alcance estricto.** Sin conflicto de dependencias, el producto es el mismo que antes. Esto incluye los productos
de T08-D05a sin dependencias homónimas: conservan la importación directa.

## Efectos

- **Carga diferida.** La DLL alias se carga en la primera llamada, no al iniciar el proceso. Su `DllMain` y la
  inicialización de su TLS ocurren entonces, en el hilo que llama primero.
- **Fallo en la primera llamada.** Si falta la DLL o su manifiesto, la primera llamada termina en la trampa del
  enlace (`0x87`), no al cargar el ejecutable. Nunca se usa otra `dep.dll`.
- **Coste.** Cada llamada pasa por un puntero resuelto una sola vez.
- **Tabla de importación.** Ya no nombra el alias.
- **Caché.** El conjunto se calcula sobre el contenido de las dependencias junto a cada `Runtime`, que ya forma
  parte de la identidad del cierre de dependencias C en la caché de artefactos (T08-D05a).

## Prueba

`tls-dll-distintas/validar-transitivas-homonimas.py` (de T08-D05). El caso 7, que esperaba el rechazo, pasa a
comprobar la capacidad:

- **`check_exclusive_alias`:** la función exclusiva de cada `api.dll` da su valor (101 y 102). Se comprueba en
  ejecutable O0–O3 y en un producto DLL.
- **Tabla de importación** (llvm-readobj): no contiene ningún `__nsc_tls_*`.
- **Sin el manifiesto de aislamiento:** la ejecución termina con `0x87`, la trampa del enlace.
- **`check_exclusive_without_isolation`:** con DLL homónimas cuyas dependencias no entran en conflicto, la función
  exclusiva conserva la importación directa (alcance estricto).

El resto de la suite (casos 1 a 6 y 8, ejecutable y producto DLL, caché) no cambia y pasa.

## Regresión

`claude\scripts\regresion-d04b-d05b.cmd` ejecuta:

- el conjunto de D05: D04, D01, D02 y `validar-transitivas-homonimas.py`;
- con D04b, `validar-copia-adaptada.py`.

Resultado: código 0 en todo el conjunto, incluida la prueba de D04b.
