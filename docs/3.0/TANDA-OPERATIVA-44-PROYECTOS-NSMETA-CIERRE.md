# Tanda operativa 44 — proyectos, NSMETA y cierre binario

**Fecha:** 1 de octubre de 2026

**Rama:** `development`

**Plataforma:** Windows x64

**Configuración validada:** Debug/O0

**Baseline:** no ejecutado; Alberto lo ejecutará en T53

## Objetivo

Cerrar §50, §98, §108, ZLIB-002 y la parte de contrato del compilador de §18/LIBPNG-004:

- hacer visible la edición seleccionada por `<Language>3</Language>` en `nsc build`;
- importar una identidad precompilada canónica una sola vez aunque aparezca por varios caminos;
- resolver automáticamente el cierre de archivos estáticos desde una sola `LibraryReference`;
- transportar imports de plataforma privados y rechazar sustituciones ABI antes del linker;
- publicar una superficie consumible que no filtre tipos internos de sesión.

No se implementaron aquí `Core.IO`, streams o la sesión incremental de Zlib: esos productos siguen
en T50. T44 entrega el mecanismo de proyecto y metadata que permite empaquetarlos correctamente.

## NSMETA 1.25

El formato incorpora dos secciones de implementación:

1. `binaryDependencies`: módulo, nombres hoja de `.lib`/`.nsmeta`/DLL opcional y fingerprint ABI
   de cada dependencia estática directa.
2. `platformImports`: pares DLL/símbolo realmente usados por el archivo, incluidos los imports
   privados que no forman parte de la API pública.

Los nombres son hojas deliberadamente: un paquete puede reubicarse como unidad sin conservar la
ruta de compilación del productor. El consumidor busca primero junto al metadata que refiere y
después bajo `LibraryReferenceRoot`; no escanea directorios ni ejecuta restore.

Estas secciones no alteran el fingerprint de la API pública. El fingerprint sigue proyectándose
con las reglas 1.24 y el consumidor comprueba por separado el fingerprint certificado de cada
arista binaria. Un cambio privado puede modificar el cierre físico sin fingir una ruptura de API,
pero sustituir una dependencia pública incompatible queda rechazado antes del enlace.

## Expansión del cierre estático

`configurePrecompiledModules` carga las referencias explícitas y recorre después las aristas 1.25.
Cada módulo se indexa por su identidad canónica:

- la misma identidad/fingerprint se materializa una vez;
- dos artefactos explícitos con el mismo módulo y distinta procedencia se rechazan;
- dos versiones incompatibles dentro del cierre se rechazan;
- el nombre de módulo leído debe coincidir con el declarado por la arista;
- el fingerprint leído debe coincidir exactamente con el certificado por el productor;
- todas las referencias descubiertas se agregan al grafo en memoria para validación, codegen y
  enlace, sin modificar `.nsproj` ni crear una dependencia de fuentes.

Con ello un consumidor enumera sólo el paquete de nivel superior. El cierre transitivo completo
entra tanto al frontend precompilado como a la lista de archivos del linker.

## Imports privados de plataforma

Al producir una biblioteca estática se recorren los callees MIR y se publican sólo los imports C
utilizados. Las declaraciones internas no se convierten en tipos públicos. Al consumir el cierre,
los pares DLL/símbolo de todos los módulos se fusionan sin duplicados, con comparación de DLL
insensible a mayúsculas, y se entregan al generador único de import libraries.

La fixture usa `GetCurrentProcessId` desde una clase interna. El consumidor no ve esa clase, pero
su ejecutable conserva el import y funciona. Esto cierra ZLIB-002 sin exigir que una API privada se
duplique en cada consumidor.

## Edición de lenguaje en `nsc build`

El loader ya aceptaba las ediciones 2 y 3 y la identidad de caché MIR ya incluía la edición. El
hueco era observable: `nsc build` no decía qué edición había seleccionado. La salida humana ahora
incluye `Build succeeded (Language 3)` y el resultado JSON publica `"language":"3"`.

