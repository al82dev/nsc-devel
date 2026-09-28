# Tanda 16: almacenamiento de namespace y cierre

## Objetivo

Completar el estado declarado directamente en un namespace: resolver sus referencias entre archivos, conservar una direccion unica, bajarlo hasta LLVM y publicarlo de forma consumible mediante LibraryReference.

## Cambios

- El modelo semantico registra referencias calificadas y no calificadas a variables de namespace, respetando visibilidad, imports y sombreado por locales.
- Los cuerpos distinguen el estado mutable de las constantes y conservan asignabilidad, direccionabilidad e identidad.
- HIR y MIR tienen nodos explicitos para valor y direccion de estado de namespace.
- MIR crea un unico registro de almacenamiento por variable, conserva inicializadores constantes y marca el almacenamiento procedente de una biblioteca precompilada.
- LLVM materializa inicializadores tanto constantes como mutables y reutiliza el mecanismo de globals y accessors para garantizar una direccion estable.
- NSMETA 1.22 publica estado publico, tipo, valor constante, simbolo de almacenamiento y accessor.
- El driver reconstruye el estado publico de una LibraryReference; las constantes se materializan localmente y el estado mutable llama al accessor propiedad del productor.
- Las DLL exportan los accessors del estado mutable publico y la validacion del archivo COFF exige que esos simbolos existan.
- El merger de NSMETA incluye, ordena y valida registros de estado de namespace.

## Archivos modificados

- CMakeLists.txt
- include/nativesharp/Frontend/ProjectMethodBodyModel.h
- include/nativesharp/Frontend/ProjectNameLookup.h
- include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h
- include/nativesharp/Frontend/ProjectOopHirModel.h
- include/nativesharp/Frontend/ProjectOopMirModel.h
- include/nativesharp/Interop/LibraryMetadata.h
- src/Driver/Driver.cpp
- src/Frontend/ProjectCompilation.cpp
- src/Frontend/ProjectMethodBodyModel.cpp
- src/Frontend/ProjectNameLookup.cpp
- src/Frontend/ProjectNamespaceFunctionModel.cpp
- src/Frontend/ProjectOopHirModel.cpp
- src/Frontend/ProjectOopHirModelDumper.cpp
- src/Frontend/ProjectOopMirModel.cpp
- src/Frontend/ProjectOopMirModelDumper.cpp
- src/Interop/LibraryMetadata.cpp
- src/Interop/LibraryMetadataMerger.cpp
- src/LLVM/ProjectOopLlvmLowerer.cpp
- tests/unit/NamespaceFunctionMetadataTests.cpp
- tests/unit/NamespaceStateModelTests.cpp

## Archivos nuevos

- docs/2.0/development/namespace-functions/TANDA-16-ESTADO-Y-CIERRE.md: registra el alcance, las decisiones y la verificacion de la tanda final.

## Mejora obtenida

Una variable declarada en un namespace ya no es solo informacion semantica. Tiene almacenamiento compartido real entre archivos, una direccion estable, lectura y escritura completas, inicializacion constante y un contrato fisico verificable al cruzar bibliotecas. Con esto se cierra el recorrido de funciones libres y estado de namespace desde fuente hasta consumidor precompilado.

## Compatibilidad

- Target unico: Windows x64.
- NSMETA sube de 1.21 a 1.22; los lectores actuales conservan compatibilidad de lectura con minors anteriores.
- No se incrementa la version de Native ABI ni Runtime ABI; cada estado mutable publico agrega a la biblioteca de usuario su accessor __nsc_get_*.
- El estado internal no se publica.
- El estado const se materializa desde su valor certificado; el mutable conserva una sola copia propiedad del productor.

## Verificacion

- Construccion incremental exclusiva de nsc: correcta.
- nsc_namespace_state_model_tests: PASS, 24 verificaciones.
- nsc_namespace_function_metadata_tests: PASS, 9 verificaciones.
- Los 13 ejecutables del feature se construyeron y ejecutaron individualmente: PASS, 139 verificaciones acumuladas.
- No se ejecuto ni se ejecutara baseline, CTest general, fixtures generales ni matriz de optimizaciones.
