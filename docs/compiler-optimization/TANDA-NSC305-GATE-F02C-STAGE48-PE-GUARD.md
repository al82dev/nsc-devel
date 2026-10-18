# Tanda NSC305 GATE-F02C — guard de prerequisite PE en Stage 48.2

## Alcance

Esta hoja corrige exclusivamente la robustez de `testAutomaticProducts` en Stage 48.2. No cambia
producto, codec, NSMETA, ABI, linker, parser ni fixtures. GATE-F02A y GATE-F02B conservan por
separado la generación auténtica de los archivos históricos 1.38 y 1.39.

## Causa

Cuando la emisión de `SharedMeta.dll` falla, las expectativas causales de creación y existencia
se registran correctamente, pero el test continúa. `readBytes` devuelve un vector vacío y el
bloque PE ejecuta `readU32(validDll, 0x3c)` sin comprobar el prerequisite; el resultado es un
acceso fuera de rango y un `SegFault` que impide ejecutar el resto de Stage 48.2.

Los quince mensajes observados antes del crash son cascadas de tres productos no creados
(`Shared`, `GenericShared` y `Static`) y de sus verificaciones dependientes. La corrida final se
ejecutó con el PATH del proceso reconstruido desde Machine+User y resolvió `nslink.exe` por nombre
en `A:\nslink-devel\bin\release\nslink.exe`; por tanto, los fallos de producto que permanecen no se
atribuyen a una ruta absoluta ausente ni se ocultan mediante un fallback dentro del compilador.

## Corrección

El test conserva todas las expectativas anteriores y sólo abandona el bloque final de mutaciones
PE cuando:

- el archivo leído no contiene siquiera el encabezado DOS de 64 bytes; o
- el offset PE no deja disponibles los 24 bytes mínimos usados por las dos mutaciones.

El retorno termina únicamente `testAutomaticProducts`; `main` continúa con las demás familias de
Stage 48.2. En la ruta válida se ejecutan sin cambios los checks `NotADll` y `WrongMachine`.

## Evidencia focal

Se refrescó sólo el PATH del proceso desde Machine+User. `where.exe nslink.exe` resolvió
exactamente `A:\nslink-devel\bin\release\nslink.exe` y CTest recibió `NSC_LINKER=nslink.exe`, sin
ruta absoluta.

- `nsc_stage48_2_library_metadata_tests`: build PASS.
- `nsc`: build PASS sobre el mismo HEAD/worktree.
- CTest `nsc.stage48_2.library-metadata`: el proceso completó las 382 comprobaciones y no sufrió
  `SegFault`; por ello el guard F02C queda probado causalmente.
- Resultado funcional del agregado: 308 PASS y 74 FAIL. Cuatro fallos son exactamente los checks
  de lectura/compatibilidad de los fixtures 1.38 y 1.39 asignados a F02A/F02B. Los otros 70 se
  concentran en emisión de productos y verificaciones dependientes de metadata/consumidores. F02C
  no los corrige ni los declara causados; conserva sus primeros mensajes y permite que el agregado
  los reporte completos en lugar de abortar tras los primeros quince.
- Logs persistentes: `A:\temp\gate-f02c-build-stage48.log`,
  `A:\temp\gate-f02c-build-nsc.log` y `A:\temp\gate-f02c-ctest-stage48.log`.
- `git diff --check` y auditoría diferencial se ejecutan antes del commit aislado.

F02C queda cerrado exclusivamente como robustez del gate. Stage 48.2 no se declara PASS integral:
faltan los fixtures auténticos F02A/F02B y debe diagnosticarse por separado la familia residual de
productos con el PATH correcto.
