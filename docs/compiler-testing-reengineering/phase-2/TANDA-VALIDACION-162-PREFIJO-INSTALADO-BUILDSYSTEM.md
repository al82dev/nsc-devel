# Validacion 162: prefijo instalado para BuildSystem

## SCOPE LOCK

- Causa: `C12B-BLD-005` recibe un arbol de build como si fuera una instalacion.
- Casos afectados: `T12B-00025`, `T12B-00026`, `T12B-00029` y `T12B-00030`.
- Archivo de producto modificado: ninguno.
- Archivo de infraestructura modificado: `nscbuilder3.py`.
- Prohibido: copiar artefactos faltantes desde caches, relajar
  `VerifyInstalledNativeSharp.cmake` o adaptar NSC para aceptar un layout
  incompleto.

## Causa confirmada

`ExecutionEngine._run_buildsystem()` calculaba `installation_root` como el
abuelo del `nsc.exe` seleccionado. Para
`A:\nsc-devel\build-clang\bin\nsc.exe`, esa operacion produce
`A:\nsc-devel\build-clang`.

Ese directorio es un arbol de construccion. No es el prefijo instalado y no
contiene el contrato
`share/nativesharp/toolchain/NativeSharpInstallation.files` que exige
`VerifyInstalledNativeSharp.cmake`. El gate copiaba el arbol equivocado a su
sandbox y fallaba antes de verificar la instalacion.

## Correccion

Builder 3 entrega ahora `BuilderConfig.install_prefix`, que ya es un parametro
explicito, persistido y validado como ruta de `A:`. El gate conserva su copia
aislada y sigue comprobando el layout instalado completo; no se agrego ningun
fallback ni se fabricaron archivos.

## Contrato de validacion

La implementacion se commiteo antes de probar en `f013e83bd`. Despues de ese
commit se ejecutaron puntualmente los cuatro casos afectados mediante la ruta
canonica de Builder 3, cada uno con seleccion `Caso`, modo `Debug` y
optimizacion `NA`.

## Evidencia posterior al commit funcional

- `T12B-00025|Debug|NA`: `PASS`, run `20261010-164623-9c9cda3a`;
  evidencia `cases/T12B-00025-Debug-NA` dentro de esa corrida.
- `T12B-00026|Debug|NA`: `PASS`, run `20261010-164630-bb9048cd`;
  evidencia `cases/T12B-00026-Debug-NA` dentro de esa corrida.
- `T12B-00029|Debug|NA`: `PASS`, run `20261010-164635-3fb17649`;
  evidencia `cases/T12B-00029-Debug-NA` dentro de esa corrida.
- `T12B-00030|Debug|NA`: `PASS`, run `20261010-164639-bfd3d751`;
  evidencia `cases/T12B-00030-Debug-NA` dentro de esa corrida.

Todas las corridas residen bajo `_nscbuilder3/history/<run-id>` y conservan
`metadata.json`, `results.json`, `summary.md` y el directorio de caso indicado.

Cada `results.json` registra estado global `PASSED`, conteo `PASS: 1`, contrato
`C12B-BLD-005` y revision completa
`f013e83bd4c22fc3d13c0aa250a45300416f127a`. Los casos de repeticion
`T12B-00026` y `T12B-00030` conservan en `detail` la cadena descriptiva
`repeated installation verification differs`, pero sus estados de caso y de
corrida son respectivamente `PASS` y `PASSED`; sus `summary.md` registran cero
fallos terminales.

## Estado de cierre dirigido

- Implementacion: terminada en `f013e83bd`.
- Validacion puntual: `4/4 PASS`.
- Corrida de las `439` variantes correctivas: no ejecutada en esta validacion.
- Corrida integral de `5,695` variantes: no ejecutada.
- Producto NSC modificado: no.
- Unidad `C:` modificada: no.
