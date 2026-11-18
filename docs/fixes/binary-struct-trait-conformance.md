# Conformidad de traits de structs en bibliotecas NativeSharp

Una biblioteca que declara un struct y su implementación de un trait genérico
publica la interfaz especializada en su metadata. Al reconstruir el struct para
un consumidor, la superficie anterior omitía sus interfaces salvo que el struct
fuera un resultado opaco. Una llamada genérica que exigía el trait rechazaba el
struct correcto como no conforme.

`PublicSurfaceBuilder::emitNominalBases` conserva ahora las interfaces registradas
para structs ordinarios. No se modifica el formato de metadata ni la llamada C/C++.
La regresión `binary_struct_trait_conformance.py` produce bibliotecas estáticas y
DLL y cruza productor y consumidor O0/O2. El caso conforme invoca `Combine` y
devuelve 42; el struct sin el trait se rechaza por esa restricción.

La reproducción anterior al cambio se conservó en
`A:/nsc-fixes-evidencia/generic-owner-producer-trait-control.log` en su primera
ejecución; ese archivo se reutilizó después y ahora conserva el resultado
posterior. La evidencia original del rechazo del productor con dueño genérico
permanece en `A:/nsc-fixes-evidencia/generic-owner-binary-trait-producer-failure.log`.
El control independiente posterior está en
`A:/nsc-fixes-evidencia/binary-struct-trait-conformance.log`. La compilación fue
incremental con otros cambios locales presentes; no acredita una revisión limpia.
