# Tanda correctiva G00 — estado incremental dentro del staging

Fecha: 2026-10-01
Estado: terminada, pendiente de baseline completo

## Fallo observado

El enlace incremental escribía correctamente `Producto.exe.nslinc`, pero la siguiente compilación
creaba una transacción con staging vacío. `WindowsExecutableBuilder` buscaba el PE y el sidecar en
ese staging nuevo, nunca encontraba el estado publicado y convertía todas las compilaciones en
miss aunque las entradas fueran idénticas.

## Regla implementada

- Antes de invocar el linker incremental se siembran en el staging el PE y `.nslinc` publicados.
- Si existe información de depuración también se siembra el PDB compañero.
- La ausencia de cualquiera de ellos representa naturalmente el cold path y no es un error.
- La copia usa las mismas rutas extendidas, flush durable y clasificación Win32 que el publicador.
- El linker sigue trabajando sólo dentro del staging; el destino final se reemplaza mediante el
  commit atómico habitual.

## Archivos modificados

- `include/nativesharp/Basic/ArtifactTransaction.h`: publica `seedArtifactStage`.
- `src/Basic/ArtifactTransaction.cpp`: implementa la siembra long-path-safe.
- `src/Driver/Driver.cpp`: prepara PE, sidecar y PDB antes del enlace incremental.

## Mejora obtenida

Una entrada idéntica produce un hit verificable sin renunciar a rollback ni arriesgar el artefacto
publicado. Un cambio de fuente sigue invalidando el estado y vuelve a enlazar.

## Validación específica

- Compilación exclusiva de `nsc_item392_incremental_link_tests`: correcta.
- Incremental-link: 5/5; cold miss, sidecar, warm hit, invalidación y PE actualizado con exit 42.
- Temporales y evidencia dirigidos a `A:` y eliminados al terminar.
- También se retiraron los residuos exactos creados por las ejecuciones dirigidas anteriores.
- No se ejecutó CTest ni baseline.
