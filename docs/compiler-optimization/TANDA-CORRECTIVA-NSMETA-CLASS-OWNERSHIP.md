# Tanda correctiva: propiedad de clases en NSMETA 1.36

## Problema

La verificación de lectura posterior de NSMETA 1.36 exigía una entrada de drop-glue a cualquier
tipo que tuviera directamente un campo `owned`. Esa regla también alcanzaba clases como
`NativeSharp.Core.ArenaAllocator`, aunque el productor sólo define drop-glue de valor para structs.

Las clases no son valores inline: su destrucción se publica y ejecuta mediante el descriptor y las
entradas de destrucción del objeto. Exigirles además el thunk `__nsc_drop_value_*` contradecía al
productor, al MIR, al backend LLVM y a la validación previa del nombre, que ya restringía ese thunk
a `LibraryMetadataTypeKind::Struct`.

## Cambios

- `src/Interop/LibraryMetadata.cpp` limita la obligación de drop-glue de valor a structs con
  almacenamiento `owned` directo.
- `tests/unit/NativeSharp3T06DropGlueTests.cpp` añade una clase pública que posee un recurso y
  comprueba que su NSMETA es válido sin publicar drop-glue de valor.
- Se conserva intacta la regla estricta para structs propietarios y la compatibilidad NSMETA 1.35.

## Archivos

- Modificado: `src/Interop/LibraryMetadata.cpp`.
- Modificado: `tests/unit/NativeSharp3T06DropGlueTests.cpp`.
- Nuevo: este documento de trazabilidad.

## Mejora

Las bibliotecas pueden publicar clases con campos `owned` sin confundir destrucción de objetos con
drop-glue de valores inline. Los structs propietarios continúan obligados a publicar el thunk del
productor para que los consumidores destruyan el valor exactamente una vez.

## Validación permitida

Se construye y ejecuta únicamente `nsc.ns3.t06.drop-glue`, y se recompila directamente
`NativeSharp.Core` con el `nsc.exe` construido en A:. No se ejecuta baseline, CTest general ni
matriz de optimizaciones.

Resultado:

- `nsc.ns3.t06.drop-glue`: 1/1 aprobado con NSLink.
- Objetivo `nsc`: compilado y relazado correctamente; contrato de runtime estático aprobado.
- `NativeSharp.Core`: emisión O0 aprobada, incluidos `.lib`, `.h` y `.nsmeta`.
- El error de `ArenaAllocator` no se reprodujo con el binario corregido.
- `NSC_BUILD_TESTS` quedó restaurado a `OFF` y se eliminaron los artefactos temporales dirigidos.
- Baseline, CTest general y matriz de optimizaciones: no ejecutados.
