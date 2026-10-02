# Tanda baseline 06B — Manifiesto de instalación para SDK Win64

## Problema

La instalación copiaba correctamente `sdk/win64/nativesharp-runtime.nsimports`, pero el generador
de `NativeSharpInstallation.manifest` rechazaba cualquier ruta fuera de `bin/`, `lib/` y
`share/nativesharp/`. El comando terminaba con:

`non-canonical installation file list path 'sdk/win64/nativesharp-runtime.nsimports'`.

## Causa

`InstallationManifest.cpp` no había incorporado el nuevo namespace administrado `sdk/win64/` a
su política de rutas canónicas.

## Cambio

- `sdk/win64/` es ahora una raíz canónica administrada.
- La verificación recorre esa raíz y rechaza archivos residuales no inventariados.
- Stage 64 incluye el archivo `.nsimports` en su paquete de prueba y añade una negativa para un
  `sdk/win64/stale.nsimports` inesperado.

No se autorizó un `sdk/` genérico: la apertura se limita a Win64, que es la única plataforma del
producto actual.

## Archivos

- `src/Driver/InstallationManifest.cpp`: política canónica y raíz administrada.
- `tests/unit/Stage64InstallationManifestTests.cpp`: positiva y negativa del SDK Win64.
- Este documento: causa, alcance y evidencia de la corrección.

## Validación

- Construcción dirigida de `nsc`: correcta; contrato de host estático aprobado.
- Construcción dirigida de `nsc_stage64_installation_manifest_tests`: correcta.
- `nsc.stage64.installation-manifest`: 1/1 aprobado en 0.34 s.
- `cmake --install` contra `A:\native-sharp-3.0-baseline`: código 0.
- Verificación final del árbol instalado: correcta.
- Escritura y verificación de `NativeSharpInstallation.manifest`: correctas.
- Sin baseline completo, sin librerías externas y sin modificación de `C:\native-sharp`.
