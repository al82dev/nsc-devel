# Tanda correctiva G00 — NS_LIB_ROOT como fallback

Fecha: 2026-10-01
Estado: terminada, pendiente de baseline completo

## Fallo observado

El runner publica `NS_LIB_ROOT=A:\native-sharp-3.0-baseline\lib` para que proyectos bootstrap
puedan encontrar paquetes instalados. El driver convertía esa variable de entorno en un
`LibraryReferenceRoot` explícito. Como consecuencia, cualquier referencia relativa local —por
ejemplo `../Products/MatrixProducer.lib`— dejaba de resolverse desde su `.nsproj` y se reescribía
como `A:\native-sharp-3.0-baseline\lib\../Products/MatrixProducer.lib`.

El mismo defecto causó cascadas en la matriz DLL, bibliotecas nativas, hosts C/C++, ejemplos
empaquetados y consumidores source-free.

## Regla implementada

- `LibraryReferenceRoot` declarado en proyecto o línea de comandos mantiene su semántica
  explícita: selecciona la raíz de configuración.
- `NS_LIB_ROOT` se almacena como `libraryReferenceFallbackRoot`.
- Una ruta absoluta conserva siempre su significado.
- Una referencia relativa existente junto al proyecto se conserva intacta.
- Sólo cuando el artefacto relativo no existe se consulta el fallback de paquetes.
- El cierre binario puede consultar tanto la raíz explícita como el fallback, sin duplicar raíces.

## Archivos modificados

- `include/nativesharp/Compilation/ProjectLoader.h`: separa raíz explícita y fallback.
- `src/Compilation/ProjectLoader.cpp`: aplica precedencia local, explícita y fallback.
- `src/Driver/Driver.cpp`: convierte `NS_LIB_ROOT` en fallback y lo incorpora al cierre binario.
- `tests/unit/ProjectLoaderTests.cpp`: comprueba paquete ausente, artefacto local y raíz explícita.

## Mejora obtenida

El staging puede proveer paquetes globales sin cambiar el significado de rutas pertenecientes a
fixtures, proyectos generados o repositorios externos. Se mantiene la selección explícita de
configuración O0/O2 cuando el usuario sí proporciona `LibraryReferenceRoot`.

## Validación específica

- Compilación exclusiva de `nsc_project_loader_tests` y de la matriz DLL: correcta.
- `nsc_project_loader_tests`: 85/85 comprobaciones correctas.
- La matriz DLL dejó de reportar `referenced library does not exist` para las 16 combinaciones y
  avanzó hasta un defecto independiente de verificación de importación DLL, reservado para otra
  tanda.
- No se ejecutó CTest ni baseline.
