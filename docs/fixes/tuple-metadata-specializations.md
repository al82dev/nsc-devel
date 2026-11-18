# Especializaciones usadas en firmas de tupla

Un productor NativeSharp con `Read(SharedView<Payload>)` publicaba la
especialización de la vista, pero los productores equivalentes con una tupla
simple o anidada no la publicaban. En el compilador aislado previo al cambio,
las tres metadata se verificaron y los catálogos declararon respectivamente
1, 0 y 0 especializaciones. Comandos y salida en
`A:/nsc-fixes-evidencia/tuple-specialization-catalog-probe.log`.

`rememberReferencedMetadataType` ahora visita iterativamente los elementos
de una tupla con el decodificador de tipos existente. Las tres variantes
publicaron 1 especialización y pasaron la verificación de metadata. La
regresión dirigida `tests/integration/tuple_metadata_specialization.py`
comprueba estos tres casos y la identidad completa de la especialización en
la metadata. La regresión está registrada en CTest como
`nsc.generics.tuple-metadata-specialization` y usa un proyecto genérico de
prueba independiente; pasó con salida 0 en
`A:/nsc-fixes-evidencia/tuple-metadata-registered-regression.log`. La misma
prueba también pasó con el Core binario aislado en
`A:/nsc-fixes-evidencia/tuple-metadata-sdk-regression-current.log`. La
comparación de artefactos anteriores confirma la identidad completa presente
en el caso directo y ausente en los dos casos de tupla
(`A:/nsc-fixes-evidencia/tuple-metadata-identity-baseline.log`). También pasó el control existente
`nsc.sdk.shared-static-binary-reference`, con salida en
`A:/nsc-fixes-evidencia/tuple-metadata-specialization-controls.log`.

El archivo `src/Interop/LibraryMetadataConstruction.cpp` construye metadata
NativeSharp y es compartido con la rama de interoperabilidad: requiere revisión
al integrar. Esta corrección no modifica el formato de metadata ni demuestra
la reconstrucción completa de firmas con tuplas o la seguridad de préstamos
en un consumidor binario. El compilador y el Core usados incluyen cambios
locales aún sin confirmar, por lo que las pruebas no certifican un árbol
limpio. Las pruebas fueron dirigidas; no se ejecutó una batería completa.
