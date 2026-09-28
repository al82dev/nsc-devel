# Tanda 13: funciones de namespace en NSMETA

## Objetivo

Publicar funciones libres públicas y reconstruirlas en consumidores LibraryReference sin crear clases contenedoras ni alterar Native ABI 1.0.

## Cambios

- NSMETA avanza de 1.20 a 1.21 mediante una sección final aditiva.
- Cada función pública conserva namespace, nombre, firma canónica, retorno, ownership, parámetros, valores predeterminados, ref-return, variadic, convención externa y símbolo físico.
- Las funciones internal no se publican.
- El codec escribe y lee la sección sólo desde minor 21; archivos 1.20 siguen siendo válidos.
- El verificador rechaza identidades incompletas, símbolos físicos ausentes, duplicados y tipos fuera del cierre declarado.
- La combinación de fragmentos ordena y deduplica por namespace y firma.
- El generador de API pública reconstruye declaraciones libres dentro de su namespace real.
- Las unidades provenientes de LibraryReference marcan sus declaraciones como precompiladas, de modo que no exigen un cuerpo local.
- El mangling determinista del consumidor coincide con el símbolo físico publicado por el productor.

## Archivos modificados

- include/nativesharp/Interop/LibraryMetadata.h
- src/Interop/LibraryMetadata.cpp
- src/Interop/LibraryMetadataMerger.cpp
- src/Driver/Driver.cpp
- include/nativesharp/Frontend/ProjectNameLookup.h
- src/Frontend/ProjectNameLookup.cpp
- include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h
- src/Frontend/ProjectNamespaceFunctionModel.cpp
- CMakeLists.txt

## Archivo nuevo

- tests/unit/NamespaceFunctionMetadataTests.cpp: prueba aislada de publicación, visibilidad, firma, símbolo, round-trip y compatibilidad 1.20.

## Mejora obtenida

Una biblioteca puede describir funciones libres como API real. El consumidor recupera el mismo callable y el mismo símbolo físico sin simular un método estático ni depender del archivo fuente del productor.

## Compatibilidad

- NSMETA: cambio aditivo 1.21.
- Native ABI: permanece 1.0.
- Runtime ABI: permanece 1.0.
- Target: Windows x64.

## Verificación

- Construcción incremental exclusiva del objetivo nsc: correcta.
- Construcción exclusiva de nsc_namespace_function_metadata_tests: correcta.
- Ejecución exclusiva de nsc_namespace_function_metadata_tests: PASS, 9 verificaciones.
- No se ejecutó baseline, CTest general, fixtures generales ni matriz de optimizaciones.