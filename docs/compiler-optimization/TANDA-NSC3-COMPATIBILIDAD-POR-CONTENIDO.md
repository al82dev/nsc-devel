# Tanda NSC 3: compatibilidad por contenido, no por etiqueta de versión

## Problema

NSC rechazaba metadata válida antes de intentar utilizarla cuando el encabezado
NSMETA indicaba una revisión posterior a la conocida. También rechazaba el
mismo contrato cuando `producerVersion` no pertenecía a una ventana fija de
versiones del compilador.

Esto convertía una etiqueta informativa en una frontera artificial. Un payload
que el lector podía decodificar y verificar era rechazado aunque no utilizara
ninguna capacidad desconocida.

La política general del compilador repetía el mismo problema: comparaba números
SemVer y verificaba un archivo de política byte por byte contra los valores de
la construcción actual.

## Cambios

- `src/Interop/LibraryMetadata.cpp`
  - dejó de rechazar NSMETA por el número `major` o por un `minor` posterior;
  - dejó de limitar `producerVersion` a dos releases concretos;
  - conserva checksum, longitud, target Windows x64, punteros, ABI física y
    coherencia del modelo;
  - el escritor conserva la versión solicitada como información y valida si el
    contenido puede representarse.
- `src/Driver/CompatibilityPolicy.cpp`
  - considera informativas las etiquetas de versión del compilador;
  - acepta campos futuros de la política mientras su estructura sea legible;
  - declara explícitamente compatibilidad basada en capacidades y contenido.
- `tests/unit/LibraryMetadataVersionToleranceTests.cpp`
  - prueba de forma aislada una etiqueta major distinta, un minor futuro y un
    productor antiguo sobre contenido compatible.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
  - reemplaza las expectativas antiguas de rechazo por compatibilidad real.
- `tests/unit/Stage66E9E10ReleaseContractTests.cpp`
  - alinea el contrato de release con la política basada en capacidades.
- `CMakeLists.txt`
  - registra la prueba mínima independiente.

NSLink no interpreta NSMETA y no contiene una ventana SemVer equivalente. No se
modificó `A:\nslink-devel` porque introducir un cambio sin conducta que corregir
sería artificial.

## Efecto

Una versión distinta ya no causa por sí sola un error. NSC intenta decodificar
y utilizar el contenido. Si el archivo está corrupto, pertenece a otro target,
usa una ABI física incompatible o contiene una estructura que el lector no
entiende, el error proviene de esa incompatibilidad concreta.

No se incrementó la versión de NSMETA.

## Verificación dirigida

- construcción incremental del target `nsc`: correcta;
- prueba `nsc_library_metadata_version_tolerance_tests`: `PASS: 9 checks`;
- emisión O0 completa de `NativeSharp.Math.Geometry`: correcta;
- artefactos generados: `.lib`, `.h` y `.nsmeta`;
- tiempo medido de la emisión dirigida: 2451 ms;
- código de salida: 0.

No se ejecutó ningún baseline ni una batería completa de pruebas.
