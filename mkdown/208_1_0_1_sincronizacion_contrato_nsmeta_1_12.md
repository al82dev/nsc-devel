# Sincronización del contrato NSMETA 1.12 / NSMETA 1.12 contract synchronization

## Resumen / Summary

El formato vigente de metadatos de biblioteca es NSMETA 1.12, pero cuatro grupos de pruebas seguían fijando literalmente la versión 1.11. La implementación del compilador y las constantes públicas ya estaban en 1.12; la divergencia pertenecía a los contratos de prueba y no a la emisión del producto.

The current library metadata format is NSMETA 1.12, but four test groups still hard-coded version 1.11. The compiler implementation and public constants were already at 1.12; the drift was in the test contracts, not in product emission.

## Archivos modificados / Modified files

- `tests/unit/Stage62ToolchainContractTests.cpp`
  - Construye la versión esperada del contrato de toolchain a partir de `kMetadataFormatMajor` y `kMetadataFormatMinor`.
  - Builds the expected toolchain-contract version from `kMetadataFormatMajor` and `kMetadataFormatMinor`.
- `tests/unit/Stage66E9E10ReleaseContractTests.cpp`
  - Deriva de las mismas constantes los límites de lectura y escritura publicados por la política de compatibilidad.
  - Derives the compatibility policy read and write bounds from the same constants.
- `tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp`
  - Verifica la versión actual sin duplicar un número menor literal en la prueba de ownership.
  - Verifies the current version without duplicating a literal minor number in the ownership test.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
  - Sincroniza el texto de diagnóstico con la versión vigente.
  - Actualiza la huella revisada del modelo raíz para la codificación 1.12.
  - Conserva archivos golden sólo para versiones históricas; la versión actual se valida directamente mediante escritura, lectura y verificación.
  - Alinea el contrato de accesibilidad efectiva: un tipo `internal` y sus miembros no forman parte de la superficie NSMETA pública.
  - Synchronizes diagnostic text with the current version.
  - Updates the reviewed root-model fingerprint for the 1.12 encoding.
  - Keeps golden files only for historical versions; the current version is validated directly by write, read, and verification.
  - Aligns effective accessibility: an `internal` type and its members are not part of the public NSMETA surface.

## Implementación y motivo / Implementation and rationale

Las pruebas de versión ahora consumen la misma fuente de verdad que el codec. Esto evita que un incremento intencional del formato deje contratos auxiliares en una versión anterior. El rango histórico continúa protegido: cada versión menor anterior debe conservar un archivo binario archivado y legible. La versión menor vigente no se archiva todavía porque no es histórica; cuando se incremente el formato, el propio bucle comenzará a exigir automáticamente su golden.

Version assertions now consume the same source of truth as the codec. This prevents an intentional format increment from leaving auxiliary contracts on the previous version. Historical compatibility remains protected: every earlier readable minor must retain an archived binary fixture. The current minor is not archived yet because it is not historical; after the next format increment, the loop will automatically require its golden.

La huella raíz esperada para NSMETA 1.12 es:

```text
d4b903103cd1b7343a591659eadd889fc78d057f25ccea86092340f919d8ee18
```

El caso de accesibilidad ya no altera metadatos para simular un símbolo ausente. Comprueba el contrato real: el contenedor interno y su campo público no se publican, y los artefactos originales se verifican sin requerirlos.

The accessibility case no longer rewrites metadata to simulate a missing symbol. It checks the real contract: the internal container and its public field are not published, and the original artifacts verify without requiring them.

## Alcance de validación / Validation scope

Por instrucción de integración, este cambio se guardó sin ejecutar pruebas ni baseline. La validación acumulativa se realizará después de cerrar todas las correcciones pendientes.

Per integration instructions, this change was recorded without running tests or a baseline. Cumulative validation will be performed after all pending corrections are closed.

## Commit de referencia / Reference commit

### Título / Title

```text
Sincroniza los contratos de prueba con NSMETA 1.12 / Synchronize test contracts with NSMETA 1.12
```

### Descripción / Description

```text
ES:
- deriva las versiones de toolchain y compatibilidad de las constantes del codec
- actualiza la huella raíz revisada para NSMETA 1.12
- exige golden binario sólo para versiones históricas
- valida que tipos internos no formen parte de la superficie pública

EN:
- derive toolchain and compatibility versions from codec constants
- update the reviewed root fingerprint for NSMETA 1.12
- require binary goldens only for historical versions
- verify that internal types are excluded from the public surface
```
