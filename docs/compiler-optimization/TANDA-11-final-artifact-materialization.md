# Tanda 11: materialización de NSMETA y artefactos finales

## Objetivo

Eliminar lecturas y verificaciones repetidas entre componentes que ya comparten un contrato de artefacto validado.

## Cambios realizados

- `LibraryMetadataCodec::write` puede devolver el modelo normalizado exacto que acaba de codificar.
- La verificación del productor reutiliza ese modelo en memoria en vez de volver a abrir, leer, asignar y decodificar el `.nsmeta` completo.
- La orden dedicada `--emit-project-library-metadata` reutiliza el mismo modelo para su resumen y elimina un segundo round-trip adicional.
- Los builders de EXE, LIB y DLL aceptan una marca explícita de objetos de programa ya verificados.
- El driver activa esa marca únicamente para los objetos entregados por `ProjectOopObjectGenerator`, que verifica cada objeto nuevo o restaurado antes de crear el bundle.
- Los consumidores directos de los builders mantienen la verificación defensiva por defecto.
- La verificación del archivo COFF, PE/DLL o import library final se conserva sin cambios.

## Archivos modificados

- `include/nativesharp/Interop/LibraryMetadata.h`
- `src/Interop/LibraryMetadata.cpp`
- `include/nativesharp/Linker/WindowsExecutableBuilder.h`
- `src/Linker/WindowsExecutableBuilder.cpp`
- `include/nativesharp/Linker/WindowsLibraryBuilder.h`
- `src/Linker/WindowsLibraryBuilder.cpp`
- `src/Driver/Driver.cpp`

## Archivos nuevos

- Este informe. No se crearon archivos de código nuevos porque se ampliaron contratos existentes entre productor, codec y builders.

## Mejora esperada

Las bibliotecas grandes evitan hasta dos lecturas y decodificaciones completas de NSMETA durante su propia producción. Todos los productos particionados evitan volver a leer cada `.obj` inmediatamente antes de enlazar o archivar, mientras conservan la validación del resultado final.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Emisión O0 de Stage 46: biblioteca de 7 miembros, header y NSMETA generados correctamente.
- Lectura independiente con `--verify-library-metadata`: NSMETA 1.20 válido, ABI y contenido correctos.
- Emisión y ejecución O0 del ejecutable Stage 49: correctas, código de salida 0.
- No se ejecutaron rutas O1/O2/O3 ni baseline completo.