Un valor desconocido sigue rechazándose por el loader con la lista soportada. La fixture T44 entra
por `nsc build --no-restore -O0`, no por un acceso interno, y comprueba la salida seleccionada.

## Identidad genérica y superficie privada

El índice `loadedByModule` impide hidratar dos veces el mismo proveedor canónico al encontrarlo por
aristas diferentes. El merger mantiene una sola especialización por clave canónica y ahora también
compara `isParameterPack`, que antes no participaba en la igualdad estructural de parámetros.

El productor continúa publicando únicamente tipos públicos. La fixture implementa `Session` y el
wrapper nativo como tipos internos; el consumidor sólo materializa `Api` y ejecuta correctamente.
Este es el contrato requerido por Core.IO/Zlib. La implementación y distribución concreta de esos
paquetes permanece asignada a T50.

## Archivos modificados

- `include/nativesharp/Interop/LibraryMetadata.h` y
  `src/Interop/LibraryMetadata.cpp`: NSMETA 1.25, producción, serialización, lectura, validación y
  proyección ABI de cierre binario/imports privados.
- `src/Interop/LibraryMetadataMerger.cpp`: unión determinista de las nuevas secciones y comparación
  completa de parameter packs.
- `include/nativesharp/Compilation/ProjectGraph.h` y
  `src/Compilation/ProjectGraph.cpp`: incorporación in-memory de referencias descubiertas.
- `src/Driver/Driver.cpp`: expansión transitiva certificada, unicidad de módulos e imports
  heredados en ejecutable, biblioteca estática y DLL.
- `src/Driver/ProjectCommands.cpp`: edición de lenguaje en salida humana y JSON de `nsc build`.
- `tests/unit/LibraryMetadataMergerTests.cpp`: cierre/imports canónicos en fragmentos.
- `CMakeLists.txt`: registro de la prueba dirigida T44.

## Archivos nuevos y razón

- `tests/integration/ns3_t44_metadata_closure/{Dependency,Package,Consumer}.ns`: cadena real de
  productor, paquete con sesión/import privado y consumidor.
- `cmake/VerifyNativeSharpT44MetadataClosure.cmake`: construye sólo esa cadena O0, ejecuta el
  consumidor y prueba una sustitución ABI negativa.
- `docs/3.0/TANDA-OPERATIVA-44-PROYECTOS-NSMETA-CIERRE.md`: este expediente de diseño, archivos,
  mejora, límites y evidencia.

## Validación dirigida

- Construcción de `nsc` Debug/O0: correcta.
- Verificador T44: dependency + package + `nsc build` consumer, ejecución exit `0`, Language 3
  visible, cierre transitivo sin enumeración manual e import privado preservado.
- Negativa T44: dependencia válida pero ABI incompatible; rechazo causal
  `binary closure ABI fingerprint mismatch` antes del linker.
- `nsc_library_metadata_merger_tests`: 17/17 checks.
- `nsc_stage48_2_library_metadata_tests` se ejecutó como diagnóstico adicional y conserva fallos
  históricos ajenos a T44: exige Object ABI 2/fingerprint antiguo y después acumula fallos de su
  arnés de productos. No se alteró ese test ni se contabilizó como aceptación de la tanda.

No se ejecutó baseline, CTest completo, O1/O2/O3 ni trabajo remoto; no se instaló ni modificó
`C:\native-sharp`.

## Mejora obtenida

Una `LibraryReference` de nivel superior ya es suficiente para consumir un paquete estático
relocatable y certificado. El compilador incorpora sus dependencias exactamente una vez, conserva
imports privados, impide mezclas ABI por nombre y mantiene privada la implementación. `nsc build`
además deja explícita la edición de lenguaje que realmente compiló. Esto elimina enumeración manual,
errores tardíos del linker y fugas de tipos internos, y deja listo el mecanismo para los paquetes
Core.IO, Zlib y Libpng de T50.
