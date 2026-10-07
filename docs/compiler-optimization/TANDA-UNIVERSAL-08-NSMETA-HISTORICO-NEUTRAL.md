# Tanda universal 08: autenticacion NSMETA historica neutral

## Scope lock

- Autenticar archivos NSMETA historicos con el byte fisico que realmente fue firmado.
- Mantener ese byte fuera del modelo semantico y fuera de toda decision de compilacion.
- Normalizar la huella expuesta por el lector al contrato universal actual.
- Proyectar la fixture historica a los campos representables por cada minor.
- Conservar formato binario, hashes de payload, validaciones downlevel y archivos golden.

## Archivos autorizados

- `src/Interop/LibraryMetadata.cpp`
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
- Este documento.

## Fuera de alcance

- Reintroducir ediciones de lenguaje en el modelo, proyectos o diagnosticos.
- Relajar errores cuando un formato antiguo no puede representar un contrato moderno.
- Reescribir archivos golden archivados.
- Modificar NSLink, instalar en `C:`, ejecutar baseline u optimizaciones distintas de O0.

## Causa confirmada

NSMETA 1.23 introdujo un byte fisico de revision de fuente. Las huellas historicas se calcularon
con el valor escrito por cada productor. Despues de retirar la edicion del modelo, el decoder leia
y descartaba ese byte antes de recomputar la huella con el valor neutral del escritor actual. El
payload seguia integro, pero la autenticacion comparaba proyecciones distintas.

La fixture que materializa cada minor historico tambien conservaba contratos agregados en minors
posteriores. El escritor los rechazaba correctamente; la fixture no estaba construyendo el
subconjunto comun que afirmaba probar.

## Cambio

1. `decodePayload` devuelve el byte fisico en una variable local del lector.
2. La autenticacion ABI reproduce la proyeccion historica exacta con ese byte.
3. Tras autenticar, el modelo expuesto recibe la huella neutral del contrato actual.
4. Los escritores nuevos siguen emitiendo el valor neutral y no aceptan una edicion del modelo.
5. La prueba limpia contratos de lifetime, escape, drop glue, typed throws y FFI/generics cuando
   el minor seleccionado aun no los representaba.

## Mejora

La compatibilidad deja de depender de una edicion semantica y pasa a depender exclusivamente de
la capacidad real del formato. Los archivos historicos autenticos siguen siendo legibles, mientras
que un payload alterado o una huella incorrecta continuan siendo rechazados.

## Validacion prevista

- Compilar solamente `nsc_stage48_2_library_metadata_tests`.
- Ejecutar solamente `nsc.stage48_2.library-metadata` con NSC y NSLink oficiales.
- No ejecutar baseline.
