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

NSMETA 1.23 introdujo un byte fisico de revision de fuente. El productor historico lo conservaba
en el payload, pero lo reemplazaba deliberadamente por cero al construir la proyeccion ABI. Tras
retirar la edicion del modelo, la proyeccion uso por error el valor neutral del escritor actual. El
payload seguia integro, pero la autenticacion comparaba proyecciones distintas.

La fixture que materializa cada minor historico tambien conservaba contratos agregados en minors
posteriores. El escritor los rechazaba correctamente; la fixture no estaba construyendo el
subconjunto comun que afirmaba probar.

## Cambio

1. `decodePayload` consume el byte fisico sin publicarlo en el modelo.
2. La proyeccion ABI reproduce el cero historico que excluye ese byte del contrato publico.
3. Los escritores nuevos siguen emitiendo el valor neutral y no aceptan una edicion del modelo.
4. La prueba limpia contratos de lifetime, escape, drop glue, typed throws y FFI/generics cuando
   el minor seleccionado aun no los representaba.

## Mejora

La compatibilidad deja de depender de una edicion semantica y pasa a depender exclusivamente de
la capacidad real del formato. Los archivos historicos autenticos siguen siendo legibles, mientras
que un payload alterado o una huella incorrecta continuan siendo rechazados.

## Validacion prevista

- Compilar solamente `nsc_stage48_2_library_metadata_tests`.
- Ejecutar solamente `nsc.stage48_2.library-metadata` con NSC y NSLink oficiales.
- No ejecutar baseline.
