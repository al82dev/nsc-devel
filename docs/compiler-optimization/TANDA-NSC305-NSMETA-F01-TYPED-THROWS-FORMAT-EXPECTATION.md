# NSMETA-F01: expectativa de formato para typed throws

## Hallazgo

El agregado NS3-T07 completaba 57/58 checks después de corregir el crash LLVM de NeverType. La
aserción restante exigía literalmente `metadata.formatMinor == 38`, aunque el formato canónico
actual es `nativesharp::interop::kMetadataFormatMinor == 40`.

Una instrumentación diagnóstica temporal, retirada antes de esta corrección, confirmó:

- `LibraryMetadataCodec::read` devolvió `true`;
- el archivo leído es NSMETA 1.40; y
- delegate, método, accessor y función libre restauraron exactamente
  `type:ThrowsApi::ThrowsApi.Error`.

Por tanto no existe pérdida de contrato, defecto del codec ni incompatibilidad del artefacto. La
única condición falsa era el literal histórico del test.

## Procedencia y fronteras auditadas

La expectativa 1.38 proviene del commit `1dab2d685c` (`fix(ns3): align T07 with NSMETA 1.38`). El
minor actual 40 fue establecido después por `cc31cdf7a` al añadir la sección de conformances de
traits. Esa evolución no actualizó esta aserción.

El productor obtiene el contrato del delegate desde su sintaxis y los contratos de miembro,
accessor y función libre desde sus símbolos semánticos. El encoder escribe los catálogos de typed
throws desde NSMETA 1.37 y el decoder los restaura en `std::string` propios de cada registro. No se
modifican esas rutas.

## Corrección

La prueba compara contra `nativesharp::interop::kMetadataFormatMinor` y describe el contrato como
`current NSMETA`. Las cuatro comparaciones de claves permanecen estrictas y byte-exactas; no se
relaja ninguna expectativa.

## Validación

- Build focal `nsc_ns3_t07_error_tests`: PASS. Log: `A:\temp\nsmeta-f01-build.log`.
- Ejecución directa: PASS 58/58 checks. Log: `A:\temp\nsmeta-f01-direct.log`.
- CTest único `nsc.ns3.t07.errors`, con NSLink de `A:`: PASS 1/1, exit 0, 5.775 s.
  Log: `A:\temp\nsmeta-f01-ctest.log`.
